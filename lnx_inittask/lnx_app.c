/* Linux application control
 * (c) 2015, Jack Lange, <jacklange@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>


#include <stdint.h>

#include <pet_log.h>
#include <pet_xml.h>
#include <pet_hashtable.h>

#include <hobbes.h>
#include <hobbes_app.h>
#include <hobbes_db.h>
#include <hobbes_util.h>
#include <hobbes_notifier.h>

extern hdb_db_t hobbes_master_db;


#include "init.h"
#include "lnx_app.h"

#define DEFAULT_ENVP            ""
#define DEFAULT_ARGV            ""


struct app_state {
    /* app id */
    hobbes_id_t      hpid;

    /* local process id */
    pid_t            pid;

    /* stdin/out fds */
    int              stdin_fd;
    int              stdout_fd;
};

/* Hashtable of apps hosted by this enclave. Indexed by app id (hpid) */
static uint32_t
app_hash_fn(uintptr_t key)
{
    return pet_hash_ptr(key);
}

static int
app_eq_fn(uintptr_t key1,
	  uintptr_t key2)
{
    return (key1 == key2);
}

static struct hashtable * app_htable = NULL;




static int
__handle_stdout(int    fd, 
		void * priv_data)
{
    struct app_state * app = (struct app_state *)priv_data;
    char     out_buf[1024] = {[0 ... 1023] = 0};
    ssize_t  bytes_read    = 0;
    

 
    while (1) {
	bytes_read = read(app->stdout_fd, out_buf, 1023);
    
	if (bytes_read <= 0) {
	    if ((bytes_read == -1) &&
		(errno == EWOULDBLOCK)) {
		break;
	    }
		
	    printf("Process has exited\n");
	    remove_fd_handler(fd);
	    pet_htable_remove(app_htable, (uintptr_t)app->hpid, 0);

	    hobbes_set_app_state(app->hpid, APP_STOPPED);
	    hnotif_signal(HNOTIF_EVT_APPLICATION);

	    free(app);
	    break;
	}

	printf("%s", out_buf);
	memset(out_buf, 0, 1024);
    }
    
    return 0;   
}

/* 
 * Internal implementation of popen/pclose, based on version from: 
 * http://stackoverflow.com/questions/26852198/getting-the-pid-from-popen
 */

static int 
__popen(char  * cmd_line,
	pid_t * assigned_pid,
	int   * stdin_fd,
	int   * stdout_fd)
{
    pid_t child_pid     = 0;
    int   stdout_fds[2] = {0, 0}; /* stdout from the child */
    int   stdin_fds[2]  = {0, 0}; /* stdin to the child */

    pipe2(stdin_fds,  0);
    pipe2(stdout_fds, O_NONBLOCK);

    child_pid = fork();

    if (child_pid == -1) {
        perror("fork");
	return -1;
    }

    /* child process */
    if (child_pid == 0) {

	/* This is needed to ensure SIGKILL to /bin/sh kills us */
	setpgid(0, 0);

	dup2(stdin_fds[0],  STDIN_FILENO);
	dup2(stdout_fds[1], STDOUT_FILENO); // Redirect stdout to pipe

        execl("/bin/sh", "/bin/sh", "-c", cmd_line, NULL);

        exit(0);

    } else {
	close(stdin_fds[0]);
	close(stdout_fds[1]); 
    }

    *assigned_pid = child_pid;
    *stdout_fd    = stdout_fds[0];
    *stdin_fd     = stdin_fds[1];

    return 0;
}

/*
static int 
__pclose(FILE * fp, pid_t pid)
{
    int stat;

    fclose(fp);

    while (waitpid(pid, &stat, 0) == -1) {
        if (errno != EINTR) {
            stat = -1;
            break;
        }
    }

    return stat;
}
*/

int
kill_lnx_app(struct app_state * app)
{


    return -1;
}

