/* HIO Stub Process 
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>

#include <pet_ioctl.h>
#include <hobbes_util.h>
#include <xemem.h>

#include <hio_ioctl.h>
#include <hio_engine.h>
#include <pisces_lock.h>

#include <libhio.h>
#include <libhio_types.h>
#include <libhio_error_codes.h>

#define STUB_ID     3
#define GETPID      39

char stub_fname[128];
#define OUTPUT_FILE "/tmp/stub.log"
struct hio_engine *engine = NULL;

/*
static void ioctl_event_loop(void) {

    while (1) {
        struct hio_syscall_t syscall_ioctl;

        //printf("\nPoll file %s\n", stub_fname);
        //fprintf(ofp, "\nPoll file %s\n", stub_fname);
        //fflush(stdout);
        //fflush(ofp);

        int ret = pet_ioctl_path(stub_fname, HIO_STUB_SYSCALL_POLL, (void *) &syscall_ioctl);
        if (ret < 0) {
            printf("Error %d when polling syscall from stub %d\n",
                ret, STUB_ID);
            //fprintf(ofp, "Error %d when polling syscall from stub %d\n", ret, STUB_ID);
            continue;
        }

/*
        printf("stub_id %d get syscall: %d (%llu, %llu, %llu, %llu, %llu)",
                syscall_ioctl.stub_id,
                syscall_ioctl.syscall_nr,
                syscall_ioctl.arg0,
                syscall_ioctl.arg1,
                syscall_ioctl.arg2,
                syscall_ioctl.arg3,
                syscall_ioctl.arg4);
*/
        /*fprintf(ofp, "stub_id %d get syscall: %d (%llu, %llu, %llu, %llu, %llu)",
                syscall_ioctl.stub_id,
                syscall_ioctl.syscall_nr,
                syscall_ioctl.arg0,
                syscall_ioctl.arg1,
                syscall_ioctl.arg2,
                syscall_ioctl.arg3,
                syscall_ioctl.arg4);
		*/

        ret = syscall(syscall_ioctl.syscall_nr, 
                syscall_ioctl.arg0,
                syscall_ioctl.arg1, 
                syscall_ioctl.arg2, 
                syscall_ioctl.arg3, 
                syscall_ioctl.arg4);

        //printf(" => ret %d\n", ret);
        //fprintf(ofp, " => ret %d\n", ret);

        {
            struct hio_syscall_ret_t ret_ioctl;
            ret_ioctl.ret_val = ret;
            ret_ioctl.ret_errno = errno;
            ret = pet_ioctl_path(stub_fname, HIO_STUB_SYSCALL_RET, (void *) &ret_ioctl);
            if (ret < 0) {
                printf("ret_ioctl returns %d\n", ret);
                //fprintf(ofp, "ret_ioctl returns %d\n", ret);
            }
        }
    }

}
*/

