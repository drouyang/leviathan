/*
 * libhio (Hobbes I/O) stub library
 * (c) Brian Kocoloski, 2016
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <pet_log.h>
#include <pet_xml.h>
#include <pet_hashtable.h>

#include <hobbes_util.h>
#include <hobbes_cmd_queue.h>
#include <xemem.h>

#include <libhio.h>
#include <libhio_types.h>
#include <libhio_error_codes.h>


/* 4KB:   Min LWK address */
#define SMARTMAP_ALIGN (1ULL << 39)
#define LWK_BASE_ADDR (1ULL << 12)
#define LWK_MAX_ADDR  SMARTMAP_ALIGN
#define LWK_SIZE      (LWK_MAX_ADDR - LWK_BASE_ADDR)


/* Pipe convenience */
#define P_TO_C(p, rank) p[rank][0]
#define C_TO_P(p, rank) p[rank][1]
#define READ(p)  p[0]
#define WRITE(p) p[1]


#define TO_CHILD(p, rank)    WRITE(P_TO_C(p, rank))
#define TO_PARENT(p, rank)   WRITE(C_TO_P(p, rank))

#define FROM_CHILD(p, rank)  READ(C_TO_P(p, rank))
#define FROM_PARENT(p, rank) READ(P_TO_C(p, rank))


struct hio_region {
    /* Filled in by parent */
    xemem_segid_t segid;
    void        * vaddr;
    uint64_t      size;
    unsigned long offset;

    /* Filled in by children */
    xemem_apid_t  apid;
};

struct hio_rank {
    uint32_t rank_id;
    pid_t    pid;
    bool     exited;

    /* We need to keep track of the HCQ command (if any) each child is
     * processing, so that if the child dies we can return failure to the
     * client
     */
    hcq_cmd_t outstanding_cmd;
};

/* Parent info */
static struct hio_rank  * ranks      = NULL;
static hcq_handle_t       hcq        = HCQ_INVALID_HANDLE;
static struct hashtable * cmd_htable = NULL;

static char     name[64]   = {0};
static bool     term_loop  = 0;
static uint32_t num_ranks  = 0;
static uint32_t num_exited = 0;


/* Child info */
static void   * lwk_reserve = NULL;
static uint32_t rank_id     = (uint32_t)-1;


/* Parent/child info */
/* HIO regions (set by parent / read by children */
static struct hio_region data;
static struct hio_region heap;
static struct hio_region stack;

/* Pipes */
static int  *** pipes = NULL;
static fd_set   parent_read;


/* Command struct for parent<->child processing */
struct hio_cmd {
    /* The size of an instance of this struct */
    uint32_t     cmd_size;

    /* The status of the HIO command */
    int32_t      status;

    /* The identifier for the HCQ command */
    hcq_cmd_t    hcq_cmd;

    /* Variable length xml specifying the stub function */
    char         xml_str[0];
};


static uint32_t
__hcq_hash_fn(uintptr_t key)
{
    return pet_hash_ptr(key);
}

static int
__hcq_eq_fn(uintptr_t key1,
            uintptr_t key2)
{
    return (key1 == key2);
}

static void
__destroy_hcq(void)
{
    hcq_free_queue(hcq);
    hcq = HCQ_INVALID_HANDLE;
}

static int
__create_hcq(void)
{
    hcq = hcq_create_queue(name);
    if (hcq == HCQ_INVALID_HANDLE)
        return -1;

    return 0;
}

/* Process received sigterm */
static void
sigterm_handler(int sig)
{
    term_loop = true;
}

static void
sigsegv_handler(int sig)
{
    if (hcq != HCQ_INVALID_HANDLE)
        __destroy_hcq();

    exit(-1);
}

static int
__install_sig_handlers(void)
{
    /* Catch sigterm to let init_task initiate teardown */
    struct sigaction new_action;

    memset(&new_action, 0, sizeof(struct sigaction));
    new_action.sa_handler = sigterm_handler;
    sigemptyset(&(new_action.sa_mask));

    if (sigaction(SIGTERM, &new_action, NULL) == -1) {
        ERROR("Could not register SIGTERM handler\n");
        return -1;
    }

    /* Catch sigsegv to tear down HCQ */
    memset(&new_action, 0, sizeof(struct sigaction));
    new_action.sa_handler = sigsegv_handler;
    sigemptyset(&(new_action.sa_mask));

    if (sigaction(SIGSEGV, &new_action, NULL) == -1) {
        ERROR("Could not register SIGSEGV handler\n");
        return -1;
    }

    return 0;
}

