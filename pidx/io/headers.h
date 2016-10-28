#ifndef __HEADERS_H
#define __HEADERS_H


PIDX_return_code write_headers(PIDX_io file, int group_index, int start_var_index, int end_var_index, int layout_type);

PIDX_return_code one_time_initialize(PIDX_io file);

PIDX_return_code init_raw_headers_layout(PIDX_io file, int group_index, int start_var_index, int end_var_index, char* filename);

PIDX_return_code write_and_close_raw_headers(PIDX_io file, char* filename);
#endif