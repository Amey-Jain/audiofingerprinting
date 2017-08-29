/* File to extract audio from video   */
#include <stdio.h>
#include <libavformat/avformat.h>
#include <math.h>
#include <unistd.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/frame.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include "bufferqueue.h"
#include "av_decoder.h"
#include "config.h"
#include "spectogram.h"
#include "lsh_db.h"

static AVFormatContext *fmt_ctx_orig = NULL,*fmt_ctx_edited = NULL;
static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *dec_ctx_orig = NULL, *dec_ctx_edited = NULL;
static AVCodecContext *dec_ctx = NULL;
static int audio_stream_index = -1;
static AVFilterGraph *graph_resample=NULL;
static AVFilterGraph *graph_frame_2048=NULL;
static AVFilterGraph *graph_hann=NULL;
static AVFilterContext *src=NULL,*sink=NULL;
static AVFilterContext *Fsrc=NULL,*Fsink=NULL;
static AVFilterContext *Wsrc=NULL,*Wsink=NULL;
//static struct input_parameters i_para[2];
char *orig_video_name = NULL;
char *edited_video_name = NULL;
enum AVSampleFormat INPUT_SAMPLE_FMT = -1;
uint64_t INPUT_SAMPLERATE = 0;
uint64_t INPUT_CHANNEL_LAYOUT = 0;
AVRational INPUT_TIMEBASE;
uint64_t pts_first_frame_orig = 0;
uint64_t pts_first_frame_edited = 0;
uint64_t pts_first_frame = 0;
static AVFrame *first_frame_orig = NULL, *first_frame_edited = NULL;
uint16_t idx = 0;
uint8_t current_select = -1;

//taken from ffmpeg-filter_audio.c https://www.ffmpeg.org/doxygen/2.2/filter_audio_8c-example.html
static int init_filter_graph_resample(AVFilterGraph **graph, AVFilterContext **src, AVFilterContext **sink)
{
  AVFilterGraph *filter_graph;
  AVFilterContext *abuffer_ctx;
  AVFilter        *abuffer;
  AVFilterContext *aformat_ctx;
  AVFilter        *aformat;
  AVFilterContext *abuffersink_ctx;
  AVFilter        *abuffersink;
  AVFilterContext *ashowinfo_ctx;
  AVFilter        *ashowinfo;
  AVFilterContext *fftfilt_ctx;
  AVFilter        *fftfilt;

  AVDictionary *options_dict = NULL;
  uint8_t options_str[1024];
  uint8_t ch_layout[64];
  int err;
  char errstr[256];
  /* Create a new filtergraph, which will contain all the filters. */
  filter_graph = avfilter_graph_alloc();
  if (!filter_graph) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Unable to create filter graph.\n"ANSI_COLOR_RESET);
    return AVERROR(ENOMEM);
  }
  
  /* Create the abuffer filter;
   * it will be used for feeding the data into the graph. */
  abuffer = avfilter_get_by_name("abuffer");
  if (abuffer == NULL) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: ERROR: Could not find the abuffer filter.\n"ANSI_COLOR_RESET);
    return AVERROR_FILTER_NOT_FOUND;
  }
  abuffer_ctx = avfilter_graph_alloc_filter(filter_graph, abuffer, "src");
  if (abuffer_ctx == NULL) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: ERROR: Could not allocate the abuffer instance.\n"ANSI_COLOR_RESET);
    return AVERROR(ENOMEM);
  }
  av_get_channel_layout_string(ch_layout, sizeof(ch_layout), 0, INPUT_CHANNEL_LAYOUT);
  av_opt_set(abuffer_ctx, "channel_layout", ch_layout, AV_OPT_SEARCH_CHILDREN);
  av_opt_set(abuffer_ctx, "sample_fmt", av_get_sample_fmt_name(INPUT_SAMPLE_FMT), AV_OPT_SEARCH_CHILDREN);
  av_opt_set_q(abuffer_ctx, "time_base", (AVRational){ 1, INPUT_SAMPLERATE }, AV_OPT_SEARCH_CHILDREN);
  av_opt_set_int(abuffer_ctx, "sample_rate", INPUT_SAMPLERATE, AV_OPT_SEARCH_CHILDREN);
  /* Now initialize the filter; we pass NULL options, since we have already                                                                                               
   * set all the options above. */
  err = avfilter_init_str(abuffer_ctx, NULL);
  if (err < 0) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not initialize the abuffer filter.\n"ANSI_COLOR_RESET);
    return err;
  }
  
  // Create resampling filter. 
  aformat = avfilter_get_by_name("aformat");
  if (!aformat) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not find the aformat filter.\n"ANSI_COLOR_RESET);
    return AVERROR_FILTER_NOT_FOUND;
  }
  aformat_ctx = avfilter_graph_alloc_filter(filter_graph, aformat, "aformat");
  if (!aformat_ctx) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not allocate the resample instance.\n"ANSI_COLOR_RESET);
    return AVERROR(ENOMEM);
  }
  // option settings in aformat filter
  snprintf(options_str, sizeof(options_str),"sample_fmts=%s:sample_rates=%d:channel_layouts=0x%"PRIx64, av_get_sample_fmt_name(AV_SAMPLE_FMT_FLT), 5512, (uint64_t)AV_CH_LAYOUT_MONO);
  err = avfilter_init_str(aformat_ctx, options_str);
  if (err < 0) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not initialize the resampling filter.\n"ANSI_COLOR_RESET);
    return err;
  }

  if(DEBUG){
    ashowinfo = avfilter_get_by_name("ashowinfo");
    if (!ashowinfo) {
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not find the ashowinfo filter.\n"ANSI_COLOR_RESET);
      return AVERROR_FILTER_NOT_FOUND;
    }
    ashowinfo_ctx = avfilter_graph_alloc_filter(filter_graph, ashowinfo, "ashowinfo_framing");
    if (!ashowinfo_ctx) {
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not allocate the ashowinfo instance.\n"ANSI_COLOR_RESET);
      return AVERROR(ENOMEM);
    }
    err = avfilter_init_str(ashowinfo_ctx, NULL);
    if (err < 0) {
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not initialize the ashowinfo instance.\n"ANSI_COLOR_RESET);
      return err;
    }
  }
  
  /* Finally create the abuffersink filter;
   * it will be used to get the filtered data out of the graph. */
  abuffersink = avfilter_get_by_name("abuffersink");
  if (!abuffersink) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not find the abuffersink filter.\n"ANSI_COLOR_RESET);
    return AVERROR_FILTER_NOT_FOUND;
  }
  abuffersink_ctx = avfilter_graph_alloc_filter(filter_graph, abuffersink, "sink");
  if (!abuffersink_ctx) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not allocate the abuffersink instance.\n"ANSI_COLOR_RESET);
    return AVERROR(ENOMEM);
  }
  /* This filter takes no options. */
  err = avfilter_init_str(abuffersink_ctx, NULL);
  if (err < 0) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not initialize the abuffersink instance.\n"ANSI_COLOR_RESET);
    return err;
  }
  
  /* Connect the filters;
   * in this simple case the filters just form a linear chain. */
  err = avfilter_link(abuffer_ctx, 0, aformat_ctx, 0);
  if(err < 0) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR:  connecting filters abuffer and aformat %d error\n"ANSI_COLOR_RESET,err);
    return err;
  }
  
  if(DEBUG){     
    err = avfilter_link(aformat_ctx, 0, ashowinfo_ctx, 0);
    if (err < 0) {
      fprintf(stderr,ANSI_COLOR_ERROR"Error connecting filters aformat and ashowinfo %d error\n"ANSI_COLOR_RESET,err);
      return err;
    } 
    err = avfilter_link(ashowinfo_ctx, 0, abuffersink_ctx, 0);
    if (err < 0) {
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: connecting filters ashowinfo and buffersink %d error\n"ANSI_COLOR_RESET,err);
      return err;
    }
  }
  
  else {
    err = avfilter_link(aformat_ctx, 0, abuffersink_ctx, 0);
    if(err < 0) {
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: connecting filters aformat and abuffersink %d error\n"ANSI_COLOR_RESET,err);
      return err;
    }
  }
  /* Configure the graph. */
  err = avfilter_graph_config(filter_graph, NULL);
  if (err < 0) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: configuring the filter graph\n"ANSI_COLOR_RESET);
    return err;
  }
  av_buffersink_set_frame_size(abuffersink_ctx, 64);
  *graph = filter_graph;
  *src   = abuffer_ctx;
  *sink  = abuffersink_ctx;
  return 0;
}