static int
__update_list(char        * id,
              uint64_t      vaddr,
              uint64_t      size,
              xemem_segid_t segid,
              uint64_t      offset)
{
    struct hio_region * hio_region = NULL; 
    int                 hio_idx    = -1;

    if (strcmp(id, "data") == 0)
        hio_idx = 0;
    else if (strcmp(id, "heap") == 0)
        hio_idx = 1;
    else if (strcmp(id, "stack") == 0)
        hio_idx = 2;

    switch (hio_idx) {
        case 0:
            hio_region = &data;
            break;

        case 1:
            hio_region = &heap;
            break;

        case 2:
            hio_region = &stack;
            break;

        default:
            ERROR("Invalid specification: invalid id (%s) region\n", id);
            return -1;
    }

    hio_region->segid  = segid;
    hio_region->vaddr  = (void *)vaddr;
    hio_region->size   = size;
    hio_region->offset = offset;

    /* To be filled in by children */
    hio_region->apid   = -1;

    printf("[libhio_stub] Region %s segid %lld: vaddr %lld (%p), size %llu, offset %lld\n",
            id, (long long)hio_region->segid, 
            (unsigned long long) hio_region->vaddr, (void *)hio_region->vaddr, 
            (unsigned long long)hio_region->size,  
            (long long)hio_region->offset);

    return 0;
}

static int
__parse_region_specification(pet_xml_t region_xml)
{
    char        * id     = NULL;
    xemem_segid_t segid  = XEMEM_INVALID_SEGID;
    uint64_t      vaddr  = 0;
    uint64_t      size   = 0;
    uint64_t      offset = 0;

    id = pet_xml_get_val(region_xml, "id");
    if (id == NULL) {
        ERROR("Invalid specification: invalid or missing id for region\n");
        return -1;
    }

    vaddr = smart_atou64(0, pet_xml_get_val(region_xml, "vaddr"));
    if (vaddr == 0) { 
        ERROR("Invalid specification: invalid or missing vaddr for region %s\n", id);
        return -1;
    }

    size = smart_atou64(0, pet_xml_get_val(region_xml, "size"));
    if (size == 0) {
        ERROR("Invalid specification: invalid or missing size for region %s\n", id);
        return -1;
    }

    segid = smart_atoi64(XEMEM_INVALID_SEGID, pet_xml_get_val(region_xml, "segid"));
    if (segid == XEMEM_INVALID_SEGID) {
        ERROR("Invalid specification: invalid or missing segid for region %s\n", id);
        return -1;
    }

    offset = smart_atoi64((uint64_t)-1, pet_xml_get_val(region_xml, "offset"));
    if (offset == (uint64_t)-1) {
        ERROR("Invalid specification: invalid or missing offset for region %s\n", id);
        return -1;
    }

    return __update_list(id, vaddr, size, segid, offset);
}

static int
__parse_specification(char * spec_str)
{
    pet_xml_t hio_spec    = PET_INVALID_XML;
    pet_xml_t region      = PET_INVALID_XML;
    uint32_t  num_regions = 0;
    uint32_t  i           = 0;
    int       status      = 0;
    char    * spec_name   = NULL;

    hio_spec = pet_xml_parse_str(spec_str);
    if (hio_spec == PET_INVALID_XML) {
        ERROR("Cannot parse specification\n");
        return -1;
    }

    /* name */
    spec_name = pet_xml_get_val(hio_spec, "name");
    if (spec_name == NULL ){
        ERROR("Invalid specification: invalid or missing name\n");
        goto out;
    }
    strncpy(name, spec_name, 64);

    /* num_ranks */
    num_ranks = smart_atou32(0, pet_xml_get_val(hio_spec, "num_ranks"));
    if (num_ranks == 0) {
        ERROR("Invalid specification: invalid or missing num_ranks\n");
        goto out;
    }

    ranks = malloc(sizeof(struct hio_rank) * num_ranks);
    if (ranks == NULL) {
        ERROR("%s\n", strerror(errno));
        goto out;
    }

    for (i = 0; i < num_ranks; i++) {
        memset(&(ranks[i]), 0, sizeof(struct hio_rank));
        ranks[i].outstanding_cmd = HCQ_INVALID_CMD;
    }

    /* num_regions */
    num_regions = smart_atou32(0, pet_xml_get_val(hio_spec, "num_regions"));
    if (num_regions != 3) {
        ERROR("Invalid specification: invalid or missing num_region (currently must be 3\n");
        goto out2;
    }

    region = pet_xml_get_subtree(hio_spec, "region");
    for (i = 0; i < num_regions; i++) {
        if (region == PET_INVALID_XML) {
            ERROR("Invalid specification: invalid or missing region tag\n");
            goto out2;
        }

        status = __parse_region_specification(region);
        if (status != 0) {
            ERROR("Cannot parse region subtree\n");
            goto out2;
        }

        region = pet_xml_get_next(region);
    }

    pet_xml_free(hio_spec);
    return 0;

out2:
    free(ranks);
out:
    pet_xml_free(hio_spec);
    return -1;
}


