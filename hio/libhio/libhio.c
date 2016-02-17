#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <pet_log.h>
#include <pet_hashtable.h>
#include <pet_xml.h>

#include <hobbes_util.h>
#include <xemem.h>

#include "libhio.h"


/* 1 PML entry */
#define SMARTMAP_ALIGN (1ULL << 39)


/* 4KB:   Min LWK address */
#define LWK_BASE_ADDR (1ULL << 12)

/* 512GB: Max LWK address is the end of the first PML entry */
#define LWK_MAX_ADDR  SMARTMAP_ALIGN


#define LWK_SIZE      (LWK_MAX_ADDR - LWK_BASE_ADDR)


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
};

/* Parent info */
static struct hio_rank  * hio_ranks = NULL;
static struct hashtable * hio_table = NULL; 
static uint32_t num_ranks   = 0;
static uint32_t num_exited  = 0;
static bool     term_loop   = 0;

/* Child info */
static void   * lwk_reserve = NULL;
static uint32_t rank_id     = (uint32_t)-1;

/* HIO regions (set by parent / read by children */
static struct hio_region data;
static struct hio_region heap;
static struct hio_region stack;


/* Reverse hashtable mapping pid to rank id */
static uint32_t
htable_hash_fn(uintptr_t key)
{
    return pet_hash_ptr(key);
}

static int
htable_eq_fn(uintptr_t key1,
             uintptr_t key2)
{
    return (key1 == key2);
}

/* Process received sigterm */
static void
sigterm_handler(int sig)
{
    term_loop = true;
}

static void
__hio_kill_children(void)
{
    uint32_t i;
    struct hio_rank * hio_rank = NULL;

    for (i = 0; i <= num_ranks; i++) {
        hio_rank = &(hio_ranks[i]);

        if ((hio_rank->pid == 0) || (hio_rank->exited))
            continue;

        /* Kill with SIGKILL */
        kill(hio_rank->pid, SIGKILL);
    }
}

static int
__hio_install_sig_handlers(void)
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

    return 0;
}

static int
__hio_update_table(char        * id,
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

    return 0;
}

static int
__hio_parse_region_specification(pet_xml_t region_xml)
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

    return __hio_update_table(id, vaddr, size, segid, offset);
}

static int
__hio_parse_specification(char * spec_str)
{
    pet_xml_t hio_spec    = PET_INVALID_XML;
    pet_xml_t region      = PET_INVALID_XML;
    uint32_t  num_regions = 0;
    uint32_t  i           = 0;
    int       status      = 0;

    hio_spec = pet_xml_parse_str(spec_str);
    if (hio_spec == PET_INVALID_XML) {
        ERROR("Cannot parse specification\n");
        return -1;
    }

    /* num_ranks */
    num_ranks = smart_atou32(0, pet_xml_get_val(hio_spec, "num_ranks"));
    if (num_ranks == 0) {
        ERROR("Invalid specification: invalid or missing num_ranks\n");
        goto out;
    }

    hio_ranks = malloc(sizeof(struct hio_rank) * num_ranks);
    if (hio_ranks == NULL) {
        ERROR("%s\n", strerror(errno));
        goto out;
    }
    memset(hio_ranks, 0, sizeof(struct hio_rank) * num_ranks);

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

        status = __hio_parse_region_specification(region);
        if (status != 0) {
            ERROR("Cannot parse region subtree\n");
            goto out2;
        }

        region = pet_xml_get_next(region);
    }

    pet_xml_free(hio_spec);
    return 0;

out2:
    free(hio_ranks);
out:
    pet_xml_free(hio_spec);
    return -1;
}


static int
__hio_init_xemem_mappings(void)
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

        heap.apid = xemem_get(heap.segid, XEMEM_RDWR);
        if (heap.apid == -1) {
            printf("Cannot get heap segid %li\n", heap.segid);
            goto heap_get_out;
        }

        stack.apid = xemem_get(stack.segid, XEMEM_RDWR);
        if (stack.apid == -1) {
            printf("Cannot get stack segid %li\n", stack.segid);
            goto stack_get_out;
        }
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
                printf("Could not attach to data apid\n");
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
                printf("Could not attach to heap apid\n");
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
                printf("Could not attach to stack apid\n");
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