/*
Initialise the filter graph used for windowing.
 */
static int init_filter_graph_framing_2048(AVFilterGraph **graph, AVFilterContext **src, AVFilterContext **sink)
{
  AVFilterGraph *filter_graph;
  AVFilterContext *abuffer_ctx;
  AVFilter        *abuffer;
  AVFilterContext *ashowinfo_ctx;
  AVFilter        *ashowinfo;
  AVFilterContext *asetnsamples_ctx;
  AVFilter        *asetnsamples;
  AVFilterContext *abuffersink_ctx;
  AVFilter        *abuffersink;
  AVDictionary *options_dict = NULL;
  uint8_t options_str[1024];
  uint8_t ch_layout[64];
  int err;
  char errstr[256];

  /* Create a new filtergraph, which will contain all the filters. */
  filter_graph = avfilter_graph_alloc();
  if (!filter_graph) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Unable to create filter graph.\n"ANSI_COLOR_RESET);
    return AVERROR(ENOMEM);
  }
  
  /* Create the abuffer filter;
   * it will be used for feeding the data into the graph. */
  abuffer = avfilter_get_by_name("abuffer");
   if (abuffer == NULL) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not find the abuffer filter.\n"ANSI_COLOR_RESET);
    return AVERROR_FILTER_NOT_FOUND;
  }
  abuffer_ctx = avfilter_graph_alloc_filter(filter_graph, abuffer, "Fsrc");
  if (abuffer_ctx == NULL) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not allocate the abuffer instance.\n"ANSI_COLOR_RESET);
    return AVERROR(ENOMEM);
  }
  
  /* Set the filter options through the AVOptions API. */
  av_get_channel_layout_string(ch_layout, sizeof(ch_layout), 0, AV_CH_LAYOUT_MONO);
  err = av_opt_set(abuffer_ctx, "channel_layout", ch_layout, AV_OPT_SEARCH_CHILDREN);
  err = av_opt_set(abuffer_ctx, "sample_fmt", av_get_sample_fmt_name(AV_SAMPLE_FMT_FLT), AV_OPT_SEARCH_CHILDREN);
  err = av_opt_set_int(abuffer_ctx, "sample_rate", 5512, AV_OPT_SEARCH_CHILDREN);
  /* Now initialize the filter; we pass NULL options, since we have already
   * set all the options above. */
  err = avfilter_init_str(abuffer_ctx, NULL);
  if (err < 0) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR Could not initialize the abuffer filter.\n"ANSI_COLOR_RESET);
    return err;
  }

  asetnsamples = avfilter_get_by_name("asetnsamples");
  if (!asetnsamples) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR Could not find the ashowinfo filter.\n"ANSI_COLOR_RESET);
    return AVERROR_FILTER_NOT_FOUND;
  }
  asetnsamples_ctx = avfilter_graph_alloc_filter(filter_graph, asetnsamples, "asetnsamples");
  if (!asetnsamples_ctx) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR Could not allocate the asetnsamples instance.\n"ANSI_COLOR_RESET);
    return AVERROR(ENOMEM);
  }

  err = avfilter_init_str(asetnsamples_ctx,"n=2048");
  if (err < 0) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not initialize the asetnsamples instance.\n"ANSI_COLOR_RESET);
    return err;
  }
  if(DEBUG){
  ashowinfo = avfilter_get_by_name("ashowinfo");
  if (!ashowinfo) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not find the ashowinfo filter.\n"ANSI_COLOR_RESET);
    return AVERROR_FILTER_NOT_FOUND;
  }
  ashowinfo_ctx = avfilter_graph_alloc_filter(filter_graph, ashowinfo, "ashowinfo_windowing");
  if (!ashowinfo_ctx) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not allocate the ashowinfo instance.\n");
    return AVERROR(ENOMEM);
  }
  err = avfilter_init_str(ashowinfo_ctx, NULL);
  if (err < 0) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not initialize the ashowinfo instance.\n"ANSI_COLOR_RESET);
    return err;
  }
  }
  /* Finally create the abuffersink filter;
   * it will be used to get the filtered data out of the graph. */
  abuffersink = avfilter_get_by_name("abuffersink");
  if (!abuffersink) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not find the abuffersink filter.\n"ANSI_COLOR_RESET);
    return AVERROR_FILTER_NOT_FOUND;
  }
  abuffersink_ctx = avfilter_graph_alloc_filter(filter_graph, abuffersink, "Fsink");
  if (!abuffersink_ctx) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not allocate the abuffersink instance.\n"ANSI_COLOR_RESET);
    return AVERROR(ENOMEM);
  }
  /* This filter takes no options. */
  err = avfilter_init_str(abuffersink_ctx, NULL);
  if (err < 0) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not initialize the abuffersink instance.\n"ANSI_COLOR_RESET);
    return err;
  }

  /* Link all filters together now */
  err = avfilter_link(abuffer_ctx, 0, asetnsamples_ctx, 0);
  if(err < 0) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR:  connecting filters abuffer and asetnsamples %d error\n"ANSI_COLOR_RESET,err);
    return err;
  }
  if(DEBUG){
    err = avfilter_link(asetnsamples_ctx, 0, ashowinfo_ctx, 0);
    if(err < 0) {
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR:  connecting filters asetnsamples and ashowinfo %d error\n"ANSI_COLOR_RESET,err);
      return err;
    }
    err = avfilter_link(ashowinfo_ctx, 0, abuffersink_ctx, 0);
    if (err < 0) {
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR:  connecting filters ashowinfo and buffersink %d error\n"ANSI_COLOR_RESET,err);
      return err;
    }
  }
  else {
    err = avfilter_link(asetnsamples_ctx, 0, abuffersink_ctx, 0);
    if (err < 0) {
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR:  connecting filters asetnsamples and buffersink %d error\n"ANSI_COLOR_RESET,err);
      return err;
    }
  }
  /* Configure the graph. */
  err = avfilter_graph_config(filter_graph, NULL);
  if (err < 0) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR:  configuring the filter graph\n"ANSI_COLOR_RESET);
    return err;
  }
  *graph = filter_graph;
  *src   = abuffer_ctx;
  *sink  = abuffersink_ctx;
  return 0;
}

