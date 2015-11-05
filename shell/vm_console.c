/* 
 * V3 Console utility
 * Taken from Palacios console display in MINIX ( by Erik Van der Kouwe )
 * (c) Jack lange, 2010
 * (c) Peter Dinda, 2011 (Scan code encoding)
 * (c) Brian Kocoloski, 2015 (Console support for Hobbes environments) 
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h> 
#include <sys/mman.h> 
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <curses.h>
#include <termios.h>
#include <linux/kd.h>
#include <linux/keyboard.h>

#include <pet_log.h>

#include <hobbes.h>
#include <hobbes_db.h>
#include <hobbes_enclave.h>
#include <hobbes_util.h>

#include <v3_ioctl.h>

#include "vm.h"

#define PAGE_SIZE sysconf(_SC_PAGESIZE)


extern int errno;
extern hdb_db_t hobbes_master_db;


static int use_curses = 0;
static int debug_enable = 0;


typedef enum { CONSOLE_CURS_SET = 1,
	       CONSOLE_CHAR_SET = 2,
	       CONSOLE_SCROLL = 3,
	       CONSOLE_UPDATE = 4,
               CONSOLE_RESOLUTION = 5 } console_op_t;


static struct {
    WINDOW * win;
    int x;
    int y;
    int rows;
    int cols;
    struct termios termios_old;
    unsigned char old_kbd_mode;
} console;


struct cursor_msg {
    int x;
    int y;
} __attribute__((packed));

struct character_msg {
    int x;
    int y;
    char c;
    unsigned char style;
} __attribute__((packed));

struct scroll_msg {
    int lines;
} __attribute__((packed));

struct resolution_msg {
    int cols;
    int rows;
} __attribute__((packed));


struct cons_msg {
    unsigned char op;
    union {
	struct cursor_msg cursor;
	struct character_msg  character;
	struct scroll_msg scroll;
	struct resolution_msg resolution;
    };
} __attribute__((packed)); 

struct hobbes_vm_cons_ring_buf {
    uint16_t read_idx;
    uint16_t write_idx;
    uint16_t total_entries;
    uint32_t reserved; /* This is needed to be interoperable with the Pisces console */

    struct cons_msg msgs[0];
} __attribute__((packed));


struct hobbes_vm_cons_connect_info {
    /* Enclave ID of VM */
    hobbes_id_t   enclave_id;

    /* Signallable segment for kicking reader */
    xemem_segid_t kick_segid;

    /* How many pages in the ring buffer */
    unsigned long num_pages;
} __attribute__((packed));


struct hobbes_vm_cons_keycode {
    hobbes_id_t   enclave_id;
    unsigned char scan_code;
} __attribute__((packed));



struct hobbes_vm_cons {
    /* Connection info passed to Pisces init task */
    struct hobbes_vm_cons_connect_info connect_info;

    /* XEMEM apid of ring buf */
    xemem_apid_t                       ring_buf_apid;

    /* Ring buf mapping */
    struct hobbes_vm_cons_ring_buf   * ring_buf;

    /* HCQ handle */
    hcq_handle_t                       hcq;
};



#define RING_BUF_PAGES 16
#define RING_BUF_SIZE  RING_BUF_PAGES * PAGE_SIZE

static int cont = 1;