int
__libhio_init_xemem_mappings(void)
{
    assert(data.vaddr  >= (void *)LWK_BASE_ADDR);
    assert(heap.vaddr  >= (void *)LWK_BASE_ADDR);
    assert(stack.vaddr >= (void *)LWK_BASE_ADDR);

    /* XEMEM gets */
    {
        data.apid = xemem_get(data.segid, XEMEM_RDWR);
        if (data.apid == -1) {
            printf("Cannot get data segid %li\n", data.segid);
            goto data_get_out;
        }
        //printf("[libhio_stub] Data segid %li, apid %li\n", data.segid, data.apid);

        heap.apid = xemem_get(heap.segid, XEMEM_RDWR);
        if (heap.apid == -1) {
            printf("Cannot get heap segid %li\n", heap.segid);
            goto heap_get_out;
        }
        //printf("[libhio_stub] Heap segid %li, apid %li\n", heap.segid, heap.apid);

        stack.apid = xemem_get(stack.segid, XEMEM_RDWR);
        if (stack.apid == -1) {
            printf("Cannot get stack segid %li\n", stack.segid);
            goto stack_get_out;
        }
        //printf("[libhio_stub] stack segid %li, apid %li\n", stack.segid, stack.apid);
    }

    /* Targeted attachments */
    {
        struct xemem_addr addr;
        void            * va = NULL;

        /* Data */
        {
            addr.apid   = data.apid;
            addr.offset = data.offset * rank_id;

            va = xemem_attach(addr, data.size, data.vaddr);
            if (va == MAP_FAILED) {
                printf("Could not attach to data apid %lld, offset %lld\n",
                        (long long) addr.apid, (long long)addr.offset);
                goto data_attach_out;
            }

            assert(va == data.vaddr);
        }

        /* Heap */
        {
            addr.apid   = heap.apid;
            addr.offset = heap.offset * rank_id;

            va = xemem_attach(addr, heap.size, heap.vaddr);
            if (va == MAP_FAILED) {
                printf("Could not attach to head apid %lld, offset %lld\n",
                        (long long) addr.apid, (long long)addr.offset);
                goto heap_attach_out;
            }

            assert(va == heap.vaddr);
        }

        /* Stack */
        {
            addr.apid   = stack.apid;
            addr.offset = stack.offset * rank_id;

            va = xemem_attach(addr, stack.size, stack.vaddr);
            if (va == MAP_FAILED) {
                printf("Could not attach to stack apid %lld, offset %lld\n",
                        (long long) addr.apid, (long long)addr.offset);
                goto stack_attach_out;
            }

            assert(va == stack.vaddr);
        }
    }

    return 0;

stack_attach_out:
    xemem_detach(heap.vaddr);

heap_attach_out:
    xemem_detach(data.vaddr);

data_attach_out:
    xemem_release(stack.apid);

stack_get_out:
    xemem_release(heap.apid);

heap_get_out:
    xemem_release(data.apid);

data_get_out:
    return -1;
}

/* This is a hack of the rank_id */
int
libhio_init_xemem_mappings(void) {
    rank_id = 0;
    return __libhio_init_xemem_mappings();
}


void
libhio_deinit_xemem_mappings(void)
{
    xemem_detach(stack.vaddr);
    xemem_release(stack.apid);

    xemem_detach(heap.vaddr);
    xemem_release(heap.apid);

    xemem_detach(data.vaddr);
    xemem_release(data.apid);
}

static int
__reserve_lwk_aspace(void)
{
    lwk_reserve = mmap((void *)LWK_BASE_ADDR, 
            LWK_SIZE,
            PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_NORESERVE,
            -1, 0);

    if (lwk_reserve == MAP_FAILED) {
        ERROR("Cannot reserve virtual LWK address space: %s. "
            "Continuing as this is not catastrophic. You may want to check ulimit.\n", 
            strerror(errno));
        return 0;
    } else {
        assert(lwk_reserve == (void *)LWK_BASE_ADDR);
    }

    return 0;
}

static void
__free_lwk_aspace(void)
{
    if (lwk_reserve == (void *)LWK_BASE_ADDR)
        munmap(lwk_reserve, LWK_SIZE);

    lwk_reserve = NULL;
}

static int
__init_hio(char * spec_str)
{
    int status;

    /* First, reserve the LWK address space range so that we don't allocate
     * memory from it locally
     */
    status = __reserve_lwk_aspace();
    if (status != 0)
        return status;

    /* Setup signal handlers */
    status = __install_sig_handlers();
    if (status != 0)
        goto out;

    /* Create command htable */
    cmd_htable = pet_create_htable(0, __hcq_hash_fn, __hcq_eq_fn);
    if (cmd_htable == NULL)
        goto out;
   
    /* Parse spec */
    printf("[libhio_stub] Spec: %s\n", spec_str);
    status = __parse_specification(spec_str);
    if (status != 0) {
        printf("Error parsing stub specification\n");
        printf("%s\n", spec_str);
        goto out2;
    }

    return 0;

out2:
    pet_free_htable(cmd_htable, 0, 0);
    cmd_htable = NULL;
out:
    __free_lwk_aspace();
    return status;
}

