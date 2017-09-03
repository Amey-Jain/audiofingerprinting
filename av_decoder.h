
#ifndef AV_DECODER
#define AV_DECODER

#endif
#include "config.h"
#include <stdint.h>

struct fingerprint_block {
  LLONG start_pts,end_pts;
  uint8_t **data;
};

int init_decoder(char *filename1,char *filename2);
int open_input_file(uint8_t file_select,uint8_t lang_flag);
int open_audio_input_file(char *filename);
int create_fingerprint_by_pts(uint16_t index,uint64_t pts);
int create_packet_timings();
void set_current_processing_video(uint8_t select);
void close_filter();