static int handle_char_set(struct character_msg * msg) {
    char c = msg->c;

    if (debug_enable) {
	fprintf(stderr, "setting char (%c), at (x=%d, y=%d)\n", c, msg->x, msg->y);
    }

    if (c == 0) {
	c = ' ';
    }


    if ((c < ' ') || (c >= 127)) {
	if (debug_enable) { 
	    fprintf(stderr, "unexpected control character %d\n", c);
	}
	c = '?';
    }

    if (use_curses) {
	/* clip whatever falls outside the visible area to avoid errors */
	if ((msg->x < 0) || (msg->y < 0) ||
	    (msg->x > console.win->_maxx) || 
	    (msg->y > console.win->_maxy)) {

	    if (debug_enable) { 
		fprintf(stderr, "Char out of range (x=%d,y=%d) MAX:(x=%d,y=%d)\n",
			msg->x, msg->y, console.win->_maxx, console.win->_maxy);
	    }

	    return -1;
	}

	if ((msg->x == console.win->_maxx) &&
	    (msg->y == console.win->_maxy)) {
	    return -1;
	}

	mvwaddch(console.win, msg->y, msg->x, c);

    } else {
	//stdout text display
	while (console.y < msg->y) {
	    printf("\n");
	    console.x = 0;
	    console.y++;
	}

	while (console.x < msg->x) {
	    printf(" ");
	    console.x++;
	}

	printf("%c", c);
	console.x++;

	//assert(console.x <= console.cols); 

	if (console.x == console.cols) {
	    printf("\n");
	    console.x = 0;
	    console.y++;
	}
    }

    return 0;
}

static int handle_curs_set(struct cursor_msg * msg) {
    if (debug_enable) {
	fprintf(stderr, "cursor set: (x=%d, y=%d)\n", msg->x, msg->y);
    }

    if (use_curses) {
	/* nothing to do now, cursor is set before update to make sure it isn't 
	 * affected by character_set
	 */

	console.x = msg->x;
	console.y = msg->y;
    }
    
    return 0;
}


static int handle_scroll(struct scroll_msg * msg) {
    int lines = msg->lines;

    if (debug_enable) {
	fprintf(stderr, "scroll: %d lines\n", lines);
    }


    assert(lines >= 0);

    if (use_curses) {
	while (lines > 0) {
	    scroll(console.win);
	    lines--;
	}
    } else {
	console.y -= lines;	
    }

    return 0;
}

static int handle_text_resolution(struct resolution_msg * msg) {
    if (debug_enable) {
	fprintf(stderr, "text resolution: rows=%d, cols=%d\n", msg->rows, msg->cols);
    }


    console.rows = msg->rows;
    console.cols = msg->cols;

    return 0;
}

static int handle_update( void ) {
    if (debug_enable) {
	fprintf(stderr, "update\n");
    }    

    if (use_curses) {

	if ( (console.x >= 0) && (console.y >= 0) &&
	     (console.x <= console.win->_maxx) &&
	     (console.y <= console.win->_maxy) ) {

	    wmove(console.win, console.y, console.x);

	}

	wrefresh(console.win);
    } else {
	fflush(stdout);
    }

    return 0;
}




/*static int send_key(int cons_fd, char scan_code) {

    return 0;
}*/



void handle_lnx_exit(void) {
    if ( debug_enable ) {
	fprintf(stderr, "Exiting from console terminal\n");
    }

    if (use_curses) {
	endwin();
    }

    //    tcsetattr(STDIN_FILENO, TCSANOW, &console.termios_old);

    // ioctl(STDIN_FILENO, KDSKBMODE, K_XLATE);
}

static void handle_pisces_sigint(int signum) {
    if (use_curses) {
	endwin();
    }

    cont = 0;
}

 

#define NO_KEY { 0, 0 }

struct key_code {
    unsigned char scan_code;
    unsigned char capital;
};