static void
usage(char ** argv)
{
    ERROR("Usage: %s: <xml spec string> <%s args ...>\n"
        "Spec format:\n"
        "<hio name=\"name\" num_ranks=\"num ranks\" num_regions=\"num regions\">\n"
        "\t<region id=\"{data/heap/stack}\" vaddr=\"vaddr\" size=\"size\" segid=\"segid\" offset=\"offset\"/>\n"
        "\t...\n"
        "</hio>\n", *argv, *argv);
}

static void
update_args(int    * argc,
            char *** argv)
{
    int i;

    for (i = 1; i < *argc; i++) {
        (*argv)[i] = (*argv)[i + 1];
    }

    (*argc)--;
}

int
libhio_stub_init(int    * argc,
                 char *** argv)
{
    int ret = -1;

    if (*argc < 2) {
        usage(*argv);
        goto out;
    }
    
    ret = __init_hio((*argv)[1]);

out:
    update_args(argc, argv);
    return ret;
}

void
libhio_stub_deinit(void)
{
    pet_free_htable(cmd_htable, 0, 0);
    cmd_htable = NULL;
    free(ranks);
    __free_lwk_aspace();
}

int
libhio_register_cb(uint64_t cmd_code,
                   hio_cb_t stub_cb)
{
    int status;

    /* We internalize callback lookups because the types are incompatible with hcq registration */
    status = pet_htable_insert(cmd_htable, (uintptr_t)cmd_code, (uintptr_t)stub_cb);
    if (status == 0) {
        ERROR("Could not register callback in hashtable (possible duplicate registration\n");
        return -1;
    }

    return 0;
}



#if 0
static int
__loop(void)
{
    struct hio_rank * hio_rank = NULL;

    uint32_t rank   = 0;
    pid_t    pid    = 0;
    int      status = 0;

    while (num_exited < num_ranks) {

        rank = (uint32_t)pet_htable_remove(table, (uintptr_t)pid, 0);
        assert(rank != 0);

        /* Actual rank is offset by 1 */
        rank -= 1;

        if (WIFEXITED(status)) {
            printf("Rank %d (pid %d) exited with status %d\n",
                rank, pid, WEXITSTATUS(status));
            fflush(stdout);
        } else {
            printf("Rank %d (pid %d) exited abnormally\n", rank, pid);
            fflush(stdout);
        }

        hio_rank = &(ranks[rank]);

        assert(hio_rank->pid == pid);
        assert(hio_rank->exited == 0);
        hio_rank->exited = 1;

        ++num_exited;
    }

    printf("All children have exited\n");
    fflush(stdout);

    pet_free_htable(table, 0, 0);
    free(ranks);
    __free_lwk_aspace();

    return 0;
}
#endif

static void
__kill_children(void)
{
    uint32_t i;

    for (i = 0; i <= num_ranks; i++) {
        struct hio_rank * hio_rank = &(ranks[i]);

        if ((hio_rank->pid == 0) || (hio_rank->exited))
            continue;

        /* Closing the pipe fd forces child exit */
        close(TO_CHILD(pipes, i));
        close(FROM_CHILD(pipes, i));

        /* Anyone that's still alive gets SIGKILL'd */
        // kill(hio_rank->pid, SIGKILL);
    }
}

static pid_t
__fork_procs(void)
{
    pid_t    pid    = 0;
    uint32_t i      = 0;

    struct hio_rank * hio_rank = NULL;

    /* Initially, assume all ranks have exited */
    num_exited = num_ranks;

    for (i = 0; i < num_ranks; i++) {
        pid = fork();
        switch(pid) {
            case -1:
                ERROR("Failed to fork: %s\n", strerror(errno));
                __kill_children();
                return -1;

            case 0:
                close(FROM_CHILD(pipes, i));
                close(TO_CHILD(pipes, i));

                free(ranks);
                rank_id = i;
                return 0;

            default:
                close(FROM_PARENT(pipes, i));
                close(TO_PARENT(pipes, i));

                --num_exited;
                break;
        }

        /* Update the pid in the table */
        hio_rank      = &(ranks[i]);
        hio_rank->pid = pid;
    }

    return 1;
}

static int
__format_hio_command(hcq_cmd_t         cmd,
                     int32_t           status,
                     char            * xml_str,
                     struct hio_cmd ** hio_cmd_p)
{

    struct hio_cmd * hio_cmd = NULL;

    size_t bytes   = 0;
    size_t xml_len = 0;

    /* strlen() does not count '\0' */
    xml_len = strlen(xml_str);
    bytes   = sizeof(struct hio_cmd) + xml_len + 1;

    hio_cmd = malloc(bytes);
    if (hio_cmd == NULL) {
        ERROR("Could not malloc hio command structure\n");
        return -HIO_SERVER_ERROR;
    }
    
    hio_cmd->cmd_size = bytes;
    hio_cmd->status   = status;
    hio_cmd->hcq_cmd  = cmd;
    strncpy((char *)hio_cmd->xml_str, xml_str, xml_len + 1);

    *hio_cmd_p = hio_cmd;
    return 0;
}