static int init_filter_graph_hanning(AVFilterGraph **graph, AVFilterContext **src, AVFilterContext **sink){
  AVFilterGraph *filter_graph;
  AVFilterContext *abuffer_ctx;
  AVFilter        *abuffer;
  AVFilterContext *afftfilt_ctx;
  AVFilter        *afftfilt;
  AVFilterContext *ashowinfo_ctx;
  AVFilter        *ashowinfo;
  AVFilterContext *abuffersink_ctx;
  AVFilter        *abuffersink;
  AVDictionary *options_dict = NULL;
  uint8_t options_str[1024];
  uint8_t ch_layout[64];
  int err;
  char errstr[256];

  /* Create a new filtergraph, which will contain all the filters. */
  filter_graph = avfilter_graph_alloc();
  if (!filter_graph) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Unable to create filter graph.\n"ANSI_COLOR_RESET);
    return AVERROR(ENOMEM);
  }
  
  /* Create the abuffer filter;
   * it will be used for feeding the data into the graph. */
  abuffer = avfilter_get_by_name("abuffer");
   if (abuffer == NULL) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not find the abuffer filter.\n"ANSI_COLOR_RESET);
    return AVERROR_FILTER_NOT_FOUND;
  }
  abuffer_ctx = avfilter_graph_alloc_filter(filter_graph, abuffer, "Wsrc");
  if (abuffer_ctx == NULL) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not allocate the abuffer instance.\n"ANSI_COLOR_RESET);
    return AVERROR(ENOMEM);
  }
  
  /* Set the filter options through the AVOptions API. */
  av_get_channel_layout_string(ch_layout, sizeof(ch_layout), 0, AV_CH_LAYOUT_MONO);
  av_opt_set(abuffer_ctx, "channel_layout", ch_layout, AV_OPT_SEARCH_CHILDREN);
  av_opt_set(abuffer_ctx, "sample_fmt", av_get_sample_fmt_name(AV_SAMPLE_FMT_FLT), AV_OPT_SEARCH_CHILDREN);
  av_opt_set_int(abuffer_ctx, "sample_rate", 5512, AV_OPT_SEARCH_CHILDREN);

  /* Now initialize the filter; we pass NULL options, since we have already
   * set all the options above. */
  err = avfilter_init_str(abuffer_ctx, NULL);
  if (err < 0) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not initialize the abuffer filter.\n"ANSI_COLOR_RESET);
    return err;
  }

  afftfilt = avfilter_get_by_name("afftfilt");
   if (afftfilt == NULL) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not find the abuffer filter.\n"ANSI_COLOR_RESET);
    return AVERROR_FILTER_NOT_FOUND;
  }
  afftfilt_ctx = avfilter_graph_alloc_filter(filter_graph, afftfilt, "Wfft");
  if (afftfilt_ctx == NULL) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not allocate the abuffer instance.\n"ANSI_COLOR_RESET);
    return AVERROR(ENOMEM);
  }

  /* Now initialize the filter; we pass NULL options, since we have already
   * set all the options above. */
  err = avfilter_init_str(afftfilt_ctx,"win_size='w2048':win_func='hann'");
  if (err < 0) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not initialize the abuffer filter.\n"ANSI_COLOR_RESET);
    return err;
  }

  if(DEBUG){
    ashowinfo = avfilter_get_by_name("ashowinfo");
    if (!ashowinfo) {
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not find the ashowinfo filter.\n"ANSI_COLOR_RESET);
    return AVERROR_FILTER_NOT_FOUND;
    }
    ashowinfo_ctx = avfilter_graph_alloc_filter(filter_graph, ashowinfo, "ashowinfo_hanning");
    if (!ashowinfo_ctx) {
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not allocate the ashowinfo instance.\n"ANSI_COLOR_RESET);
      return AVERROR(ENOMEM);
    }
    err = avfilter_init_str(ashowinfo_ctx, NULL);
    if (err < 0) {
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not initialize the ashowinfo instance.\n"ANSI_COLOR_RESET);
      return err;
    }
  }

  /* Finally create the abuffersink filter;
   * it will be used to get the filtered data out of the graph. */
  abuffersink = avfilter_get_by_name("abuffersink");
  if (!abuffersink) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not find the abuffersink filter.\n"ANSI_COLOR_RESET);
    return AVERROR_FILTER_NOT_FOUND;
  }
  abuffersink_ctx = avfilter_graph_alloc_filter(filter_graph, abuffersink, "Wsink");
  if (!abuffersink_ctx) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not allocate the abuffersink instance.\n"ANSI_COLOR_RESET);
    return AVERROR(ENOMEM);
  }
  /* This filter takes no options. */
  err = avfilter_init_str(abuffersink_ctx, NULL);
  if (err < 0) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Could not initialize the abuffersink instance.\n"ANSI_COLOR_RESET);
    return err;
  }

  /* Link all filters together now */
  if(DEBUG){
    err = avfilter_link(abuffer_ctx, 0, ashowinfo_ctx, 0);
    if(err < 0) {
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR:  connecting filters abuffer and fftfilt %d error\n"ANSI_COLOR_RESET,err);
      return err;
    }
    err = avfilter_link(ashowinfo_ctx, 0, afftfilt_ctx, 0);
    if (err < 0) {
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR:  connecting filters ashowinfo and buffersink %d error\n"ANSI_COLOR_RESET,err);
      return err;
    }
    err = avfilter_link(afftfilt_ctx, 0, abuffersink_ctx, 0);
    if (err < 0) {
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR:  connecting filters ashowinfo and buffersink %d error\n"ANSI_COLOR_RESET,err);
      return err;
    }
  } 
  else{
    err = avfilter_link(abuffer_ctx, 0, afftfilt_ctx, 0);
    if (err < 0) {
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR:  connecting filters ashowinfo and buffersink %d error\n"ANSI_COLOR_RESET,err);
      return err;
    }
    err = avfilter_link(afftfilt_ctx, 0, abuffersink_ctx, 0);
    if (err < 0) {
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR:  connecting filters ashowinfo and buffersink %d error\n"ANSI_COLOR_RESET,err);
      return err;
    }
  }
  
  /* Configure the graph. */
  err = avfilter_graph_config(filter_graph, NULL);
  if (err < 0) {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR:  configuring the filter graph\n"ANSI_COLOR_RESET);
    return err;
  }
  av_buffersink_set_frame_size(abuffersink_ctx,2048);
  *graph = filter_graph;
  *src   = abuffer_ctx;
  *sink  = abuffersink_ctx;
  return 0;
}