static const struct key_code ascii_to_key_code[] = {             // ASCII Value Serves as Index
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x00 - 0x03
    NO_KEY,         NO_KEY,         NO_KEY,         { 0x0E, 0 }, // 0x04 - 0x07
    { 0x0E, 0 },    { 0x0F, 0 },    { 0x1C, 0 },    NO_KEY,      // 0x08 - 0x0B
    NO_KEY,         { 0x1C, 0 },    NO_KEY,         NO_KEY,      // 0x0C - 0x0F
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x10 - 0x13
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x14 - 0x17
    NO_KEY,         NO_KEY,         NO_KEY,         { 0x01, 0 }, // 0x18 - 0x1B
    NO_KEY,         NO_KEY,         NO_KEY,         NO_KEY,      // 0x1C - 0x1F
    { 0x39, 0 },    { 0x02, 1 },    { 0x28, 1 },    { 0x04, 1 }, // 0x20 - 0x23
    { 0x05, 1 },    { 0x06, 1 },    { 0x08, 1 },    { 0x28, 0 }, // 0x24 - 0x27
    { 0x0A, 1 },    { 0x0B, 1 },    { 0x09, 1 },    { 0x0D, 1 }, // 0x28 - 0x2B
    { 0x33, 0 },    { 0x0C, 0 },    { 0x34, 0 },    { 0x35, 0 }, // 0x2C - 0x2F
    { 0x0B, 0 },    { 0x02, 0 },    { 0x03, 0 },    { 0x04, 0 }, // 0x30 - 0x33
    { 0x05, 0 },    { 0x06, 0 },    { 0x07, 0 },    { 0x08, 0 }, // 0x34 - 0x37
    { 0x09, 0 },    { 0x0A, 0 },    { 0x27, 1 },    { 0x27, 0 }, // 0x38 - 0x3B
    { 0x33, 1 },    { 0x0D, 0 },    { 0x34, 1 },    { 0x35, 1 }, // 0x3C - 0x3F
    { 0x03, 1 },    { 0x1E, 1 },    { 0x30, 1 },    { 0x2E, 1 }, // 0x40 - 0x43
    { 0x20, 1 },    { 0x12, 1 },    { 0x21, 1 },    { 0x22, 1 }, // 0x44 - 0x47
    { 0x23, 1 },    { 0x17, 1 },    { 0x24, 1 },    { 0x25, 1 }, // 0x48 - 0x4B
    { 0x26, 1 },    { 0x32, 1 },    { 0x31, 1 },    { 0x18, 1 }, // 0x4C - 0x4F
    { 0x19, 1 },    { 0x10, 1 },    { 0x13, 1 },    { 0x1F, 1 }, // 0x50 - 0x53
    { 0x14, 1 },    { 0x16, 1 },    { 0x2F, 1 },    { 0x11, 1 }, // 0x54 - 0x57
    { 0x2D, 1 },    { 0x15, 1 },    { 0x2C, 1 },    { 0x1A, 0 }, // 0x58 - 0x5B
    { 0x2B, 0 },    { 0x1B, 0 },    { 0x07, 1 },    { 0x0C, 1 }, // 0x5C - 0x5F
    { 0x29, 0 },    { 0x1E, 0 },    { 0x30, 0 },    { 0x2E, 0 }, // 0x60 - 0x63
    { 0x20, 0 },    { 0x12, 0 },    { 0x21, 0 },    { 0x22, 0 }, // 0x64 - 0x67
    { 0x23, 0 },    { 0x17, 0 },    { 0x24, 0 },    { 0x25, 0 }, // 0x68 - 0x6B
    { 0x26, 0 },    { 0x32, 0 },    { 0x31, 0 },    { 0x18, 0 }, // 0x6C - 0x6F
    { 0x19, 0 },    { 0x10, 0 },    { 0x13, 0 },    { 0x1F, 0 }, // 0x70 - 0x73
    { 0x14, 0 },    { 0x16, 0 },    { 0x2F, 0 },    { 0x11, 0 }, // 0x74 - 0x77
    { 0x2D, 0 },    { 0x15, 0 },    { 0x2C, 0 },    { 0x1A, 1 }, // 0x78 - 0x7B
    { 0x2B, 1 },    { 0x1B, 1 },    { 0x29, 1 },    { 0x0E, 0 }  // 0x7C - 0x7F
};