static int
__write_hio_command(int              fd,
                    struct hio_cmd * hio_cmd)
{
    size_t bytes = hio_cmd->cmd_size;
    size_t total = 0;

    while (total < bytes) {
        ssize_t b_written = 0;
        void  * buf       = (void *)hio_cmd + total;

        b_written = write(fd, buf, bytes - total); 
        switch (b_written) {
            case -1:
                ERROR("Could not write hio_cmd: %s\n", strerror(errno));
                return -HIO_SERVER_ERROR;

            case 0:
                ERROR("Wrote 0 bytes of hio_cmd\n");
                return -HIO_SERVER_ERROR;

            default:
                break;
        }

        total += b_written;
    }

    return 0;
}

static int
__write_hio_command_failure(int              fd,
                            int              status,
                            struct hio_cmd * hio_cmd)
{
    struct hio_cmd resp;

    resp.cmd_size   = sizeof(struct hio_cmd);
    resp.status     = status;
    resp.hcq_cmd    = hio_cmd->hcq_cmd;

    return __write_hio_command(fd, &resp);
}


static int
__read_hio_command(int               fd,
                   struct hio_cmd ** hio_cmd_p)
{
    size_t   total    = 0;
    uint32_t cmd_size = 0;

    struct hio_cmd * hio_cmd;

    /* First, figure out how big the command is */
    while (total < sizeof(cmd_size)) {
        ssize_t b_read = 0;
        void  * buf    = (void *)&cmd_size + total;

        b_read = read(fd, buf, sizeof(cmd_size) - total);
        switch (b_read) {
            case -1:
            case 0:
                /* Assumption is that this means teardown */
//                ERROR("Read 0 bytes from parent\n");
                return -1;

            default:
                break;
        }

        total += b_read;
    }

    hio_cmd = malloc(cmd_size);
    if (hio_cmd == NULL) {
        ERROR("Could not malloc hio command structure\n");
        return -1;
    }
    hio_cmd->cmd_size = cmd_size;

    /* Read cmd in */
    while (total < cmd_size) {
        ssize_t b_read = 0;
        void  * buf    = (void *)hio_cmd + total;

        b_read = read(fd, buf, cmd_size - total); 
        switch (b_read) {
            case -1:
                ERROR("Could not read hio_cmd: %s\n", strerror(errno));
                return -1;

            case 0:
                ERROR("Read 0 bytes of hio_cmd\n");
                return -1;

            default:
                break;
        }

        total += b_read;
    }

    *hio_cmd_p = hio_cmd;
    return 0;
}

static int
__setup_hio_args(pet_xml_t   xml_spec,
                 hio_arg_t * args,
                 uint32_t    argc)
{
    pet_xml_t xml_arg = PET_INVALID_XML;
    char    * xml_val = NULL;
    uint32_t  i       = 0;

    xml_arg = pet_xml_get_subtree(xml_spec, "arg");
    for (i = 0; i < argc; i++) {
        if (xml_arg == PET_INVALID_XML) {
            ERROR("Cannot unpack HIO args: arg subtree not found\n");
            goto out;
        }

        xml_val = pet_xml_get_val(xml_arg, "val");
        if (xml_val == NULL) {
            ERROR("Cannot unpack HIO args: val not found in arg tag\n");
            goto out;
        }

        /* All args are cast to hio_arg_t (intptr_t) */
        args[i] = smart_atoi64(-1, xml_val);
        xml_arg = pet_xml_get_next(xml_arg);
    }

    return 0;

out:
    return -HIO_BAD_XML;
}

static int
__setup_hio_response(pet_xml_t         xml_spec,
                     int               cmd_status,
                     hio_ret_t         cb_ret,
                     struct hio_cmd  * hio_cmd,
                     struct hio_cmd ** hio_resp)
{
    char * xml_str     = NULL;
    char   tmp_ptr[64] = {0};
    int    status      = 0;

    /* Add tag for ret */
    snprintf(tmp_ptr, 64, "%li", (intptr_t)cb_ret);
    status = pet_xml_add_val(xml_spec, "ret", tmp_ptr);
    if (status != 0) {
        ERROR("Could not add ret tag to xml spec\n");
        return -HIO_SERVER_ERROR;
    }

    /* Format the new command */
    xml_str = pet_xml_get_str(xml_spec);
    status = __format_hio_command(hio_cmd->hcq_cmd, cmd_status, xml_str, hio_resp);
    free(xml_str);

    if (status != 0) {
        ERROR("Could not allocate HIO response\n");
        return status;
    }

    return 0;
}

static int
__process_hio_command(struct hio_cmd *  hio_cmd,
                      struct hio_cmd ** hio_resp)
{
    pet_xml_t   xml_spec = PET_INVALID_XML;
    uint32_t    xml_rank = 0;
    uint32_t    cmd_no   = 0;
    uint32_t    argc     = 0;
    hio_arg_t * args     = NULL;
    int         status   = 0;