static void polling_event_loop(void) {
    int ret = 0;
    struct hio_syscall_t syscall_ioctl;
    struct hio_cmd_t *cmd = NULL;

    while (1) {
        if (engine->rb_syscall_prod_idx == engine->rb_syscall_cons_idx) 
            continue;

        pisces_spin_lock(&engine->lock);
        if (engine->rb_syscall_prod_idx == engine->rb_syscall_cons_idx)
            continue;

        memset(&syscall_ioctl, 0, sizeof(struct hio_syscall_t));

        cmd = &(engine->rb[engine->rb_syscall_cons_idx]);
        if (cmd->stub_id != STUB_ID) {
            pisces_spin_unlock(&engine->lock);
            continue;
        }


        //printf("STUB %d: consume syscall index %d (prod index %d)\n", 
        //        STUB_ID,
        //        engine->rb_syscall_cons_idx,
        //        engine->rb_syscall_prod_idx);

        syscall_ioctl.stub_id = cmd->stub_id;
        syscall_ioctl.syscall_nr = cmd->syscall_nr;
        syscall_ioctl.arg0 = cmd->arg0;
        syscall_ioctl.arg1 = cmd->arg1;
        syscall_ioctl.arg2 = cmd->arg2;
        syscall_ioctl.arg3 = cmd->arg3;
        syscall_ioctl.arg4 = cmd->arg4;

        engine->rb_syscall_cons_idx = (engine->rb_syscall_cons_idx + 1) % HIO_RB_SIZE;
        pisces_spin_unlock(&engine->lock);

        // syscall_ioctl
        {
            //printf("stub_id %d get syscall: %d", syscall_ioctl.stub_id, syscall_ioctl.syscall_nr);

            ret = syscall(syscall_ioctl.syscall_nr, 
                    syscall_ioctl.arg0,
                    syscall_ioctl.arg1, 
                    syscall_ioctl.arg2, 
                    syscall_ioctl.arg3, 
                    syscall_ioctl.arg4);

            //printf(" => ret %d\n", ret);
            fprintf(ofp, " => ret %d\n", ret);
        }

        // return
        {
            pisces_spin_lock(&engine->lock);
            if (engine->rb_ret_prod_idx == engine->rb_syscall_cons_idx) {
                printf("No pending syscall, return before syscall??\n");
                continue;
            }
            cmd = &engine->rb[engine->rb_ret_prod_idx];
            cmd->ret_val = ret;
            cmd->ret_errno = errno;
            engine->rb_ret_prod_idx = (engine->rb_ret_prod_idx+1) % HIO_RB_SIZE;
            pisces_spin_unlock(&engine->lock);
        }
    } 
}

int main(int argc, char* argv[])
{
    int ret;
    
    printf("Start stub process...\n");
    //fprintf(ofp, "Start stub process...\n");

    int status = 0;
    printf("    init libhio stub...\n");
    //fprintf(ofp, "    init libhio stub...\n");
    status = libhio_stub_init(&argc, &argv);
    if (status != 0) {
        ERROR("Failed to init libhio stub\n");
        return status;
    }

    printf("    init hobbes client...\n");
    //fprintf(ofp, "    init hobbes client...\n");
    status = hobbes_client_init();
    if (status != 0) {
        ERROR("Failed to init hobbes client\n");
        return status;
    }


    printf("    init xemem mapping...\n");
    //fprintf(ofp, "    init xemem mapping...\n");
    status = libhio_init_xemem_mappings();
    if (status) {
        ERROR("Failed to init xemem mappings\n");
        return status;
    }
    
    // kernel based ringbuffer impelmentation
    /*
    sprintf(stub_fname, "/dev/hio-stub%d", STUB_ID);
    ret = access(stub_fname, F_OK);
    if(ret != 0) {
        printf("    create file %s\n", stub_fname);
        //fprintf(ofp, "    create file %s\n", stub_fname);
        ret = pet_ioctl_path("/dev/hio", HIO_IOCTL_REGISTER , (void *) STUB_ID);
        if (ret != 0) {
            printf("Error %d when registering stub %d. Is hio/mod/linux_user/engine started?\n", ret, STUB_ID);
            //fprintf(ofp, "Error %d when registering stub %d. Is hio/mod/linux_user/engine started?\n", ret, STUB_ID);
            return -1;
        }
    }
    
    ioctl_event_loop();
    */
    {
        xemem_segid_t segid;
        segid = xemem_lookup_segid(HIO_ENGINE_SEG_NAME);
        if (segid == XEMEM_INVALID_SEGID) {
            printf("xemem_lookup_segid failed\n");
            return -1;
        }

        xemem_apid_t apid = xemem_get(segid, XEMEM_RDWR);
        if (apid == -1) {
            printf("Cannot get data segid %li\n", segid);
            return -1;
        }

        struct xemem_addr addr;
        addr.apid = apid;
        addr.offset = 0;
        int *buf;
        buf = xemem_attach(addr, HIO_ENGINE_PAGE_SIZE, NULL);

        printf("Buffer address %p, buffer content %x\n", buf, *buf);

        engine = (struct hio_engine *) buf;

        polling_event_loop();
    }

    libhio_stub_deinit();
    return 0;
}