static int 
send_char_to_palacios_as_scancodes(int (*write_fn)(int, unsigned char, void *),
				   int           fd,
				   unsigned char c,
				   void        * priv_data)
{
    unsigned char sc;

    if (debug_enable) {
	fprintf(stderr,"key '%c'\n",c);
    }

    if (c<0x80) { 
	struct key_code k = ascii_to_key_code[c];
	
	if (k.scan_code==0 && k.capital==0) { 
	    if (debug_enable) { 
		fprintf(stderr,"Cannot send key '%c' to palacios as it maps to no scancode\n",c);
	    }
	} else {
	    if (k.capital) { 
		//shift down
		sc = 0x2a ; // left shift down
		if (write_fn(fd, sc, priv_data) != 0)
		    return -1;
	    }
	    
	    
	    sc = k.scan_code;
	    
	    if (write_fn(fd, sc, priv_data) != 0)  // key down
		return -1;
	    
	    sc |= 0x80;      // key up

	    if (write_fn(fd, sc, priv_data) != 0) 
		return -1;
	    
	    if (k.capital) { 
		sc = 0x2a | 0x80;
		if (write_fn(fd, sc, priv_data) != 0) 
		    return -1;
	    }
	}
	    
    } else {
	if (debug_enable) { 
	    fprintf(stderr,"Cannot send key '%c' to palacios because it is >=0x80\n",c);
	}
    }
    return 0;
}


#define MIN_TTY_COLS  80
#define MIN_TTY_ROWS  25
static int check_terminal_size (void)
{
    unsigned short n_cols = 0;
    unsigned short n_rows = 0;
    struct winsize winsz; 

    ioctl (fileno(stdin), TIOCGWINSZ, &winsz);
    n_cols = winsz.ws_col;
    n_rows = winsz.ws_row;

    if (n_cols < MIN_TTY_COLS || n_rows < MIN_TTY_ROWS) {
        printf ("Your window is not large enough.\n");
        printf ("It must be at least %dx%d, but yours is %dx%d\n",
                MIN_TTY_COLS, MIN_TTY_ROWS, n_cols, n_rows);
	return (-1);
    }

    /* SUCCESS */
    return (0);
}