    hio_cb_t  cb;
    hio_ret_t cb_ret;

    /* This mallocs a new spec str that needs to be freed */
    xml_spec = pet_xml_parse_str(hio_cmd->xml_str);
    if (xml_spec == PET_INVALID_XML) {
        ERROR("Cannot parse XML from spec str\n");
        return -HIO_BAD_XML;
    }

    /* Sanity check rank */
    xml_rank = smart_atou32((uint32_t)-1, pet_xml_get_val(xml_spec, "rank"));
    if (xml_rank != rank_id) {
        ERROR("Rank %u (pid %d) received HIO cmd for rank %u\n", rank_id, getpid(), xml_rank);
        status = -HIO_SERVER_ERROR;
        goto out;
    }

    /* Command */
    cmd_no = smart_atou32((uint32_t)-1, pet_xml_get_val(xml_spec, "cmd"));
    if (cmd_no == (uint32_t)-1) {
        ERROR("Cannot process HIO command: invalid or missing cmd\n");
        status = -HIO_BAD_XML;
        goto out;
    }

    /* Argc */
    argc = smart_atou32((uint32_t)-1, pet_xml_get_val(xml_spec, "argc"));
    if (argc == (uint32_t)-1) {
        ERROR("Cannot process HIO command: invalid or missing argc\n");
        status = -HIO_BAD_XML;
        goto out;
    }

    /* Find cb */
    cb = (hio_cb_t)pet_htable_search(cmd_htable, (uintptr_t)cmd_no);
    if (cb == NULL) {
        ERROR("Cannot process HIO command: cannot find callback for cmd %u\n", cmd_no);
        status = -HIO_NO_STUB_CMD;
        goto out;
    }

    /* Allocate a struct to hold the arg list */
    args = malloc(sizeof(hio_arg_t) * argc);
    if (args == NULL) {
        ERROR("Cannot process HIO command: cannot allocate arg array\n");
        status = -HIO_SERVER_ERROR;
        goto out;
    }

    /* Grab the args */
    status = __setup_hio_args(xml_spec, args, argc);
    if (status != 0) {
        ERROR("Cannot process HIO command: could not setup XML args\n");
        goto out2;
    }
    
    /* Invoke callback */
    status = cb(argc, args, &cb_ret);
    if (status != HIO_SUCCESS) {
        ERROR("Cannot process HIO command: could not invoke callback\n");
        goto out2;
    }

    /* Setup the response structure and segment list */
    status = __setup_hio_response(xml_spec, status, cb_ret, hio_cmd, hio_resp);
    if (status != 0) {
        ERROR("Cannot process HIO command: cannot setup response structure\n");
    }

out2:
    free(args);

out:
    pet_xml_free(xml_spec);
    return status;
}

static int
__child_loop(void)
{
    int status;
    int to_p, from_p;
    
    status = __libhio_init_xemem_mappings();
    if (status) {
        ERROR("Failed to init xemem mappings\n");
        return status;
    }

    from_p = FROM_PARENT(pipes, rank_id);
    to_p   = TO_PARENT(pipes, rank_id);

    /* Wait for stuff from parent */
    while (true) {
        struct hio_cmd * hio_cmd  = NULL;
        struct hio_cmd * hio_resp = NULL;
        fd_set rd_set;

        FD_ZERO(&rd_set);
        FD_SET(from_p, &rd_set);

        status = select(from_p + 1, &rd_set, NULL, NULL, NULL);
        if (status == -1) {
            if (errno == EINTR)
                continue;

            ERROR("select: %s\n", strerror(errno));
            break;
        }

        /* Read from parent */
        status = __read_hio_command(from_p, &hio_cmd);
        if (status != 0)
            break;

        /* Execute the requested function. Some notes:
         *
         * We allocate a new command in the function if the callback is
         * invoked.  This is because the XML changes (we add a return value and
         * segment list to it), and we don't know the size ahead of time
         *
         * status simply tells whether or not we invoked the callback - it says
         * nothing of the callback's return value, which is packed into the xml
         */
        status = __process_hio_command(hio_cmd, &hio_resp);

        if (status == 0) {
            /* Write result back to parent */
            status = __write_hio_command(to_p, hio_resp);
            free(hio_resp);
        } else {
            ERROR("Rank %u (pid %d) could not process hio command\n", rank_id, getpid());

            /* Indicate failure */
            status = __write_hio_command_failure(to_p, status, hio_cmd);
        }

        /* Free command struct now */
        free(hio_cmd);

        if (status != 0) {
            ERROR("Rank %u (pid %d) could not write command result to parent. Exiting...\n", 
                rank_id, getpid());
            break;
        }
    }

    /* Teardown */
    pet_free_htable(cmd_htable, 0, 0);
    cmd_htable = NULL;
    libhio_deinit_xemem_mappings();
    __free_lwk_aspace();

    /* Tell parent you're done */
    close(from_p);
    close(to_p);

    return 0;
}

