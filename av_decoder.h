
#ifndef AV_DECODER
#define AV_DECODER

#endif
#include "config.h"
#include <inttypes.h>
#include <libavutil/frame.h>

struct fingerprint_block {
  LLONG start_pts,end_pts;
  uint8_t **data;
};

int init_decoder(char *filename1,char *filename2,uint8_t file_select);
int open_input_file(uint8_t file_select,uint8_t lang_flag);
void create_fingerprint_by_pts(uint16_t index,int64_t pts);
void close_filter();
