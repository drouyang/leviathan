#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <errno.h>

#include <lwk/aspace.h>
#include <lwk/pmem.h>

#include <xemem.h>
#include <libhio.h>

#define PAGE_SIZE 4096

/* Use this format if each rank specifies its own HIO calls */
/*             fn name,   cmd,        ret, params ...       */
LIBHIO_CLIENT2(hio_open,   __NR_open,   int, const char *, int);
LIBHIO_CLIENT1(hio_close,  __NR_close,  int, int);
LIBHIO_CLIENT3(hio_read,   __NR_read,   ssize_t, int, void *, size_t);
LIBHIO_CLIENT3(hio_write,  __NR_write,  ssize_t, int, const void *, size_t);
LIBHIO_CLIENT7(hio_mmap,   __NR_mmap,   void *, void *, size_t, int, int, int, off_t, hio_segment_t *);
LIBHIO_CLIENT3(hio_munmap, __NR_munmap, int, void *, size_t, hio_segment_t *);

extern int hio_status;

/* Use this format if a single process will issue HIO calls for all ranks in
 * the app You must specify rank number in the 1st argument to the call
 */
/*                 fn name,      cmd,       rank_no,  ret, params ...        */
//LIBHIO_CLIENT2_APP(hio_open_app, __NR_open, uint32_t, int, const char *, int);


/* Mapping a segment is a four step process:
 * (1) xemem_get/attach the seg into our aspace
 * (2) invoke aspace_virt_to_phys on the attached region to generate a page
 * frame list 
 * (3) invoke aspace_map_region on the target region in the target
 * aspace
 * (4) detach the xemem attachment (hold onto the apid)
 */
static int
__map_hio_segment(hio_segment_t * seg,
                  id_t            aspace_id)
{
    xemem_apid_t apid;
    void * local_attach;
    uint32_t nr_pages, page_size, i, j;
    int status;

    if (aspace_id == MY_ID)
        aspace_get_myid(&aspace_id);

    /* (1) xemem get/attach */
    {
        struct xemem_addr addr;

        apid = xemem_get(seg->segid, XEMEM_RDWR);
        if (apid == -1) {
            printf("Could not get xemem segid %li\n", seg->segid);
            return -1;
        }

        addr.apid   = apid;
        addr.offset = 0;

        local_attach = xemem_attach(addr, seg->size, NULL);
        if (local_attach == MAP_FAILED) {
            printf("Could not attach xemem apid %li (%s)\n", addr.apid, strerror(errno));
            goto out_attach;
        }
    }

    /* (2) figure out the pfns and (3) map them to the target aspace */
    {
        vaddr_t local_vaddr, target_vaddr;
        paddr_t paddr;
        struct pmem_region region;

        page_size = seg->page_size;
        nr_pages  = seg->size / seg->page_size;

        for (i = 0; i < nr_pages; i++) {
            local_vaddr  = (addr_t)local_attach + (seg->page_size * i);
            target_vaddr = (addr_t)seg->vaddr   + (seg->page_size * i);

            /* (2) */
            status = aspace_virt_to_phys(MY_ID, local_vaddr, &paddr);
            if (status != 0) {
                printf("aspace_virt_to_phys failed (%s)\n", strerror(errno));
                goto out_virt_to_phys;
            }

            /* Temporary hack: add umem so we can use aspace_map_region below.
             * (the kernel won't let us map non-umem memory)
             */
            {
                memset(&region, 0, sizeof(struct pmem_region));

                region.start            = paddr;
                region.end              = paddr + seg->page_size;
                region.type_is_set      = true;
                region.type             = PMEM_TYPE_UMEM;
                region.allocated_is_set = true;
                region.allocated        = true;

                status = pmem_add(&region);
                if (status != 0) {
                    printf("pmem_add failed (%s)\n", strerror(errno));
                    goto out_umem;
                }
            }

            /* (3) */
            status = aspace_map_region(
                    aspace_id, 
                    target_vaddr,
                    seg->page_size,
                    VM_READ | VM_WRITE | VM_USER,
                    seg->page_size,
                    "hio",
                    paddr
                );

            if (status != 0) {
                printf("aspace_map_region failed (%d) (%s)\n",
                    status, strerror(errno));
                goto out_map_pmem;
            }

            /* Remove umem now. Unclear how to do it later */
            pmem_free_umem(&region);
            pmem_del(&region);
        }
    }

    /* (4) teardown local mapping */
    xemem_detach(local_attach);

    return 0;

out_map_pmem:
out_umem:
out_virt_to_phys:
    for (j = 0; j < i; j++) {
        aspace_unmap_region(
            aspace_id, 
            (addr_t)seg->vaddr + (j * seg->page_size), 
            seg->page_size
        );
    }

    xemem_detach(local_attach);

out_attach:
    xemem_release(apid);
    return -1;
}