static int
__parse_cmd_xml(char     * xml_str,
                uint32_t * rank_no)
{
    pet_xml_t xml_spec = PET_INVALID_XML;

    xml_spec = pet_xml_parse_str(xml_str);
    if (xml_spec == PET_INVALID_XML) {
        ERROR("Could not parse XML spec\n");
        return -HIO_BAD_XML;
    }

    *rank_no = smart_atou32((uint32_t)-1, pet_xml_get_val(xml_spec, "rank"));
    pet_xml_free(xml_spec);

    if (*rank_no == (uint32_t)-1) {
        ERROR("Could not parse rank from XML spec\n");
        return -HIO_BAD_XML;
    }

    return 0;
}

static int
__process_hcq_command(void)
{
    hcq_cmd_t cmd      = HCQ_INVALID_CMD;
    uint64_t  cmd_code = 0;

    char   * xml_str   = NULL;
    uint32_t xml_size  = 0;
    int      status    = -1;
    int      fd        = 0;
    uint32_t rank_no   = 0;

    struct hio_rank * hio_rank = NULL;
    struct hio_cmd  * hio_cmd  = NULL;

    /* Get next command */
    cmd = hcq_get_next_cmd(hcq);
    if (cmd == HCQ_INVALID_CMD) {
        ERROR("Received invalid command in HCQ\n");
        return -1;
    }

    /* Sanity check cmd code */
    cmd_code = hcq_get_cmd_code(hcq, cmd);
    if (cmd_code != HIO_CMD_CODE) {
        ERROR("Received cmd_code %lu on HCQ (should be %lu)\n", cmd_code, HIO_CMD_CODE);
        status = -HIO_BAD_SERVER_HCQ;
        goto out;
    }

    /* Read xml str */
    xml_str = (char *)hcq_get_cmd_data(hcq, cmd, &xml_size);
    if (xml_str == NULL) {
        ERROR("Could not read HIO cmd spec\n");
        status = -HIO_BAD_XML;
        goto out;
    }

    /* Parse the xml to get rank_no */
    status = __parse_cmd_xml(xml_str, &rank_no);
    if (status != 0) {
        ERROR("Could not parse rank from HIO cmd spec\n");
        goto out;
    }

    if (rank_no >= num_ranks) {
        ERROR("Rank %u specified to handle HIO command, but only %u ranks created\n",
            rank_no, num_ranks);
        status = -HIO_BAD_RANK;
        goto out;
    }

    /* Ensure the rank is not busy already. Not that we couldn't potentially
     * handle this, but for now err out as the client must be doing some weird
     * stuff
     */
    hio_rank = &(ranks[rank_no]);
    if (hio_rank->outstanding_cmd != HCQ_INVALID_CMD) {
        ERROR("Rank %u specified to handle HIO command, but rank already processing a command\n",
            rank_no);
        status = -HIO_RANK_BUSY;
        goto out;
    }

    /* Format hio command */
    status = __format_hio_command(cmd, -HIO_SERVER_ERROR, xml_str, &hio_cmd);
    if (status != 0) {
        ERROR("Could not format HIO cmd from cmd spec\n");
        goto out;
    }

    /* Write the cmd to the child */
    fd = TO_CHILD(pipes, rank_no);
    status = __write_hio_command(fd, hio_cmd);
    free(hio_cmd);

    if (status != 0) {
        ERROR("Could not write HIO cmd to child\n");
        goto out;
    } 

    /* Remember that this rank is processing this command */
    hio_rank->outstanding_cmd = cmd;
    return 0;

out:
    hcq_cmd_return(hcq, cmd, status, 0, NULL);
    return 0;
}

static void 
__mark_child_exited(uint32_t rank)
{
    /* Child exited */
    struct hio_rank * hio_rank = &(ranks[rank]);

    assert(hio_rank->exited == 0);
    hio_rank->exited = 1;

    ++num_exited;

    printf("Rank %u (pid %d) exited\n", rank, hio_rank->pid);
    fflush(stdout);

    /* Close the pipes for this child */
    close(FROM_CHILD(pipes, rank));
    close(TO_CHILD(pipes, rank));

    /* Remove from parent read set */
    FD_CLR(FROM_CHILD(pipes, rank), &parent_read);
}



/*
        pid = wait(&status);
        if (pid == -1) {
            if (errno == EINTR)
                continue;

            if (errno == ECHILD) {
                ERROR("All children have exited. Force exiting parent loop\n");
                break;
            }

            ERROR("wait: %s\n", strerror(errno));
            continue;
        }
*/