int
launch_lnx_app(char	   * name, 
	       char	   * exe_path, 
	       char	   * argv, 
	       char	   * envp,
	       hobbes_id_t   hpid)
{
    struct app_state * app = NULL;

    char * cmd_line  = NULL;
    pid_t  app_pid   = 0;
    int    stdin_fd  = 0;
    int    stdout_fd = 0;
    int    ret       = 0;

    asprintf(&cmd_line, "%s %s %s", envp, exe_path, argv); 

    printf("Launching app: %s\n", cmd_line);

    ret = __popen(cmd_line, &app_pid, &stdin_fd, &stdout_fd);
    free(cmd_line);
  
    if (ret != 0) {
	ERROR("Could not launch linux process\n");
	return -1;
    }

    {

	app = calloc(sizeof(struct app_state), 1);
	
	if (app == NULL) {
	    ERROR("Could not allocate app state\n");
	    return -1;
	}

	app->hpid         = hpid;
	app->pid          = app_pid;
	app->stdin_fd     = stdin_fd;
	app->stdout_fd    = stdout_fd;

	ret = add_fd_handler(app->stdout_fd, __handle_stdout, app);

	if (ret == -1) {
	    ERROR("Cannot add handler for process output\n");
	    free(app);
	    return -1;
	}

	ret = pet_htable_insert(app_htable, (uintptr_t)app->hpid, (uintptr_t)app);
	if (ret == 0) {
	    ERROR("Cannot add app to hashtable\n");
	    remove_fd_handler(app->stdout_fd);
	    free(app);
	    return -1;
	}
    }

    return 0;
}


int 
launch_hobbes_lnx_app(char * spec_str)
{
    pet_xml_t   spec = NULL;
    hobbes_id_t hpid = HOBBES_INVALID_ID;
    int         ret  = -1;    /* This is only set to 0 if the function completes successfully */


    spec = pet_xml_parse_str(spec_str);

    if (!spec) {
	ERROR("Invalid App spec\n");
	return -1;
    }

    {
	char        * name        = NULL; 
	char        * exe_path    = NULL; 
	char        * argv        = DEFAULT_ARGV;
	char        * envp        = DEFAULT_ENVP; 
	char        * hobbes_env  = NULL;	
	char        * val_str     = NULL;

	printf("App spec str = (%s)\n", spec_str);

	/* Executable Path Name */
	val_str = pet_xml_get_val(spec, "path");

	if (val_str) {
	    exe_path = val_str;
	} else {
	    ERROR("Missing required path in Hobbes APP specification\n");
	    goto out;
	}

	/* Process Name */
	val_str = pet_xml_get_val(spec, "name");

	if (val_str) {
	    name = val_str;
	} else {
	    name = exe_path;
	} 

	/* ARGV */
	val_str = pet_xml_get_val(spec, "argv");

	if (val_str) {
	    argv = val_str;
	}

	/* ENVP */
	val_str = pet_xml_get_val(spec, "envp");

	if (val_str) {
	    envp = val_str;
	}

	/* App id */
	val_str = pet_xml_get_val(spec, "app_id");
	hpid    = smart_atoi(HOBBES_INVALID_ID, val_str);
	if (hpid == HOBBES_INVALID_ID) {
	    ERROR("Missing required app_id in Hobbes APP specification\n");
	    goto out;
	}

	/* Register as a hobbes application */
	{
	    int chars_written = 0;

	    printf("Launching App (Hobbes AppID = %u) (EnclaveID=%d)\n", hpid,  hobbes_get_my_enclave_id() );
	    printf("Application Name=%s\n", name);

	    /* Hobbes enabled ENVP */
	    chars_written = asprintf(&hobbes_env, 
				     "%s=%u %s=%u %s", 
				     HOBBES_ENV_APP_ID,
				     hpid, 
				     HOBBES_ENV_ENCLAVE_ID,
				     hobbes_get_my_enclave_id(), 
				     envp);

	    if (chars_written == -1) {
		ERROR("Failed to allocate envp string for application (%s)\n", name);
		goto out;
	    }

	    /* Launch App */
	    ret = launch_lnx_app(name, 
				 exe_path, 
				 argv,
				 hobbes_env,
				 hpid);

	    free(hobbes_env);

	    if (ret != 0) {
		ERROR("Failed to Launch application (spec_str=[%s])\n", spec_str);
		goto out;
	    }

	    hobbes_set_app_state(hpid, APP_RUNNING);
	    hnotif_signal(HNOTIF_EVT_APPLICATION);
	}
    }


    ret = 0;

 out:
    if ((ret != 0) && (hpid != HOBBES_INVALID_ID)) {
	hobbes_set_app_state(hpid, APP_ERROR);
	hnotif_signal(HNOTIF_EVT_APPLICATION);
    }

    pet_xml_free(spec);
    return ret;
}



int
init_lnx_app(void)
{
    app_htable = pet_create_htable(0, app_hash_fn, app_eq_fn);
    if (!app_htable) {
	ERROR("Could not create application hashtable\n");
	return -1;
    }

    return 0;
}

int
deinit_lnx_app(void)
{
    pet_free_htable(app_htable, 1, 0);
    return 0;
}