/*
 * Initialises decoder
  */ 
int init_decoder(char *filename1,char *filename2)
{
  int err;
  if(filename1 == NULL || filename2 == NULL){
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Unable to initialise decoder. Filename null\n"ANSI_COLOR_RESET);
    return -1;
  }
  else if(filename1 != NULL && filename2 != NULL) {
    orig_video_name = filename1;
    edited_video_name = filename2;

    err = open_input_file(0,0); 
    if(err != 0){
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Not able to open original input file %s\n"ANSI_COLOR_RESET,orig_video_name);
      return err;
    }

    err = open_input_file(1,0); 
    if(err != 0){
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Not able to open original input file %s\n"ANSI_COLOR_RESET,edited_video_name);
      return err;
    }
  }
  
  return 0; 
}

int init_filter_graphs(uint8_t select){

  if(select > 1){
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: %s %d for select invalid\nonly two values 0\toriginal\n1\tedited\n"ANSI_COLOR_RESET,__FUNCTION__,select);
    exit(1);
  }
  else if(select == 0){//initialise filter graph
    
  }
}

/*
Sets parameters for current video by calling function init_input_parameters on it
args: select 0 for original video 1 for edited video
*/
void set_current_processing_video(uint8_t select){ 
  if(select == 0){
    fprintf(stdout,ANSI_COLOR_DEBUG"DEBUG: Processing original video\n"ANSI_COLOR_RESET);
    init_input_parameters(first_frame_orig,dec_ctx_orig,fmt_ctx_orig);
    fmt_ctx = fmt_ctx_orig;
    dec_ctx = dec_ctx_orig;
    pts_first_frame = pts_first_frame_orig;
    idx = 0;
    current_select = 0;
    //initialise filters
    init_filter_graph_resample(&graph_resample, &src, &sink);
    init_filter_graph_framing_2048(&graph_frame_2048, &Fsrc, &Fsink);
    init_filter_graph_hanning(&graph_hann, &Wsrc, &Wsink);
  }
  else if(select == 1){
    fprintf(stdout,ANSI_COLOR_DEBUG"DEBUG: Processing edited video\n"ANSI_COLOR_RESET);
    init_input_parameters(first_frame_edited,dec_ctx_edited,fmt_ctx_edited);
    fmt_ctx = fmt_ctx_edited;
    dec_ctx = dec_ctx_edited;
    pts_first_frame = pts_first_frame_edited;
    idx = 0;
    current_select = 1;
    //initialise filters
    init_filter_graph_resample(&graph_resample, &src, &sink);
    init_filter_graph_framing_2048(&graph_frame_2048, &Fsrc, &Fsink);
    init_filter_graph_hanning(&graph_hann, &Wsrc, &Wsink);
  }
  else {
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: %s: Invalid value %d of video select flag. It can be either 0 or 1\n"ANSI_COLOR_RESET,__FUNCTION__,select);
    return;
  }
}