static int
__console(hobbes_id_t    enclave_id,
	  int            cons_fd,
	  int (*write_fn)(int, unsigned char, void *),
	  int (*handle_fn)(int, void *),
	  void         * private_data)
{
    use_curses = 1;

    /* Check for minimum Terminal size at start */
    if (0 != check_terminal_size()) {
        printf ("Error: terminal too small!\n");
        return -1;
    }

    tcgetattr(STDIN_FILENO, &console.termios_old);

    console.x = 0;
    console.y = 0;


    if (use_curses) {
	gettmode();
	console.win = initscr();
	
	if (console.win == NULL) {
	    fprintf(stderr, "Error initialization curses screen\n");
	    return -1;
	}

	scrollok(console.win, 1);

	erase();
    }

    /*
    termios = console.termios_old;
    termios.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | IGNPAR);
    termios.c_iflag &= ~(INLCR | INPCK | ISTRIP | IXOFF | IXON | PARMRK); 
    //termios.c_iflag &= ~(ICRNL | INLCR );    

    //  termios.c_iflag |= SCANCODES; 
    //    termios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL); 
    //termios.c_lflag &= ~(ICANON | IEXTEN | ISIG | NOFLSH);
    termios.c_lflag &= ~(ICANON | ECHO);

    termios.c_cc[VMIN] = 1;
    termios.c_cc[VTIME] = 0;

    tcflush(STDIN_FILENO, TCIFLUSH);
    tcsetattr(STDIN_FILENO, TCSANOW, &termios);
    */    

    raw();
    cbreak();
    noecho();
    keypad(console.win, TRUE);

    //ioctl(STDIN_FILENO, KDSKBMODE, K_RAW);

    while (cont) {
	int ret; 
	fd_set rset;

	FD_ZERO(&rset);
	FD_SET(cons_fd, &rset);
	FD_SET(STDIN_FILENO, &rset);

	ret = select(cons_fd + 1, &rset, NULL, NULL, NULL);
	
	//	printf("Returned from select...\n");

	if (ret == 0) {
	    continue;
	} else if (ret == -1) {
	    if (errno == EINTR)
		continue;
	    fprintf(stderr, "%s (errno=%d)\n", strerror(errno), errno);
	    return -1;
	}

	if (FD_ISSET(cons_fd, &rset)) {
	    if (handle_fn(cons_fd, private_data) == -1) {
		printf("Console Error\n");
		return -1;
	    }
	}

	if (FD_ISSET(STDIN_FILENO, &rset)) {
	    int key = getch();



	    if (key == 27) { // ESC
		int key2 = getch();

		if (key2 == '`') {
		    break;
		} else if (key2 == 27) {
		    unsigned char sc = 0;
		    sc = 0x01;
		    if (write_fn(cons_fd, sc, private_data) != 0)
			return -1;
		    sc |= 0x80;
		    if (write_fn(cons_fd, sc, private_data) != 0)
			return -1;
		} else {
		    unsigned char sc = 0;

		    sc = 0x1d;  // left ctrl down
		    if (write_fn(cons_fd, sc, private_data) != 0)
			return -1;

		    if (send_char_to_palacios_as_scancodes(write_fn, cons_fd, key2, private_data)) {
			printf("Error sending key to console\n");
			return -1;
		    }

		    sc = 0x1d | 0x80;   // left ctrl up
		    if (write_fn(cons_fd, sc, private_data) != 0)
			return -1;
		}
		


	  } else if (key == KEY_LEFT) { // LEFT ARROW
		unsigned char sc = 0;
		//	sc = 0xe0;
		//writeit(cons_fd, sc);
		sc = 0x4B;
		if (write_fn(cons_fd, sc, private_data) != 0)
		    return -1;
		sc = 0x4B | 0x80;;
		if (write_fn(cons_fd, sc, private_data) != 0)
		    return -1;
		//sc = 0xe0 | 0x80;
		//writeit(cons_fd, sc);
	    } else if (key == KEY_RIGHT) { // RIGHT ARROW
		unsigned char sc = 0;
		//sc = 0xe0;
		//writeit(cons_fd, sc);
		sc = 0x4D;
		if (write_fn(cons_fd, sc, private_data) != 0)
		    return -1;
		sc = 0x4d | 0x80;
		if (write_fn(cons_fd, sc, private_data) != 0)
		    return -1;
		//sc = 0xe0 | 0x80;
		//writeit(cons_fd, sc);
	    } else if (key == KEY_UP) { // UP ARROW
		unsigned char sc = 0;
		//sc = 0xe0;
		//writeit(cons_fd, sc);
		sc = 0x48;
		if (write_fn(cons_fd, sc, private_data) != 0)
		    return -1;
		sc = 0x48 | 0x80;
		if (write_fn(cons_fd, sc, private_data) != 0)
		    return -1;
		//sc = 0xe0 | 0x80;
		//writeit(cons_fd, sc);
	    } else if (key == KEY_DOWN) { // DOWN ARROW
		unsigned char sc = 0;
		//	sc = 0xe0;
		//writeit(cons_fd, sc);
		sc = 0x50;
		if (write_fn(cons_fd, sc, private_data) != 0)
		    return -1;
		sc = 0x50 | 0x80;
		if (write_fn(cons_fd, sc, private_data) != 0)
		    return -1;
		//sc = 0xe0 | 0x80;
		//writeit(cons_fd, sc);


            } else {
		if (send_char_to_palacios_as_scancodes(write_fn, cons_fd, key, private_data)) {
		    printf("Error sending key to console\n");
		    return -1;
		}
	    }
	    
	}
    } 

    erase();

    printf("Console terminated\n");

    return 0; 

}


static int 
__handle_console_msg(struct cons_msg * msg)
{
    switch (msg->op) {
	case CONSOLE_CURS_SET:
	    //	    printf("Console cursor set (x=%d, y=%d)\n", msg.cursor.x, msg.cursor.y);
	    handle_curs_set(&(msg->cursor));
	    break;
	case CONSOLE_CHAR_SET:
	    handle_char_set(&(msg->character));
	    /*	    printf("Console character set (x=%d, y=%d, c=%c, style=%c)\n", 
	      msg.character.x, msg.character.y, msg.character.c, msg.character.style);*/
	    break;
	case CONSOLE_SCROLL:
	    //  printf("Console scroll (lines=%d)\n", msg.scroll.lines);
	    handle_scroll(&(msg->scroll));
	    break;
	case CONSOLE_UPDATE:
	    // printf("Console update\n");
	    handle_update();
	    break;
	case CONSOLE_RESOLUTION:
	    handle_text_resolution(&(msg->resolution));
	    break;
	default:
	    printf("Invalid console message operation (%d)\n", msg->op);
	    break;
    }

    return 0;
}

