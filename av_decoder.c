/* File to extract audio from video   */
#include <stdio.h>
//#include <libavfilter/avcodec.h>
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
#include "av_decoder.h"
#include "config.h"

static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *dec_ctx;
static int audio_stream_index = -1;
static AVFilterGraph *graph_resample=NULL;
static AVFilterGraph *graph_window=NULL;
static AVFilterContext *src=NULL,*sink=NULL;
static AVFilterContext *Wsrc=NULL,*Wsink=NULL;
enum AVSampleFormat INPUT_SAMPLE_FMT = -1;
uint64_t INPUT_SAMPLERATE = 0;
uint64_t INPUT_CHANNEL_LAYOUT = 0;
AVRational INPUT_TIMEBASE;
char *orig_video_name = NULL;
char *edited_video_name = NULL;

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
  AVFilterContext *ashowinfo2_ctx;
  AVFilter        *ashowinfo2;

  AVDictionary *options_dict = NULL;
  uint8_t options_str[1024];
  uint8_t ch_layout[64];
  int err;
  char errstr[256];
  /* Create a new filtergraph, which will contain all the filters. */
  filter_graph = avfilter_graph_alloc();
  if (!filter_graph) {
    fprintf(stderr, "Unable to create filter graph.\n");
    return AVERROR(ENOMEM);
  }
  
  /* Create the abuffer filter;
   * it will be used for feeding the data into the graph. */
  abuffer = avfilter_get_by_name("abuffer");
  if (abuffer == NULL) {
    fprintf(stderr, "Could not find the abuffer filter.\n");
    return AVERROR_FILTER_NOT_FOUND;
  }
  abuffer_ctx = avfilter_graph_alloc_filter(filter_graph, abuffer, "src");
  if (abuffer_ctx == NULL) {
    fprintf(stderr, "Could not allocate the abuffer instance.\n");
    return AVERROR(ENOMEM);
  }
  
  /* Set the filter options through the AVOptions API. */
  av_get_channel_layout_string(ch_layout, sizeof(ch_layout), 0, INPUT_CHANNEL_LAYOUT);
  err = av_opt_set(abuffer_ctx, "channel_layout", ch_layout, AV_OPT_SEARCH_CHILDREN);
  fprintf(stderr,"DEBUG: av_opt_set for channel_layout returned %d %s\n",err);
  err = av_opt_set(abuffer_ctx, "sample_fmt", av_get_sample_fmt_name(INPUT_SAMPLE_FMT), AV_OPT_SEARCH_CHILDREN);
  fprintf(stderr,"DEBUG: av_opt_set for sample_fmt returned %d\n",err);
  err = av_opt_set_q(abuffer_ctx, "time_base", (AVRational){ 1, INPUT_SAMPLERATE }, AV_OPT_SEARCH_CHILDREN);
  fprintf(stderr,"DEBUG: av_opt_set for time_base returned %d\n",err);
  err = av_opt_set_int(abuffer_ctx, "sample_rate", INPUT_SAMPLERATE, AV_OPT_SEARCH_CHILDREN);
  fprintf(stderr,"DEBUG: av_opt_set for channel_layout returned %d\n",err);
  /* Now initialize the filter; we pass NULL options, since we have already
   * set all the options above. */
  err = avfilter_init_str(abuffer_ctx, NULL);
  if (err < 0) {
    fprintf(stderr, "Could not initialize the abuffer filter.\n");
    return err;
  }
  
  // Create resampling filter. 
  aformat = avfilter_get_by_name("aformat");
  if (!aformat) {
    fprintf(stderr, "Could not find the aformat filter.\n");
    return AVERROR_FILTER_NOT_FOUND;
  }
  aformat_ctx = avfilter_graph_alloc_filter(filter_graph, aformat, "aformat");
  if (!aformat_ctx) {
    fprintf(stderr, "Could not allocate the resample instance.\n");
    return AVERROR(ENOMEM);
  }
  // option settings in aformat filter
  snprintf(options_str, sizeof(options_str),"sample_fmts=%s:sample_rates=%d:channel_layouts=0x%"PRIx64, av_get_sample_fmt_name(AV_SAMPLE_FMT_FLT), 5512, (uint64_t)AV_CH_LAYOUT_MONO);
  err = avfilter_init_str(aformat_ctx, options_str);
  if (err < 0) {
    fprintf(stderr, "Could not initialize the resampling filter.\n");
    return err;
  }

  ashowinfo = avfilter_get_by_name("ashowinfo");
  if (!ashowinfo) {
    fprintf(stderr, "Could not find the ashowinfo filter.\n");
    return AVERROR_FILTER_NOT_FOUND;
  }
  ashowinfo_ctx = avfilter_graph_alloc_filter(filter_graph, ashowinfo, "ashowinfo");
  if (!ashowinfo_ctx) {
    fprintf(stderr, "Could not allocate the ashowinfo instance.\n");
    return AVERROR(ENOMEM);
  }
  err = avfilter_init_str(ashowinfo_ctx, NULL);
  if (err < 0) {
    fprintf(stderr, "Could not initialize the ashowinfo instance.\n");
    return err;
  }
    
  /* Finally create the abuffersink filter;
   * it will be used to get the filtered data out of the graph. */
  abuffersink = avfilter_get_by_name("abuffersink");
  if (!abuffersink) {
    fprintf(stderr, "Could not find the abuffersink filter.\n");
    return AVERROR_FILTER_NOT_FOUND;
  }
  abuffersink_ctx = avfilter_graph_alloc_filter(filter_graph, abuffersink, "sink");
  if (!abuffersink_ctx) {
    fprintf(stderr, "Could not allocate the abuffersink instance.\n");
    return AVERROR(ENOMEM);
  }
  /* This filter takes no options. */
  err = avfilter_init_str(abuffersink_ctx, NULL);
  if (err < 0) {
    fprintf(stderr, "Could not initialize the abuffersink instance.\n");
    return err;
  }
  
  /* Connect the filters;
   * in this simple case the filters just form a linear chain. */
  err = avfilter_link(abuffer_ctx, 0, aformat_ctx, 0);
  if(err < 0) {
    fprintf(stderr, "Error connecting filters abuffer and aformat %d error\n",err);
    return err;
  }
  err = avfilter_link(aformat_ctx, 0, ashowinfo_ctx, 0);
  if (err < 0) {
    fprintf(stderr,"Error connecting filters aformat and ashowinfo %d error\n",err);
    return err;
  } 
  err = avfilter_link(ashowinfo_ctx, 0, abuffersink_ctx, 0);
  if (err < 0) {
    fprintf(stderr, "Error connecting filters ashowinfo and buffersink %d error\n",err);
    return err;
  }
    
  /* Configure the graph. */
  err = avfilter_graph_config(filter_graph, NULL);
  if (err < 0) {
    fprintf(stderr, "Error configuring the filter graph\n");
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
static int init_filter_graph_windowing(AVFilterGraph **graph, AVFilterContext **src, AVFilterContext **sink)
{
  AVFilterGraph *filter_graph;
  AVFilterContext *abuffer_ctx;
  AVFilter        *abuffer;
  AVFilterContext *fftfilt_ctx;
  AVFilter        *fftfilt;
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
    fprintf(stderr, "Unable to create filter graph.\n");
    return AVERROR(ENOMEM);
  }
  
  /* Create the abuffer filter;
   * it will be used for feeding the data into the graph. */
  abuffer = avfilter_get_by_name("abuffer");
   if (abuffer == NULL) {
    fprintf(stderr, "Could not find the abuffer filter.\n");
    return AVERROR_FILTER_NOT_FOUND;
  }
  abuffer_ctx = avfilter_graph_alloc_filter(filter_graph, abuffer, "src");
  if (abuffer_ctx == NULL) {
    fprintf(stderr, "Could not allocate the abuffer instance.\n");
    return AVERROR(ENOMEM);
  }
  
  /* Set the filter options through the AVOptions API. */
  av_get_channel_layout_string(ch_layout, sizeof(ch_layout), 0, AV_CH_LAYOUT_MONO);
  err = av_opt_set(abuffer_ctx, "channel_layout", ch_layout, AV_OPT_SEARCH_CHILDREN);
  fprintf(stderr,"DEBUG: av_opt_set for channel_layout returned %d %s\n",err);
  err = av_opt_set(abuffer_ctx, "sample_fmt", av_get_sample_fmt_name(AV_SAMPLE_FMT_FLT), AV_OPT_SEARCH_CHILDREN);
  fprintf(stderr,"DEBUG: av_opt_set for sample_fmt returned %d\n",err);
  err = av_opt_set_int(abuffer_ctx, "sample_rate", 5512, AV_OPT_SEARCH_CHILDREN);
  fprintf(stderr,"DEBUG: av_opt_set for channel_layout returned %d\n",err);
  /* Now initialize the filter; we pass NULL options, since we have already
   * set all the options above. */
  err = avfilter_init_str(abuffer_ctx, NULL);
  if (err < 0) {
    fprintf(stderr, "Could not initialize the abuffer filter.\n");
    return err;
  }
  
  fftfilt = avfilter_get_by_name("afftfilt");
  if (!fftfilt) {
    fprintf(stderr, "Could not find the fftfilt filter.\n");
    return AVERROR_FILTER_NOT_FOUND;
  }
  fftfilt_ctx = avfilter_graph_alloc_filter(filter_graph, fftfilt, "afftfilt");
  if (!fftfilt_ctx) {
    fprintf(stderr, "Could not allocate the fftfilt instance.\n");
    return AVERROR(ENOMEM);
  }
  snprintf(options_str, sizeof(options_str),"win_size=%s:win_func=%s:overlap=%f","w2048","hanning",0.96875);
  err = avfilter_init_str(fftfilt_ctx, options_str);
  if (err < 0) {
    fprintf(stderr, "Could not initialize the fftfilt filter.\n");
    return err;
  }

  ashowinfo = avfilter_get_by_name("ashowinfo");
  if (!ashowinfo) {
    fprintf(stderr, "Could not find the ashowinfo filter.\n");
    return AVERROR_FILTER_NOT_FOUND;
  }
  ashowinfo_ctx = avfilter_graph_alloc_filter(filter_graph, ashowinfo, "Hanning window");
  if (!ashowinfo_ctx) {
    fprintf(stderr, "Could not allocate the ashowinfo instance.\n");
    return AVERROR(ENOMEM);
  }
  err = avfilter_init_str(ashowinfo_ctx, NULL);
  if (err < 0) {
    fprintf(stderr, "Could not initialize the ashowinfo instance.\n");
    return err;
  }
    
  /* Finally create the abuffersink filter;
   * it will be used to get the filtered data out of the graph. */
  abuffersink = avfilter_get_by_name("abuffersink");
  if (!abuffersink) {
    fprintf(stderr, "Could not find the abuffersink filter.\n");
    return AVERROR_FILTER_NOT_FOUND;
  }
  abuffersink_ctx = avfilter_graph_alloc_filter(filter_graph, abuffersink, "sink");
  if (!abuffersink_ctx) {
    fprintf(stderr, "Could not allocate the abuffersink instance.\n");
    return AVERROR(ENOMEM);
  }
  /* This filter takes no options. */
  err = avfilter_init_str(abuffersink_ctx, NULL);
  if (err < 0) {
    fprintf(stderr, "Could not initialize the abuffersink instance.\n");
    return err;
  }

  /* Link all filters together now */
  err = avfilter_link(abuffer_ctx, 0, fftfilt_ctx, 0);
  if(err < 0) {
    fprintf(stderr, "Error connecting filters abuffer and fftfilt %d error\n",err);
    return err;
  }
  err = avfilter_link(fftfilt_ctx, 0, ashowinfo_ctx, 0);
  if (err < 0) {
    fprintf(stderr,"Error connecting filters aformat and ashowinfo %d error\n",err);
    return err;
  }
  err = avfilter_link(ashowinfo_ctx, 0, abuffersink_ctx, 0);
  if (err < 0) {
    fprintf(stderr, "Error connecting filters ashowinfo and buffersink %d error\n",err);
    return err;
  }

  /* Configure the graph. */
  err = avfilter_graph_config(filter_graph, NULL);
  if (err < 0) {
    fprintf(stderr, "Error configuring the filter graph\n");
    return err;
  }
  *graph = filter_graph;
  *src   = abuffer_ctx;
  *sink  = abuffersink_ctx;
  return 0;

}