/*
Initializes input parameters based upon input frame
*/
void init_input_parameters(AVFrame *frame,AVCodecContext *dec_ctx,AVFormatContext *fmt_ctx)
{
  if(frame == NULL){ 
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR:Frame is NULL\n"ANSI_COLOR_RESET);
    return;
  }
  if(dec_ctx == NULL){
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR:AVCodec context NULL\n"ANSI_COLOR_RESET);
    return;
  }
    
  INPUT_CHANNEL_LAYOUT = frame->channel_layout;
  INPUT_SAMPLERATE = frame->sample_rate;
  INPUT_SAMPLE_FMT = dec_ctx->sample_fmt;
  INPUT_TIMEBASE = fmt_ctx->streams[audio_stream_index]->time_base;
  if(!INPUT_CHANNEL_LAYOUT || !INPUT_SAMPLERATE || (INPUT_SAMPLE_FMT == -1)){
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR:input parameters not set\n"ANSI_COLOR_RESET);
    return;
  }
  else{
    fprintf(stderr,"DEBUG:input parameters set Sample rate %d Time base %d/%d\nDuration of selected stream %lu\n",INPUT_SAMPLERATE,INPUT_TIMEBASE.num,INPUT_TIMEBASE.den,fmt_ctx->streams[audio_stream_index]->duration);
  }
}

/*
Just to dump debugging information about a frame
*/
void dump_frame_info(struct AVFrame *frame){
    fprintf(stderr,ANSI_COLOR_ERROR"DEBUG: frame->format:%d\n",frame->format);
    fprintf(stderr,ANSI_COLOR_ERROR"DEBUG: frame->width:%d\n",frame->width);
    fprintf(stderr,ANSI_COLOR_ERROR"DEBUG: frame->height:%d\n",frame->height);
    fprintf(stderr,ANSI_COLOR_ERROR"DEBUG: frame->channels:%d\n",frame->channels);
    fprintf(stderr,ANSI_COLOR_ERROR"DEBUG: frame->channel_layout:%d\n",frame->channel_layout);
    fprintf(stderr,ANSI_COLOR_ERROR"DEBUG: frame->nb_samples:%d\n",frame->nb_samples);
}


/*
Opens input file and sets,initialises important context and parameters
args: file_select Selects name of the file to be opened from orig_video_name(0) and edited_video_name(1)
 */
