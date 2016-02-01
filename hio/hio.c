#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include <xemem.h>


extern int errno;

/* 1 PML entry */
#define SMARTMAP_ALIGN (1ULL << 39)


/* 4KB:   Min LWK address */
#define LWK_BASE_ADDR (1ULL << 12)

/* 512GB: Max LWK address is the end of the first PML entry */
#define LWK_MAX_ADDR  SMARTMAP_ALIGN


#define LWK_SIZE      (LWK_MAX_ADDR - LWK_BASE_ADDR)


static xemem_segid_t data_segid  = XEMEM_INVALID_SEGID;
static xemem_segid_t heap_segid  = XEMEM_INVALID_SEGID;
static xemem_segid_t stack_segid = XEMEM_INVALID_SEGID;

static xemem_apid_t  data_apid   = -1;
static xemem_apid_t  heap_apid   = -1;
static xemem_apid_t  stack_apid  = -1;

static void * data_va  = NULL;
static void * heap_va  = NULL;
static void * stack_va = NULL;

static unsigned long data_size  = 0;
static unsigned long heap_size  = 0;
static unsigned long stack_size = 0;


static void * lwk = NULL;


static void 
__exit_hio(void)
{
    xemem_detach(stack_va);
    xemem_release(stack_apid);

    xemem_detach(heap_va);
    xemem_release(heap_apid);

    xemem_detach(data_va);
    xemem_release(data_apid);

    munmap(lwk, LWK_SIZE);

    printf("HIO stub exiting\n");
    fflush(stdout);
}

static void
sigterm_handler(int signum)
{
    __exit_hio();
}

static int
__init_hio(char ** argv)
{
    data_va  = (void *)(unsigned long)strtoul(argv[1], NULL, 16);
    heap_va  = (void *)(unsigned long)strtoul(argv[4], NULL, 16);
    stack_va = (void *)(unsigned long)strtoul(argv[7], NULL, 16);

    data_size  = strtoul(argv[2], NULL, 16);
    heap_size  = strtoul(argv[5], NULL, 16);
    stack_size = strtoul(argv[8], NULL, 16);

    data_segid  = strtoll(argv[3], NULL, 10);
    heap_segid  = strtoll(argv[6], NULL, 10);
    stack_segid = strtoll(argv[9], NULL, 10);

    assert(data_va >= (void *)LWK_BASE_ADDR);
    assert(heap_va >= (void *)LWK_BASE_ADDR);
    assert(stack_va >= (void *)LWK_BASE_ADDR);


    /* First, reserve the entire LWK address space so that we do not allocate memory from it */
    lwk = mmap((void *)LWK_BASE_ADDR, 
            LWK_SIZE,
            PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_NORESERVE,
            -1, 0);

    if (lwk == MAP_FAILED) {
        printf("Cannot reserve virtual LWK address space: %s\n", strerror(errno));
        return -1;
    }

    assert(lwk == (void *)LWK_BASE_ADDR);

    /* XEMEM gets */
    {
        data_apid = xemem_get(data_segid, XEMEM_RDWR);
        if (data_apid == -1) {
            printf("Cannot get data segid %lli\n", data_segid);
            goto data_get_out;
        }

        heap_apid = xemem_get(heap_segid, XEMEM_RDWR);
        if (heap_apid == -1) {
            printf("Cannot get heap segid %lli\n", heap_segid);
            goto heap_get_out;
        }

        stack_apid = xemem_get(stack_segid, XEMEM_RDWR);
        if (stack_apid == -1) {
            printf("Cannot get stack segid %lli\n", stack_segid);
            goto stack_get_out;
        }
    }

    /* Targeted attachments */
    {
        struct xemem_addr addr;
        void            * va = NULL;

        /* Data */
        {
            addr.apid   = data_apid;
            addr.offset = 0;

            va = xemem_attach(addr, data_size, data_va);
            if (va == MAP_FAILED) {
                printf("Could not attach to data apid\n");
                goto data_attach_out;
            }

            assert(va == data_va);
            printf("data va = %p, target_va = %p\n", va, data_va);
        }

        /* Heap */
        {
            addr.apid   = heap_apid;
            addr.offset = 0;

            va = xemem_attach(addr, heap_size, heap_va);
            if (va == MAP_FAILED) {
                printf("Could not attach to heap apid\n");
                goto heap_attach_out;
            }

            assert(va == heap_va);
            printf("heap va = %p, target_va = %p\n", va, heap_va);
        }

        /* Stack */
        {
            addr.apid   = stack_apid;
            addr.offset = 0;

            va = xemem_attach(addr, stack_size, stack_va);
            if (va == MAP_FAILED) {
                printf("Could not attach to stack apid\n");
                goto stack_attach_out;
            }

            assert(va == stack_va);
            printf("stack va = %p, target_va = %p\n", va, stack_va);
        }
    }

    /* Catch sigterm */
    {
        struct sigaction new_action, old_action;

        new_action.sa_handler = sigterm_handler;
        sigemptyset(&(new_action.sa_mask));

        sigaction(SIGTERM, NULL, &old_action); 
        if (old_action.sa_handler != SIG_IGN)
            sigaction(SIGTERM, &new_action, NULL);
    }

    return 0;

stack_attach_out:
    xemem_detach(heap_va);

heap_attach_out:
    xemem_detach(data_va);

data_attach_out:
    xemem_release(stack_apid);

stack_get_out:
    xemem_release(heap_apid);

heap_get_out:
    xemem_release(data_apid);

data_get_out:
    munmap(lwk, LWK_SIZE);
    return -1;
}

static void
usage(char ** argv)
{
    printf("Usage: %s: <data va (hex)> <data size (hex)> <data segid>"
            " <heap va (hex)> <heap size (hex)> <heap segid>"
            " <stack va (hex)> <stack size (hex)> <stack segid> <app args ...>\n",
            *argv);
}

int 
main(int     argc,
     char ** argv)
{
    if (argc < 10) {
        usage(argv);
        fflush(stdout);
        return -1;
    }

    if (__init_hio(argv) != 0) {
        printf("Failed to init HIO app\n");
        fflush(stdout);
        return -1;
    }

    /* Sanity check that this doesn't cause segfault/xemem bus error */
    memset(data_va, 0, data_size);
    memset(heap_va, 0, heap_size);
    memset(stack_va, 0, stack_size);

    printf("HIO stub main\n");
    fflush(stdout);

    while (1) sleep (1);

    __exit_hio();

    return 0;
}
