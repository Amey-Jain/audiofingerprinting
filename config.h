#ifndef CONFIG
#define CONFIG
#endif

#define BUF_SIZE 30
// any changes made here should also be made to add_timestamp_to_array function
#define GRANUALITY 2000 // in miliseconds
//it can handle size of 99:59:59,999
#define LLONG uint32_t 
#define SAMPLE_PER_SUB_FP 2048 //number of samples to be processed for one sub fingerprint

#define ANSI_COLOR_ERROR     "\x1b[31m"
#define ANSI_COLOR_DEBUG   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define DEBUG 0
 