int open_input_file(uint8_t file_select,uint8_t language)
{
  int ret;
  AVCodec *dec = NULL;
  AVDictionaryEntry *tag = NULL;
  // this parameter needs to be set taking parameters from CLI, taking language number 1 or 2.
  uint8_t i,j,got_frame;
  char *filename;
  AVFrame *frame = av_frame_alloc();
  int err,len;
  char errstr[128];
  AVPacket pkt;
  av_register_all();
  avfilter_register_all();
  //open input video file
  if(file_select == 0){
    filename = orig_video_name; 
    if((ret = avformat_open_input(&fmt_ctx_orig,orig_video_name,NULL,NULL)) < 0){
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Unable to open %s\n"ANSI_COLOR_RESET,filename);
    return ret;
    }
    fmt_ctx = fmt_ctx_orig;
  }
  else if(file_select == 1){
    filename = edited_video_name;
    if((ret = avformat_open_input(&fmt_ctx_edited,edited_video_name,NULL,NULL)) < 0){
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Unable to open %s\n"ANSI_COLOR_RESET,filename);
    return ret;
    }
    fmt_ctx = fmt_ctx_edited;
  }
  else{
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Invalid value for file_select flag\n"ANSI_COLOR_RESET);
    return -1;
  }
  
  //print useful format information
  printf("Opening format:%s\nFile:%s\nTotal %d streams in video\n",fmt_ctx->iformat->name,filename,fmt_ctx->nb_streams);
  if((ret == avformat_find_stream_info(fmt_ctx,NULL)) < 0){
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR:Unable to find stream info\n"ANSI_COLOR_RESET);
    return ret;
  }
  //select a default audio stream index
  ret = av_find_best_stream(fmt_ctx,AVMEDIA_TYPE_AUDIO, -1, -1, &dec, 0);
  if(ret < 0){
    fprintf(stderr,ANSI_COLOR_ERROR"Cannot find audio stream in input\n"ANSI_COLOR_RESET);
    return ret;
  }
  else if(dec == NULL)
    fprintf(stderr,ANSI_COLOR_ERROR"Audio decoder not found\n"ANSI_COLOR_RESET);
  audio_stream_index = ret; 
  fprintf(stdout,"DEBUG: Audio stream %d selected by av_find_best_stream\n",audio_stream_index);

  //support for multiple audio streams(for multiple languages possibly) if any in video;
  //if language parameter is not set by default eng is choosen else first stream is selected;
  for(i = 0; i < fmt_ctx->nb_streams;i++){
    tag = NULL;
    if(fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
      tag = av_dict_get(fmt_ctx->streams[i]->metadata, "language", tag, AV_DICT_IGNORE_SUFFIX);
      if(tag != NULL && strcmp(tag->value,"eng") == 0){ //language unset
	if(language == 0){
	  audio_stream_index = i;
	  break;
	}
      }
      else{
	audio_stream_index = i;
	break;
      }
    }
  }
  fprintf(stdout,"DEBUG: Audio stream %d selected by loop\n",audio_stream_index);

  /* init audio decoder */

  /* select audio stream and initialise decoder*/
  if(file_select == 0){
    dec_ctx_orig = fmt_ctx->streams[audio_stream_index]->codec;
    if((ret = avcodec_open2(dec_ctx_orig, dec, NULL)) < 0) {
      fprintf(stderr,ANSI_COLOR_ERROR"Cannot open audio decoder\n"ANSI_COLOR_RESET);
      return ret;
    }

    dec_ctx = dec_ctx_orig;
    fprintf(stdout,"DEBUG: Selected audio stream:%d for original video\n",audio_stream_index);
  }
  else {
    dec_ctx_edited = fmt_ctx->streams[audio_stream_index]->codec;
    if((ret = avcodec_open2(dec_ctx_edited, dec, NULL)) < 0) {
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Cannot open audio decoder\n"ANSI_COLOR_RESET);
      return ret;
    }

    dec_ctx = dec_ctx_edited;
    fprintf(stdout,"DEBUG: Selected audio stream:%d for edited video\n",audio_stream_index);
  }

  while(1){ //read an audio frame for format specifications
    ret = av_read_frame(fmt_ctx, &pkt);
    if(ret < 0){
      fprintf(stderr,ANSI_COLOR_ERROR"Unable to read frame:%d \n"ANSI_COLOR_RESET,ret);
      break;
    }
    if(pkt.stream_index == audio_stream_index){ 
      got_frame = 0;
      len = avcodec_decode_audio4(dec_ctx, frame, &got_frame, &pkt);
      if(len < 0){
	av_strerror(len,errstr,128);
	fprintf(stderr,ANSI_COLOR_ERROR"ERROR %s %d %s\n"ANSI_COLOR_RESET,__FUNCTION__,__LINE__,errstr);
      }	
      else if(got_frame)
	break;
    }
  }
  
  fprintf(stdout,"DEBUG: Time base unit:AVStream->time_base: %lu/%lu\nFrame rate %lu/%lu\n",fmt_ctx->streams[audio_stream_index]->time_base.num,fmt_ctx->streams[audio_stream_index]->time_base.den,dec_ctx->framerate.num,dec_ctx->framerate.den); //dec_ctx doesn't shows values
  fprintf(stdout,"DEBUG: Start time in timebase units %lu\n",fmt_ctx->streams[audio_stream_index]->start_time);
  fprintf(stdout,"DEBUG: First read frame PTS:%lu\n",frame->pts);
  
  if(file_select == 0){
    pts_first_frame_orig = frame->pts;
    first_frame_orig = frame;
  }
  else {
    pts_first_frame_edited = frame->pts;
    first_frame_edited = frame;
  }
  return 0;      
}

