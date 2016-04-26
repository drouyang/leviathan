/* 
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 *
 * HIO Engine Kitten User Space Process
 *
 * 1) export a xemem segment useded as the syscall ring buffer
 * 2) polling on the ring buffer
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>
#include <signal.h>
#include <unistd.h>


#include <hio_engine.h>
#include <hio_ioctl.h>
#include <pet_ioctl.h>
#include <hobbes_util.h>
#include <xemem.h>

struct hio_engine *engine = NULL;

static volatile int shouldStop = 0;

void intHandler(int dummy) {
    shouldStop = 1;
}

static void dispachter_loop(void) {
    struct hio_syscall_ret_t cur_ret;

    printf("Enter HIO engine dispather loop...\n");

    if (engine == NULL) {
        printf("Error: engine is NULL\n");
    }

    while (!shouldStop) {
        if (engine->rb_ret_prod_idx != engine->rb_ret_cons_idx) {
            pisces_spin_lock(&engine->lock);

            if (engine->rb_ret_prod_idx == engine->rb_ret_cons_idx)
                continue;

            struct hio_cmd_t *cmd = &(engine->rb[engine->rb_ret_cons_idx]);

            /*
            printf("ENGINE dispatcher: consume cur_ret index %d (prod index %d)\n", 
                    engine->rb_ret_cons_idx,
                    engine->rb_ret_prod_idx);
                    */

            // hio ioctl for return
            cur_ret.stub_id = cmd->stub_id;
            cur_ret.syscall_nr = cmd->syscall_nr;
            cur_ret.ret_val = cmd->ret_val;

            engine->rb_ret_cons_idx = (engine->rb_ret_cons_idx + 1) % HIO_RB_SIZE;
            pisces_spin_unlock(&engine->lock);

            pet_ioctl_path("/dev/hio", HIO_IOCTL_SYSCALL_RET, &cur_ret);
            //printf("Return from kernel with ret %d\n", ret);
        } 
    }
}

int main(int argc, char* argv[])
{
    int ret;
    char hio_fname[128] = "/dev/hio";
    xemem_segid_t segid;
    void *buf;

    signal(SIGINT, intHandler);

    /* check /dev/hio */
    {
        ret = access(hio_fname, F_OK);
        if(ret != 0) {
            printf("Error(%d): %s does not exist\n", ret, hio_fname);
            return -1;
        }

        ret = access(hio_fname, W_OK | R_OK);
        if(ret != 0) {
            printf("Error(%d): accessing %s.\n", ret, hio_fname);
            return -1;
        }
    }


    /* Allocating page aligned memory*/
    {
        printf("Allocating shared ring buffer memory...\n");
        posix_memalign((void **)&buf, HIO_ENGINE_PAGE_SIZE, HIO_ENGINE_PAGE_SIZE);
        if (buf == NULL) {
            printf("memory allocation failed\n");
            return -1;
        }
        memset(buf, 0, HIO_ENGINE_PAGE_SIZE);

        engine = (struct hio_engine *) buf;
        engine->magic = HIO_ENGINE_MAGIC;
        printf("Engine addr %p, content %x\n", buf, engine->magic);
    }

    /* export ringbuffer with XEMEM */
    {
        printf("Exporting ringbuffer %p with XEMEM...\n", buf);
        hobbes_client_init();
        segid = xemem_make(buf, HIO_ENGINE_PAGE_SIZE, HIO_ENGINE_SEG_NAME);
        if (segid == XEMEM_INVALID_SEGID) {
            printf("xemem_make failed\n");
            free(buf);
            return -1;
        }
    }

    /* pass ringbuffer to the Kitten kernel */
    ret = pet_ioctl_path("/dev/hio", HIO_IOCTL_ENGINE_ATTACH , buf);
    if (ret < 0) printf("Error from kernel with ret %d\n", ret);
    
    /* polling returns from ringbuffer */
    dispachter_loop();

    xemem_remove(segid);
    free(buf);

    return 0;
}
