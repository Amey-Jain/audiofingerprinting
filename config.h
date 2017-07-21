#ifndef CONFIG
#define CONFIG
#endif

// Maximum size of charachters for representation of timing of subtitles
#define BUF_SIZE 30
// any changes made here should also be made to add_timestamp_to_array function
#define GRANUALITY 2000 // in miliseconds
//it can handle size of 99:59:59,999
#define LLONG uint32_t 
#define SAMPLE_PER_SUB_FP 2048 //number of samples to be processed for one sub fingerprint