static int
vm_lnx_write(int           cons_fd,
             unsigned char c,
	     void        * private_data)
{
    return (write(cons_fd, &c, 1) == 1) ? 0 : -1;
}

static int
vm_lnx_handle(int    cons_fd,
              void * private_data)
{
    struct cons_msg msg;
    int ret;

    ret = read(cons_fd, &msg, sizeof(struct cons_msg));

    if (ret != sizeof(struct cons_msg)) {
	fprintf(stderr, "Error: Could not read LINUX console message\n");
	return -1;
    }

    return __handle_console_msg(&msg);
}


static int 
vm_lnx_enclave_console(hobbes_id_t enclave_id,
                       char      * enclave_name)
{
    int    cons_ret = -1;
    int    cons_fd  = -1;
    int    vm_fd    = -1;
    int    dev_id   = hdb_get_enclave_dev_id(hobbes_master_db, enclave_id);
    char * dev_path = get_vm_dev_path(dev_id);

    vm_fd = open(dev_path, O_RDONLY);
    if (vm_fd == -1) {
	ERROR("Cannot open VM device for enclave %d (%s)\n", 
		enclave_id, enclave_name);
	return -1;
    }

    cons_fd = ioctl(vm_fd, V3_VM_CONSOLE_CONNECT, NULL);

    close(vm_fd);

    if (cons_fd == -1) {
	ERROR("Could not connect VM console for enclave %d (%s)\n", 
		enclave_id, enclave_name);
	return -1;
    }

    atexit(handle_lnx_exit);

    cons_ret =  __console(enclave_id, cons_fd, vm_lnx_write, vm_lnx_handle, NULL);

    close(cons_fd);

    return cons_ret;
}

static int
vm_pisces_write(int           cons_fd,
                unsigned char c,
		void        * private_data)
{
    struct hobbes_vm_cons       * cons = private_data;
    hcq_cmd_t                     cmd = HCQ_INVALID_CMD;
    struct hobbes_vm_cons_keycode key_code;

    char    * err_str  = NULL;
    uint32_t  err_len  = 0;
    int       status   = 0;

    key_code.enclave_id = cons->connect_info.enclave_id;
    key_code.scan_code  = c;

    cmd = hcq_cmd_issue(
	cons->hcq,
	ENCLAVE_CMD_VM_CONS_KEYCODE,
	sizeof(struct hobbes_vm_cons_keycode),
	&key_code);
    if (cmd == HCQ_INVALID_CMD) {
	ERROR("Could not issue command to host enclave command queue\n");
	return -1;
    }

    status  = hcq_get_ret_code(cons->hcq, cmd);
    err_str = hcq_get_ret_data(cons->hcq, cmd, &err_len);

    if (err_len > 0) {
	printf(" ERROR: %s\n", err_str);
    }

    hcq_cmd_complete(cons->hcq, cmd);

    return status;
}

static int
__vm_pisces_dequeue(struct hobbes_vm_cons_ring_buf * ring_buf)
{
    uint16_t read_idx  = 0;
    uint16_t write_idx = 0;

    __sync_synchronize();
    read_idx  = ring_buf->read_idx;
    __sync_synchronize();
    write_idx = ring_buf->write_idx;
    __sync_synchronize();

    if (read_idx == write_idx)
	return -1;

    ring_buf->read_idx = (read_idx + 1) % ring_buf->total_entries;
    __sync_synchronize();

    return 0;
}

static int
__vm_pisces_get_msg(struct hobbes_vm_cons_ring_buf * ring_buf,
                    struct cons_msg               ** msg)
{
    uint16_t read_idx  = 0;
    uint16_t write_idx = 0;

    __sync_synchronize();
    read_idx  = ring_buf->read_idx;
    __sync_synchronize();
    write_idx = ring_buf->write_idx;
    __sync_synchronize();

