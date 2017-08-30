#ifndef LSH_DB
#define LSH_DB
#endif

#include<stdint.h>
#include<inttypes.h>

#define NO_OF_ENTRIES_ALLOC 10
#define NO_OF_ENTRIES_VOTES 1000
#define MAX_INDEXES 1500

struct table_entry {
  uint64_t f_value; // value of fingerprint
  uint16_t *indexes; // indexes of fingerprint of file from beginning 0,1,2...
  int index_ctr; // points to last index of entry in indexes
  uint16_t index_max;
}; 

struct table {
  struct table_entry *entries;
  int entries_ctr; // points to last index of entry in original
  uint16_t entries_max;
};
 
struct votes {
  uint16_t indexes[NO_OF_ENTRIES_VOTES];
  uint8_t votes[NO_OF_ENTRIES_VOTES];
  int ctr;
  uint16_t ctr_max;
};

struct lookup_table {
  uint16_t index_no;
  uint16_t entries[25];
  uint64_t start_pts,end_pts;
};
void group_minhash_to_lsh_buckets(uint8_t *min_hash, int number_of_hash_tables); 
void initialise_database(void);
void init_db();
void close_db();
void print_fp_by_index(uint16_t index);
int search_for_entry_in_table(uint64_t entry, int table_no);
int insert_entry_into_table(uint64_t *result,uint16_t index,uint64_t start_pts,uint64_t end_pts);
int search_and_match(uint64_t *result,uint16_t index);
void print_tables();