static int
__parent_loop(void)
{
    uint32_t i;
    int status, fds_ready, child_fd, hcq_fd, max_fd = 0;

    /* Create the HCQ */
    status = __create_hcq();
    if (status)
        goto out;

    /* Initialize the read fd set */
    FD_ZERO(&parent_read);

    for (i = 0; i < num_ranks; i++) {
        child_fd = FROM_CHILD(pipes, i);
        if (child_fd > max_fd)
            max_fd = child_fd;

        FD_SET(child_fd, &parent_read);
    }

    hcq_fd = hcq_get_fd(hcq);
    if (hcq_fd > max_fd)
        max_fd = hcq_fd;

    FD_SET(hcq_fd, &parent_read); 

    /* Loop until all children are dead, or we get sigterm'd */
    while (num_exited < num_ranks) {
        fd_set cur_set;
        struct hio_cmd  * hio_cmd  = NULL;
        struct hio_rank * hio_rank = NULL;

        /* Check for sigterm */
        if (term_loop)
            break;

        /* Wait for events from the hcq or any of the read fds */
        cur_set = parent_read;
        fds_ready = select(max_fd + 1, &cur_set, NULL, NULL, NULL);
        if (fds_ready == -1) {
            if (errno == EINTR)
                continue;

            ERROR("select: %s\n", strerror(errno));
            break;
        }

        /* See if HCQ is ready */
        if (FD_ISSET(hcq_fd, &cur_set)) {
            status = __process_hcq_command();

            /* The only time status is non-zero is if we couldn't fetch a command
             * from the HCQ. That would signify something catastrophic so we 
             * go down
             */
            if (status != 0) {
                ERROR("Could not process HCQ command\n");
                break;
            }

            --fds_ready;
        }

        /* Process child events */
        for (i = 0; (fds_ready > 0) && (i < num_ranks); i++) {
            child_fd = FROM_CHILD(pipes, i);
            if (FD_ISSET(child_fd, &cur_set)) {

                hio_rank = &(ranks[i]);
    
                /* Read command */
                status = __read_hio_command(child_fd, &hio_cmd);
                if (status != 0) {
                    /* Maybe the child exited. Even if it didn't this is a bad error,
                     * and marking it as exited will close it's pipe and trigger it
                     * to exit
                     */
                    __mark_child_exited(i);

                    /* If there's an outstanding command, return error now */
                    if (hio_rank->outstanding_cmd != HCQ_INVALID_CMD) {
                        hcq_cmd_return(
                            hcq, 
                            hio_rank->outstanding_cmd,
                            -HIO_SERVER_ERROR,
                            0,
                            NULL
                        );
                    }
                } else {
                    hcq_cmd_return(
                        hcq, 
                        hio_cmd->hcq_cmd, 
                        hio_cmd->status, 
                        hio_cmd->cmd_size - sizeof(struct hio_cmd),
                        hio_cmd->xml_str
                    );

                    free(hio_cmd);
                }

                /* No more outstanding command in child */
                hio_rank->outstanding_cmd = HCQ_INVALID_CMD;

                --fds_ready;
            }
        }
    }

out:
    __kill_children();
    __destroy_hcq();

    return 0;
}

static void
__free_pipes(void)
{
    uint32_t i;

    for (i = 0; i < num_ranks; i++) {
        free(pipes[i][0]);
        free(pipes[i][1]);
        free(pipes[i]);
    }
    
    free(pipes);
}

static int
__create_pipes(void)
{
    uint32_t i;

    pipes = malloc(sizeof(int **) * num_ranks);
    if (pipes == NULL) {
        ERROR("Could not create inter-process pipes\n");
        return -1;
    }

    /* Allocate */
    for (i = 0; i < num_ranks; i++) {
        pipes[i] = malloc(sizeof(int *) * 2);
        if (pipes[i] == NULL) {
            ERROR("Could not create inter-process pipes\n");
            goto out_pipes;
        }

        pipes[i][0] = malloc(sizeof(int) * 2);
        if (pipes[i][0] == NULL) {
            ERROR("Could not create inter-process pipes\n");
            free(pipes[i]);
            goto out_pipes;
        }

        pipes[i][1] = malloc(sizeof(int) * 2);
        if (pipes[i][1] == NULL) {
            ERROR("Could not create inter-process pipes\n");
            free(pipes[i][0]);
            free(pipes[i]);
            goto out_pipes;
        }

        /* Pipe them */
        pipe(P_TO_C(pipes, i));
        pipe(C_TO_P(pipes, i));
    }

    return 0;

out_pipes:
    {
        uint32_t j;
        for (j = 0; j < i; j++) {
            free(pipes[j][0]);
            free(pipes[j][1]);
            free(pipes[j]);
        }

        free(pipes);
    }

    return -1;
}

int
libhio_event_loop(void)
{
    int status;

    /* First, allocate the pipes */
    status = __create_pipes();
    if (status)
        return status;

    /* Fork off the procs */
    status = __fork_procs();
    switch (status) {
        case -1:
            ERROR("Could not fork off HIO stub processes\n");
            break;
            
        case 0:
            /* Child execution */

            /* Setup hobbes */ 
            hobbes_client_init();

            status = __child_loop();
            __free_pipes();

            exit(status);

        default:
            /* Parent goes into a wait loop */
            status = __parent_loop();
            __free_pipes();

            return status;
    }

    return -1;
}