/*
 * Initialises decoder
 * To select only orig file pass 1 to file_select flag
 */ 
int init_decoder(char *filename1,char *filename2,uint8_t file_select)
{
  int err;
  if(file_select && filename1 != NULL){
    orig_video_name = filename1;
    err = open_input_file(0);
    if(err != 0){
      fprintf(stderr,"Not able to open input file\n");
      return err;
    }
  }
  else if(filename1 == NULL || filename2 == NULL && !file_select){
    fprintf(stderr,"ERROR: Unable to initialise decoder. Filename null\n");
    return -1;
  }
  else{
    orig_video_name = filename1;
    edited_video_name = filename2;
  } 
  // add any other settings from outer file in this function
  err = init_filter_graph_resample(&graph_resample, &src, &sink);
  if(err != 0){
    fprintf(stderr,"ERROR: Not able to initialize filter_graph for resampling %d\n",err);
    return err;
    }
  else {
    err = init_filter_graph_windowing(&graph_window, &Wsrc, &Wsink);
    if(err < 0){
      fprintf(stderr,"ERROR: Not able to initialise filter_graph for windowing %d\n",err);
      return err;
    }
    fprintf(stderr,"DEBUG: Filter graph initialisation completed\n");
  }
  return 0; 
}

void init_input_parameters(AVFrame *frame,AVCodecContext *dec_ctx)
{
  if(frame == NULL){ 
    fprintf(stderr,"ERROR:Frame is NULL\n");
    return;
  }
  if(dec_ctx == NULL){
    fprintf(stderr,"ERROR:AVCodec context NULL\n");
    return;
  }
    
  INPUT_CHANNEL_LAYOUT = frame->channel_layout;
  INPUT_SAMPLERATE = frame->sample_rate;
  INPUT_SAMPLE_FMT = dec_ctx->sample_fmt;
  INPUT_TIMEBASE = fmt_ctx->streams[audio_stream_index]->time_base;
      if(!INPUT_CHANNEL_LAYOUT || !INPUT_SAMPLERATE || (INPUT_SAMPLE_FMT == -1)){
    fprintf(stderr,"ERROR:input parameters not set\n");
    return;
  }
    else{
      fprintf(stderr,"DEBUG:input parameters set Sample rate %d Time base %d/%d\nDuration of selected stream %lu\n",INPUT_SAMPLERATE,INPUT_TIMEBASE.num,INPUT_TIMEBASE.den,fmt_ctx->streams[audio_stream_index]->duration);
    }
}


