#ifndef LSH_DB
#define LSH_DB
#endif

#include<stdint.h>
#include<inttypes.h>

#define MAX_ENTRIES_PER_TABLE 30

struct table_entry {
  uint64_t f_value; // value of fingerprint
  uint16_t indexes[10]; // indexes of fingerprint of file from beginning 0,1,2...
  uint16_t index_ctr; // points to last index of entry in indexes
}; 

struct table {
  struct table_entry entries[MAX_ENTRIES_PER_TABLE];
  uint16_t entries_ctr; // points to last index of entry in original
};
 
struct votes {
  uint16_t indexes[10];
  uint8_t votes[10];
  int ctr;
};

void group_minhash_to_lsh_buckets(uint8_t *min_hash, int number_of_hash_tables); 
void initialise_database(void);
void init_db();
int search_for_entry_in_table_i(uint64_t entry, int table_no);
int insert_result_into_table(uint64_t *result,uint16_t index);
int search_and_match(uint64_t *result,uint16_t index);
void print_tables();