    if (read_idx == write_idx)
	return -1;

    *msg = &(ring_buf->msgs[read_idx]);
    __sync_synchronize();
    
    return 0;
}

static int
vm_pisces_handle(int    cons_fd,
                 void * private_data)
{
    struct hobbes_vm_cons * cons = private_data;
    struct cons_msg       * msg;

    int status        = 0;
    int notifications = 0;

    /* The Pisces init task will keep kicking as long as the queue is not
     * empty; there is no guarantee that the number of kicks equals the number
     * of console messages. In fact they will almost never be equal.
     *
     * The strategy is to first clear all pending notifications, then simply
     * dequeue and process messages until there are none left. Of course, more
     * notifications could (and likely will) come while processing, which just
     * means cons_fd will be active again when we're done
     *
     * Note that it's possible to have a notification without any new console
     * messages. This can happen if we get kicked and a new message arrives
     * while already executing this loop. We will process the message without
     * acking the notification, and then will get kicked once we return, at
     * which point there may not necessarily be any new messages to process.
     */

    /* Make sure we have at least one notification - we got kicked afterall */
    notifications = xemem_ack(cons_fd);
    assert(notifications >= 0);

    /* Clear the rest */
    while (notifications-- > 0)
	assert(xemem_ack(cons_fd) >= 0);

    /* Process all that's there */
    while (1) {
	status = __vm_pisces_get_msg(cons->ring_buf, &msg);
	if (status != 0)
	    break;

	status = __handle_console_msg(msg);
	assert(status == 0);

	status = __vm_pisces_dequeue(cons->ring_buf);
	assert(status == 0);
    }

    return 0;
}


static void
__vm_pisces_enclave_console_disconnect(struct hobbes_vm_cons * cons,
                                       char                  * host_enclave_name)
{
    hcq_cmd_t cmd      = HCQ_INVALID_CMD;
    char    * err_str  = NULL;
    uint32_t  err_len  = 0;

    /* Disconnect the console */
    cmd = hcq_cmd_issue(
	cons->hcq,
	ENCLAVE_CMD_VM_CONS_DISCONNECT,
	sizeof(hobbes_id_t),
	&(cons->connect_info.enclave_id));
    if (cmd == HCQ_INVALID_CMD) {
	ERROR("Could not issue command to host enclave command queue\n");
	return;
    }

    err_str = hcq_get_ret_data(cons->hcq, cmd, &err_len);

    if (err_len > 0) {
	printf("%s ERROR: %s\n", host_enclave_name, err_str);
    }

    hcq_cmd_complete(cons->hcq, cmd);
}