/*
Read frame sequence covering 1.5 seconds from time given and buffers those frame.
Works like loop traversing frames to frames for a duration of 1.5 seconds. Resampling 
function is called on frame itself then.

arg: time_to_seek_ms Time in mili seconds to get frame from
arg: index           Index of subtitle block
*/
void create_fingerprint_by_pts(uint16_t index,int64_t time_to_seek_ms)
{
  //  int64_t start_pts = av_rescale(time_to_seek_ms, INPUT_TIMEBASE.den,INPUT_TIMEBASE.num) ;
  //  start_pts = start_pts/1000;
  //  start_pts += pts_first_frame;
  //  time_to_seek_ms += GRANUALITY;
  int64_t start_pts = pts_first_frame;
  int64_t granuality = av_rescale(GRANUALITY/1000, INPUT_TIMEBASE.den,INPUT_TIMEBASE.num);
  //  end_pts += pts_first_frame;
  int64_t end_pts = start_pts + granuality;
  int i = 0,j, count = 0,temp = 0,out_count = 0,in_count = 0;
  uint8_t *OUTPUT_SAMPLES = NULL,frame_count=0;
  AVPacket pkt;
  uint8_t *in;
  int got_frame,ret=0;
  AVFrame *frame = av_frame_alloc();
  AVFrame *frame_2048 = av_frame_alloc();
  double *log_bins=NULL;
  double *log_bins_array = (double *) malloc(sizeof(double) * 128 * 32);
  int log_bins_arr_index = 0;
  int copy_size = 32 * sizeof(double);
  const struct AVFrame *frame_peek1[32];
  AVFrame *frame_copy = av_frame_alloc();
  int size,len,buf_size;
  uint64_t output_ch_layout = av_get_channel_layout("mono");
  enum AVSampleFormat src_sample_fmt;
  uint8_t *output_buffer = NULL;
  int err;
  char errstr[128];
  fftw_complex *out=NULL;
  struct FFBufQueue *frame_queue_32 = (struct FFBufQueue *) malloc(sizeof(struct FFBufQueue));
  uint8_t *result=NULL;
  uint64_t *res_array = NULL;
  frame_queue_32->available = 0;
  if(end_pts > (fmt_ctx->streams[audio_stream_index]->duration + pts_first_frame)){
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: End PTS(%lu) greater then duration of stream(%lu)\n"ANSI_COLOR_RESET,end_pts,fmt_ctx->streams[audio_stream_index]->duration);
    return;
  }

  while(end_pts < (fmt_ctx->streams[audio_stream_index]->duration + pts_first_frame)){
    ret = av_seek_frame(fmt_ctx,audio_stream_index, start_pts, AVSEEK_FLAG_BACKWARD); //get one frame before timing to cover all
    
    // trying avformat_seek_file instead of av_seek_frame
    //  ret = avformat_seek_file(fmt_ctx,audio_stream_index,0,start_pts,start_pts,AVSEEK_FLAG_FRAME);
    if(ret < 0){
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: avformat_seek_file failed with error code %d\n"ANSI_COLOR_RESET,ret);
      return;
    }
    //    fprintf(stdout,"Start PTS: %lu End PTS: %lu\n",start_pts,end_pts);
    
    do{ //outer do-while to read packets
      ret = av_read_frame(fmt_ctx, &pkt);
      if(ret < 0){
	fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Unable to read frame:%d \n"ANSI_COLOR_RESET,ret);
	break;
      }
      if(pkt.stream_index == audio_stream_index){ // processing audio packets
	size = pkt.size;
	while(size > 0){ // inner while to decode frames, if more than one are present in a single packet
	  got_frame = 0;
	  if(pkt.pts == AV_NOPTS_VALUE){
	    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: No PTS information present in stream\n"ANSI_COLOR_RESET);
	  }
	  if(pkt.duration == 0){
	    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: No duration information present in stream\n"ANSI_COLOR_RESET);
	    return;
	  }
	  
	  len = avcodec_decode_audio4(dec_ctx, frame, &got_frame, &pkt);
	  if(len < 0){
	    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: while decoding\n"ANSI_COLOR_RESET);
	  }
	  if(got_frame){
	    err = av_buffersrc_add_frame(src, frame);
	    i++;
	    if(err < 0) {
	      av_frame_unref(frame);
	      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Failed to add frame to source buffer\n"ANSI_COLOR_RESET);
	      return;
	    }
	    size = size - len;
	  }
	}
      }
    }while(frame->pts < end_pts);
    
    // make frame available for overlapping and windowing
    // frame is size of 64 samples already.
    // feed 32 frames to filter_graph_window and collect one 2048 sample frame out of it.
    // This process is repeated for every frame thus, 128 times we get 2048 frames.
    // Algo: 1. Fill source buffer with 32 frames
    //       2. Remove one frame and add another and get 2048 frame.
    // Fill source buffer
    while((ret = av_buffersink_get_frame(sink,frame)) >= 0){
      if(frame_count < 32 && frame->nb_samples == 64) { //feed frames to window graph
	//push the frame to bufferqueue 
	ff_bufqueue_add(NULL,frame_queue_32,frame);
	// Uncomment to add debug information about frame
	//     dump_frame_info(frame);
	err = av_buffersrc_add_frame(Fsrc, frame);
	if(err < 0) {
	  av_frame_unref(frame);
	  fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Not able to add frame to source buffer\n"ANSI_COLOR_RESET);
	  break;
	}
	frame_count++;
      }
      else {
	break;
      }
    }
    
    frame_count = 0;
    while(frame_count < 128){
      // use this frame_2048 and fill it in windowing graph
      ret = av_buffersink_get_frame(Fsink,frame_2048);
      if(ret < 0){
	av_strerror(ret,errstr,128);
	fprintf(stderr,ANSI_COLOR_ERROR"ERROR: No frame in buffer sink (Fsink) Frame count %d %s\n"ANSI_COLOR_RESET,frame_count,errstr);
	return ret;
      }
      
      ret = av_buffersink_get_frame(sink,frame);
      if(ret < 0){
	fprintf(stderr,ANSI_COLOR_ERROR"ERROR: No more frames in the sink\n"ANSI_COLOR_RESET);
	break;
      }
      //  dump_frame_info(frame);
      //Add one frame from sink to bufferqueue
      ff_bufqueue_get(frame_queue_32);
      ff_bufqueue_add(NULL,frame_queue_32,frame);
      //and peek from 0 - 31 adding to Fsrc
      for(i=0;i<32;i++){
	frame_peek1[i] = ff_bufqueue_peek(frame_queue_32,i);
	if(frame_peek1[i] == NULL) { 
	  fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Queue does not have enough buffers\n"ANSI_COLOR_RESET);
	  return -1;
	}
	//else if not null push the frame to buffersrc Fsrc for framing to 2048
	//   dump_frame_info(frame_peek1[i]);    
	frame_copy->format = frame_peek1[i]->format;
	frame_copy->width = frame_peek1[i]->width;
	frame_copy->height = frame_peek1[i]->height;
	frame_copy->channels = frame_peek1[i]->channels;
	frame_copy->channel_layout = frame_peek1[i]->channel_layout;
	frame_copy->nb_samples = frame_peek1[i]->nb_samples;
	//    dump_frame_info(frame_copy);    
	ret = av_frame_get_buffer(frame_copy, 32);
	if(ret < 0){
	  av_strerror(ret,errstr,128);
	  fprintf(stdout,"DEBUG: av_frame_get_buffer returned %d:\"%s\"\n",ret,errstr);
	  return -1;
	}
	
	ret = av_frame_copy(frame_copy, frame_peek1[i]);
	if(ret < 0){
	  av_strerror(ret,errstr,128);
	  fprintf(stdout,"DEBUG: Frame not copied %d error \"%s\"\n",ret,errstr);
	  return -1;
	}
	
	ret = av_frame_copy_props(frame_copy,frame_peek1[i]);
	if(ret < 0){
	  av_strerror(ret,errstr,128);
	  fprintf(stdout,"DEBUG: Frame metadata not copied properly %d error \"%s\"\n",ret,errstr);
	  return -1;
	}
	
	//NOTE: av_buffersrc_add_frame at last uses av_frame_free on frame supplied. Thus we supply a copy of it. So that we don't loose reference.
	err = av_buffersrc_add_frame(Fsrc, frame_copy);
	if(err < 0){
	  av_strerror(err,errstr,128);
	  fprintf(stdout,"DEBUG: Cannot add frames to Fsrc \"%s\"\n",errstr);
	  break;
	} 
      }
      frame_count++;
      av_buffersrc_add_frame(Wsrc, frame_2048);
    }
    
    // fprintf(stderr,ANSI_COLOR_ERROR"DEBUG: frame_count value %d\n",frame_count);
    init_fft_params();
    //at final graph_hann should contain 128 frames of hanned window 
    for(i=0;i<127;i++){
      ret = av_buffersink_get_frame(Wsink, frame_2048);
      if(ret < 0){
	av_strerror(ret,errstr,128);
	fprintf(stdout,"DEBUG: av_buffersink_get_frame returned %d:\"%s\"\n",ret,errstr);
	return -1;
      } 
      log_bins = process_av_frame(frame_2048);
      if(log_bins == NULL){
	fprintf(stderr,ANSI_COLOR_ERROR"ERROR: process_av_frame unable to process frame\n"ANSI_COLOR_RESET);
	break;
      }
      memcpy(&log_bins_array[log_bins_arr_index],log_bins,copy_size);
      log_bins_arr_index = log_bins_arr_index + 32;
    }
    
    //Finally log_bins_array now contains all 128x32 wavelets from which we will extract top 
    result = extract_top_wavelets(log_bins_array,200);
    fprintf(stdout,ANSI_COLOR_YELLOW"Index %d Fingerprint for time slot: %lu - %lu\n"ANSI_COLOR_RESET,idx,start_pts,end_pts);
    long num = 0;
    res_array = malloc(sizeof(uint64_t) * 25);
    for(i=0,j=0;i<100,j<25;i++){
      if((i+1) % 4 == 0){
	num = ((result[i-3] << 24) | (result[i-2] << 16) | (result[i - 1] << 8) | result[i]);
	//	fprintf(stdout,"%lu\t",num);
	res_array[j] = num;
	j++;
      }
      //      else if(i % 20 == 0)
	//	fprintf(stdout,"\n");
    }
    //    fprintf(stdout,"\n");
    
    if(current_select == 0){
      ret = insert_entry_into_table(res_array, idx);
      if(ret < 0){
	fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Failed to insert entry into table for index %d Error code:%d\n"ANSI_COLOR_RESET,idx,ret);
	return -1;
      }
      //print_tables();
    }
    else if(current_select == 1){
      //search for fingerprint in tables
      printf("Table for index %d\n",idx);
      search_and_match(res_array,idx);
      //print_tables();
    }
    start_pts = end_pts;
    end_pts = end_pts + granuality;
    log_bins_arr_index = 0;
    idx++;
  }

  //  print_tables();
  free(log_bins_array);
  free(log_bins);
  free(res_array);
  free(frame_queue_32);
  av_frame_free(&frame);
  av_frame_free(&frame_2048);
  av_frame_free(&frame_copy);

  return;
}



void close_filter()
{
  avfilter_graph_free(&graph_resample);
  avfilter_graph_free(&graph_frame_2048);
}