/*
Opens input file and sets,initialises important context and parameters
args: file_select Selects name of the file to be opened from orig_video_name(0) and edited_video_name(1)

 */
int open_input_file(uint8_t file_select)
{
  int ret;
  AVCodec *dec = NULL;
  AVDictionaryEntry *tag = NULL;
  // this parameter needs to be set taking parameters from CLI, taking language number 1/2.
  uint8_t language = 0,i,j,got_frame;
  char *filename;
  AVFrame *frame = av_frame_alloc();
  int err,len;
  char errstr[128];
  AVPacket pkt;
  av_register_all();
  avfilter_register_all();
  //open input video file
  if(file_select == 0)
    filename = orig_video_name;
  else if(file_select == 1)
    filename = edited_video_name;
  else{
    fprintf(stderr,"ERROR: Invalid value for file_select flag\n");
    return -1;
  }
  if((ret = avformat_open_input(&fmt_ctx,filename,NULL,NULL)) < 0){
    printf("Unable to open %s\n",filename);
    return ret;
  }

  //print useful format information
  printf("Opening format:%s\nFile:%sTotal %d streams in video\n",fmt_ctx->iformat->name,filename,fmt_ctx->nb_streams);
  if((ret == avformat_find_stream_info(fmt_ctx,NULL)) < 0){
    printf("Unable to find stream info\n");
    return ret;
  }
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
 
  /* select audio stream and initialise decoder*/
  ret = av_find_best_stream(fmt_ctx,AVMEDIA_TYPE_AUDIO, -1, -1, &dec, 0);
  if(ret < 0){
    printf("Cannot find audio stream in input\n");
    return ret;
  }
  else if(dec == NULL)
    printf("Audio decoder not found\n");
  audio_stream_index = ret; 
  dec_ctx = fmt_ctx->streams[audio_stream_index]->codec;
  fprintf(stderr,"DEBUG: Selected audio stream:%d",audio_stream_index);
  fprintf(stderr,"DEBUG: Time base unit:AVStream->time_base: %lu/%lu\nFrame rate %lu/%lu\n",fmt_ctx->streams[audio_stream_index]->time_base.num,fmt_ctx->streams[audio_stream_index]->time_base.den,dec_ctx->framerate.num,dec_ctx->framerate.den);
  fprintf(stderr,"DEBUG: Start time in timebase units %lu\n",fmt_ctx->streams[audio_stream_index]->start_time);
  /* init audio decoder */
  if((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
    printf("Cannot open audio decoder\n");
    return ret;
  }
  while(1){ //read an audio frame for format specifications
    ret = av_read_frame(fmt_ctx, &pkt);
    if(ret < 0){
      printf("Unable to read frame:%d \n",ret);
      break;
    }
    if(pkt.stream_index == audio_stream_index){ 
      got_frame = 0;
      len = avcodec_decode_audio4(dec_ctx, frame, &got_frame, &pkt);
      if(len < 0){
	av_strerror(len,errstr,128);
	fprintf(stderr,"ERROR %s %d %s\n",__FUNCTION__,__LINE__,errstr);
      }	
      else if(got_frame)
	break;
    }
  }
  fprintf(stderr,"DEBUG: First read frame PTS:%lu\n",frame->pts); 
 /* Initialise filters */
 init_input_parameters(frame, dec_ctx);
 av_frame_free(&frame);
 return 0;      
}

/*
Read frame sequence covering 1.5 seconds from time given and buffers those frame.
Works like loop traversing frames to frames for a duration of 1.5 seconds. Resampling 
function is called on frame itself then.

arg: time_to_seek_ms Time in miliseconds to get frame from
arg: index           Index of subtitle block
*/
void process_frame_by_pts(uint16_t index,int64_t time_to_seek_ms)
{
  int64_t start_pts = (time_to_seek_ms/1000) * INPUT_TIMEBASE.den;
  int64_t end_pts = ((time_to_seek_ms + GRANUALITY)/1000) * INPUT_TIMEBASE.den;
  int i = 0, count = 0,temp = 0,out_count = 0,in_count = 0;
  uint8_t *OUTPUT_SAMPLES = NULL,frame_count=0;
  AVPacket pkt;
  uint8_t *in;
  int got_frame,ret=0;
  AVFrame *frame = av_frame_alloc();
  AVFrame *frame_2048 = av_frame_alloc();
  int size,len,buf_size;
  uint64_t output_ch_layout = av_get_channel_layout("mono");
  enum AVSampleFormat src_sample_fmt;
  uint8_t *output_buffer = NULL;
  int err;
  char errstr[128];
  if(end_pts > fmt_ctx->streams[audio_stream_index]->duration){
    printf("Error: End PTS(%lu) greater then duration of stream(%lu)\n",end_pts,fmt_ctx->streams[audio_stream_index]->duration);
    return;
  }
  ret = av_seek_frame(fmt_ctx,audio_stream_index, start_pts,AVSEEK_FLAG_BACKWARD); //get one frame before timing to cover all
  
  //trying avformat_seek_file instead of av_seek_frame
  //ret = avformat_seek_file(fmt_ctx,audio_stream_index,start_seek_target,start_seek_target,fmt_ctx->streams[audio_stream_index]->duration,AVSEEK_FLAG_BACKWARD);
  if( ret < 0 ){
    fprintf(stderr,"avformat_seek_file failed with error code %d\n",ret);
    return;
    }
  fprintf(stdout,"Start PTS: %lu End PTS: %lu\n",start_pts,end_pts);
  
  // Problem is that output gets stored in uint8_t type whereas I want output in float type
  do{ //outer do-while to read packets
    ret = av_read_frame(fmt_ctx, &pkt);
    if(ret < 0){
      printf("Unable to read frame:%d \n",ret);
      break;
    }
    if(pkt.stream_index == audio_stream_index){ // processing audio packets
      size = pkt.size;
      while(size > 0){ // inner while to decode frames, if more than one are present in a single packet
	got_frame = 0;
	if(pkt.pts == AV_NOPTS_VALUE){
	  fprintf(stderr,"ERROR: No PTS information present in stream\n");
	}
	if(pkt.duration == 0){
	  fprintf(stderr,"ERROR: No duration information present in stream\n");
	  return;
	}
	
	len = avcodec_decode_audio4(dec_ctx, frame, &got_frame, &pkt);
	if(len < 0){
	  fprintf(stderr,"Error while decoding\n");
	}
	if(got_frame){
	  err = av_buffersrc_add_frame(src, frame);
	  if(err < 0) {
	    av_frame_unref(frame);
	    fprintf(stderr,"Error adding frame to source buffer\n");
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
    if(frame_count < 64){ //feed frames to window graph
      err = av_buffersrc_add_frame(Wsrc, frame);
      frame_count++;
      if(err < 0) {
	av_frame_unref(frame);
	fprintf(stderr,"Error adding frame to source buffer\n");
	break;
      }
    }
  }

  // Remove a frame and add other while getting 2048 hanning window
    ret = av_buffersink_get_frame(Wsink,frame_2048);
    if(ret < 0){
      av_strerror(ret,errstr,128);
      fprintf(stderr,"No frame in buffer sink (Wsink) %s\n",errstr);
      return ret;
    }
  
  av_frame_free(&frame);
  av_frame_free(&frame_2048);
  return;
}



void close_filter()
{
  avfilter_graph_free(&graph_resample);
  avfilter_graph_free(&graph_window);
}