int 
main(int     argc,
     char ** argv) 
{
    char * pmi_rank = getenv("PMI_RANK");
    char * hio_name = getenv("STUB_NAME");
    int    rank = -1;
    int    status;

    if (argc != 2) {
        printf("Usage: %s <file to open>\n", *argv);
        return -1;
    }

    if (pmi_rank == NULL) {
        printf("No PMI_RANK in env. assuming rank 0\n");
        rank = 0;
    } else {
        rank = atoi(pmi_rank);
    }

    if (hio_name == NULL) {
        printf("No STUB_NAME in env. exiting\n");
        return -1;
    }

    sleep(2);

    status = libhio_client_init(hio_name, rank);
    if (status != 0) { 
        printf("Failed to init HIO client\n");
        return -1;
    }

    /* Open/read/write/close a file */
    {
        char * file_name = argv[1];
        int fd;
        ssize_t bytes;
        off_t offset;
        void * buf[PAGE_SIZE * 10];

        fd = hio_open(file_name, O_RDONLY);
        if (hio_status != HIO_SUCCESS || fd < 0) { 
            printf("Could not open file %s (fd=%d)\n", file_name, fd);
            goto out;
        }

        bytes = hio_read(fd, buf, PAGE_SIZE * 10);
        printf("Read %li bytes from fd %d\n", bytes, fd);
        if (bytes> 0)
            printf("%s\n", buf);

        hio_close(fd);

        fd = hio_open(file_name, O_WRONLY | O_TRUNC);
        if (hio_status != HIO_SUCCESS || fd < 0)  {
            printf("Could not open file %s (fd=%d)\n", file_name, fd);
            goto out;
        }

        bytes = hio_write(fd, "Overwrite file\n", 15);
        printf("Wrote %li bytes to fd %d\n", bytes, fd);

        hio_close(fd);
    }

    /* mmap the file */
    {
        char * file_name = argv[1];
        int fd;
        ssize_t bytes;
        off_t offset;
        void * addr;
        void * at_addr;
        hio_segment_t seg;

        fd = hio_open(file_name, O_RDONLY);
        if (hio_status != HIO_SUCCESS || fd < 0) { 
            printf("Could not open file %s (fd=%d)\n", file_name, fd);
            goto out;
        }

        addr = hio_mmap(NULL, PAGE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0, &seg);
        if (hio_status != HIO_SUCCESS || addr == MAP_FAILED) {
            printf("failed to issue mmap to fd: %d\n", fd);
            goto out;
        }

        /* Linux mapped new memory to'addr'. We do not have this region
         * mapped in our aspace, so we need to map it in now
         */
        status = __map_hio_segment(&seg, MY_ID);
        if (status != 0) {
            printf("Failed to target the mmap to the stub-allocated vaddr\n");
            goto out;
        }

        addr = seg.vaddr;

        printf("at %p: %s\n", addr, (char *)addr);

        hio_close(fd);
    }

out:
    libhio_client_deinit();

    return 0;
} 