static int
__hio_reserve_lwk_aspace(void)
{
    lwk_reserve = mmap((void *)LWK_BASE_ADDR, 
            LWK_SIZE,
            PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_NORESERVE,
            -1, 0);

    if (lwk_reserve == MAP_FAILED) {
        ERROR("Cannot reserve virtual LWK address space: %s\n", strerror(errno));
        return -1;
    }

    assert(lwk_reserve == (void *)LWK_BASE_ADDR);
    return 0;
}

static void
__hio_free_lwk_aspace(void)
{
    munmap(lwk_reserve, LWK_SIZE);
}

static int
__libhio_loop(void)
{
    struct hio_rank * hio_rank = NULL;

    uint32_t rank   = 0;
    pid_t    pid    = 0;
    int      status = 0;

    while (num_exited < num_ranks) {
        if (term_loop)
            __hio_kill_children();

        pid = wait(&status);
        if (pid == -1) {
            if (errno == EINTR)
                continue;

            if (errno == ECHILD) {
                ERROR("All children have exited. Force exiting libhio parent loop\n");
                break;
            }

            ERROR("wait: %s\n", strerror(errno));
            continue;
        }

        rank = (uint32_t)pet_htable_remove(hio_table, (uintptr_t)pid, 0);
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

        hio_rank = &(hio_ranks[rank]);

        assert(hio_rank->pid == pid);
        assert(hio_rank->exited == 0);
        hio_rank->exited = 1;

        ++num_exited;
    }

    printf("All children have exited\n");
    fflush(stdout);

    pet_free_htable(hio_table, 0, 0);
    free(hio_ranks);
    __hio_free_lwk_aspace();

    return 0;
}

static pid_t
__hio_fork_procs(void)
{
    pid_t    pid    = 0;
    int      status = 0;
    uint32_t i      = 0;

    struct hio_rank * hio_rank = NULL;

    /* Initially, assume all ranks have exited */
    num_exited = num_ranks;

    for (i = 0; i < num_ranks; i++) {
        pid = fork();
        switch(pid) {
            case -1:
                ERROR("Failed to fork: %s\n", strerror(errno));
                return -1;

            case 0:
                pet_free_htable(hio_table, 0, 0);
                free(hio_ranks);
                rank_id = i;
                return 0;

            default:
                --num_exited;
                break;
        }

        /* Update the pid in the table */
        hio_rank      = &(hio_ranks[i]);
        hio_rank->pid = pid;

        /* Store the child's pid (offset by 1, so no 0 value) into the
         * hashtable */
        status = pet_htable_insert(hio_table,
                    (uintptr_t)pid,
                    (uintptr_t)i + 1);

        if (status == 0) {
            ERROR("Cannot add process pid to hashtable\n");
            goto out;
        }
    }

    return 1;

out:
    __hio_kill_children();

    return -1;
}

static int
__init_hio(char * spec_str)
{
    int status;

    /* First, reserve the LWK address space range so that we don't allocate
     * memory from it locally
     */
    status = __hio_reserve_lwk_aspace();
    if (status != 0)
        return status;

    /* Setup signal handlers */
    status = __hio_install_sig_handlers();
    if (status != 0)
        goto out;

    /* Parse spec */
    status = __hio_parse_specification(spec_str);
    if (status != 0)
        goto out;

    /* Init htable */
    hio_table = pet_create_htable(0, htable_hash_fn, htable_eq_fn);
    if (hio_table == NULL) {
        ERROR("Cannot create hashtable\n");
        goto out2;
    }

    /* Fork off the procs */
    status = __hio_fork_procs();
    switch (status) {
        case -1:
            ERROR("Could not fork off HIO stub processes\n");
            break;
            
        case 0:
            /* Children init xemem mappings and return */
            status = __hio_init_xemem_mappings();
            goto out_child;

        default:
            /* Parent goes into a wait loop */
            exit(__libhio_loop());
    }

    if (status) pet_free_htable(hio_table, 0, 0);
out2:
    if (status) free(hio_ranks);
out_child:
out:
    if (status) __hio_free_lwk_aspace();

    return status;
}

static void
usage(char ** argv)
{
    ERROR("Usage: %s: <xml spec string> <%s args ...>\n"
        "Spec format:\n"
        "<hio num_ranks\"num ranks\" num_regions=\"num regions\">\n"
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
libhio_init(int    * argc,
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
libhio_deinit(void)
{
    xemem_detach(stack.vaddr);
    xemem_release(stack.apid);

    xemem_detach(heap.vaddr);
    xemem_release(heap.apid);

    xemem_detach(data.vaddr);
    xemem_release(data.apid);

    __hio_free_lwk_aspace();
}