static int
vm_pisces_enclave_console(hobbes_id_t enclave_id,
                          char      * enclave_name,
			  hobbes_id_t host_enclave_id,
			  char      * host_enclave_name)
{
    hcq_cmd_t    cmd  = HCQ_INVALID_CMD;
    char   * err_str  =  NULL;
    uint32_t err_len  =  0;
    int      cons_fd  = -1;
    int      cons_ret =  0;
    int64_t  status   = -1;

    struct hobbes_vm_cons * cons = NULL;
    struct xemem_addr       addr;

    /* Allocate console struct */
    cons = malloc(sizeof(struct hobbes_vm_cons));
    if (cons == NULL) {
	ERROR("Could not allocate console\n");
	return -1;
    }

    /* Setup connection request */
    cons->connect_info.enclave_id = enclave_id;
    cons->connect_info.num_pages  = RING_BUF_PAGES;
    cons->connect_info.kick_segid = xemem_make_signalled(NULL, 0, NULL, &cons_fd);
    if (cons->connect_info.kick_segid == XEMEM_INVALID_SEGID) {
	ERROR("Could not allocate XEMEM segid for cons event notification\n");
	goto out_xemem;
    }

    cons->hcq = hobbes_open_enclave_cmdq(host_enclave_id);
    if (cons->hcq == HCQ_INVALID_HANDLE) {
	ERROR("Could not open command queue for enclave (%s)\n",
		host_enclave_name);
	goto out_open_cmdq;
    }

    /* Connect the console */
    cmd = hcq_cmd_issue(
	cons->hcq,
	ENCLAVE_CMD_VM_CONS_CONNECT,
	sizeof(struct hobbes_vm_cons_connect_info),
	&(cons->connect_info));
    if (cmd == HCQ_INVALID_CMD) {
	ERROR("Could not issue command to host enclave command queue (%s)\n",
		 host_enclave_name);
	goto out_cmd_issue;
    }

    status  = hcq_get_ret_code(cons->hcq, cmd);
    err_str = hcq_get_ret_data(cons->hcq, cmd, &err_len);

    if (err_len > 0) {
	printf("%s ERROR: %s\n", host_enclave_name, err_str);
    }

    hcq_cmd_complete(cons->hcq, cmd);

    if (status < 0) {
	goto out_connect;
    }

    /* Map the console ring buffer via XEMEM */
    cons->ring_buf_apid = xemem_get((xemem_segid_t)status, XEMEM_RDWR);
    if (cons->ring_buf_apid < 0) {
	ERROR("Could not XEMEM_GET ring buffer\n");
	goto out_xemem_get;
    }

    addr.apid   = cons->ring_buf_apid;
    addr.offset = 0;

    cons->ring_buf = xemem_attach(addr, RING_BUF_SIZE, NULL);
    if (cons->ring_buf == MAP_FAILED) {
	ERROR("Could not XEMEM_ATTACH ring buffer\n");
	goto out_xemem_attach;
    }

    printf("RBUF TOTAL ENTRIES: %d\n", cons->ring_buf->total_entries);

    /* Catch sigint */
    signal(SIGINT, handle_pisces_sigint);

    /* Run the console */
    cons_ret = __console(
	    cons->connect_info.enclave_id, 
	    cons_fd, 
	    vm_pisces_write, 
	    vm_pisces_handle, 
	    cons);

    if (cons_ret != 0) {
	ERROR("Console exited with error: %d\n", cons_ret);
    }

    xemem_detach(cons->ring_buf);

out_xemem_attach:
    xemem_release(cons->ring_buf_apid);

out_xemem_get:
    /* Disconnect the console */
    __vm_pisces_enclave_console_disconnect(cons, host_enclave_name);

out_connect:
out_cmd_issue:
    hobbes_close_enclave_cmdq(cons->hcq);

out_open_cmdq:
    close(cons_fd);
    xemem_remove(cons->connect_info.kick_segid);

out_xemem:
    free(cons);

    return status;
}


int
vm_enclave_console(hobbes_id_t enclave_id)
{
    hobbes_id_t    host_enclave_id   = HOBBES_INVALID_ID;
    enclave_type_t host_enclave_type = INVALID_ENCLAVE;

    char * enclave_name      = NULL;
    char * host_enclave_name = NULL;

    enclave_name    = hdb_get_enclave_name(hobbes_master_db, enclave_id);
    host_enclave_id = hobbes_get_enclave_parent(enclave_id);
    if (host_enclave_id == HOBBES_INVALID_ID) {
	ERROR("Could not find parent enclave for enclave %d (%s)\n",
		enclave_id, enclave_name);
	return -1;
    }

    host_enclave_name = hdb_get_enclave_name(hobbes_master_db, host_enclave_id);
    host_enclave_type = hobbes_get_enclave_type(host_enclave_id);
    if (host_enclave_type == INVALID_ENCLAVE) {
	ERROR("Could not find enclave %d (%s)\n", 
		host_enclave_id, host_enclave_name);
	return -1;
    }

    if (host_enclave_type == MASTER_ENCLAVE)
	return vm_lnx_enclave_console(enclave_id, enclave_name);
    else if (host_enclave_type == PISCES_ENCLAVE)
	return vm_pisces_enclave_console(enclave_id, enclave_name, host_enclave_id, host_enclave_name);
    else {
	ERROR("Cannot initialize VM console on host enclave %d (%s)\n", 
		host_enclave_id, host_enclave_name);
	return -1;
    }
}
