/* Hobbes Local File I/O handlers 
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */


int file_stat_handler (hcq_handle_t hcq, hcq_cmd_t cmd);
int file_open_handler (hcq_handle_t hcq, hcq_cmd_t cmd);
int file_read_handler (hcq_handle_t hcq, hcq_cmd_t cmd);
int file_write_handler(hcq_handle_t hcq, hcq_cmd_t cmd);
int file_fstat_handler(hcq_handle_t hcq, hcq_cmd_t cmd);
int file_close_handler(hcq_handle_t hcq, hcq_cmd_t cmd);
