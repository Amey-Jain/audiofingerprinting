#include<stdint.h>
#include<inttypes.h>
#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
#include "config.h"
#include "lsh_db.h"
#include "sub_reader.h"
#include "av_decoder.h"

char *filename=NULL;
char *tablename;
int number_of_hash_tables = 25;
static struct table *tables[25];
struct votes r_set; 
static struct lookup_table *indices[MAX_INDEXES]; //Index lookup table for getting index values when required
static uint16_t candidate[30];
static uint16_t sum_fp_match;
static uint16_t ctr_fp_match_avg;


int callback_debug(void *p, int *result_columns, char **col_text, char **col_names){
  int i = 0;
  for(i;i<(*result_columns);i++){
    fprintf(stderr,"%s = %s\n",col_names[i],col_text[i] ? col_text : "NULL");
  }
  fprintf(stderr,"\n");
  return 0;
}
 
/*
min_hash array of 100 minimum hash values
number_of_hash_tables number of hash tables we will going to use (25)
number_of_hash_per_key (4)
 */
void group_minhash_to_lsh_buckets(uint8_t *min_hash, int number_of_hash_per_key){
  int i=0,j=0;
  uint32_t hashbucket;
  if(number_of_hash_per_key > 8){
    fprintf(stderr,"ERROR: Number of hash per key can not be greater than 8\n");
    return ;
  }
  for(i;i<number_of_hash_tables;i++){
    uint8_t array[8];
    for(j;j<number_of_hash_per_key;j++){
      array[j] = min_hash[i * number_of_hash_per_key + j];
    }
    hashbucket = array[0] | array[1] << 8 | array[2] << 16 | array[3] << 24;
  }
  //store hashbucket with index i being the hash table and hashbucket being signature
}

void init_db(){
  int i,j;
  void *temp;
  //allocate space to all tables
  for(i=0;i<25;i++){
    tables[i] = malloc(sizeof(struct table));
    if(tables[i] == NULL){
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Not enough memory to allocate from heap for table\n"ANSI_COLOR_RESET);
      return -1;
    }
    temp = malloc(sizeof(struct table_entry) * NO_OF_ENTRIES_ALLOC);
    if(temp != NULL)
      tables[i]->entries = temp;
    if(temp == NULL){
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Not enough memory to allocate from heap for entries\n"ANSI_COLOR_RESET);
      return -1;
    }
    tables[i]->entries_ctr = -1;
  }
  fprintf(stdout,ANSI_COLOR_DEBUG"DEBUG: Tables initialized sucessfully\n"ANSI_COLOR_RESET);

  r_set.ctr_max = NO_OF_ENTRIES_VOTES;
  r_set.ctr = -1;
  fprintf(stdout,ANSI_COLOR_DEBUG"DEBUG: Result set initialized sucessfully\n"ANSI_COLOR_RESET);
  for(i=0;i<MAX_INDEXES;i++){
    indices[i] = malloc(sizeof(struct lookup_table));
  }
}

/*
searches for a long int in table table_no 
return values  | meaning
>=0            | index of entry where entry is found
-1             | Table is empty
-2             | Entry is not found
*/ 
int search_for_entry_in_table(uint64_t entry,int table_no){
  int i,j,ctr;
  ctr = tables[table_no]->entries_ctr;
  if(ctr == -1){
    fprintf(stdout,ANSI_COLOR_DEBUG"DEBUG: Table %d is empty\n"ANSI_COLOR_RESET);
    return -1;
  }
  else {
    for(i=0;i<=ctr;i++){
      if(tables[table_no]->entries[i].f_value == entry){
	return i;
      }
    }
  }
  return -2;
}


/*
insert index into table table_no and entry with entry_index
If index is found in entry nothing is done, else a new index is added to it
 */
int insert_index_into_entry(int table_no, int entry_index,uint16_t index,uint64_t start_pts,uint64_t end_pts,uint16_t delay){
  int i,j,ctr;
  uint16_t *iptr;
  ctr = tables[table_no]->entries[entry_index].index_ctr;
  if(ctr == -1){// case: New entry and first index entry
    ctr++;
    if((tables[table_no]->entries[entry_index].indexes = malloc(sizeof(uint16_t) * NO_OF_ENTRIES_ALLOC)) == NULL){
      tables[table_no]->entries[entry_index].index_ctr = -1;
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Not able to allocate memory space for indices\n"ANSI_COLOR_RESET);
      return -1;
    }
    tables[table_no]->entries[entry_index].index_max = NO_OF_ENTRIES_ALLOC;
    tables[table_no]->entries[entry_index].indexes[ctr] = (uint16_t) index;
    tables[table_no]->entries[entry_index].index_ctr = ctr;
    //    printf("Value of inserted index %d\n",tables[table_no]->entries[entry_index].indexes[ctr]);
  }
  else if(ctr < tables[table_no]->entries[entry_index].index_max - 1){ // middle entry
    ctr++;
    tables[table_no]->entries[entry_index].indexes[ctr] = (uint16_t) index;
    tables[table_no]->entries[entry_index].index_ctr = ctr;
    //    printf("Value of inserted index %d\n",tables[table_no]->entries[entry_index].indexes[ctr]);
  }
  else if(ctr == tables[table_no]->entries[entry_index].index_max - 1) { //if table is full 
    //reallocate with increased space
    tables[table_no]->entries[entry_index].index_max = tables[table_no]->entries[entry_index].index_max + NO_OF_ENTRIES_ALLOC;
    iptr = tables[table_no]->entries[entry_index].indexes;
    if((iptr = realloc(iptr,sizeof(uint16_t) * tables[table_no]->entries[entry_index].index_max)) == NULL){
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Not able to reallocate memory space for indices\n"ANSI_COLOR_RESET);
      return -1;
    }
    ctr++;
    tables[table_no]->entries[entry_index].indexes = iptr;
    tables[table_no]->entries[entry_index].indexes[ctr] = index;
    tables[table_no]->entries[entry_index].index_ctr = ctr;
    //    printf("Value of inserted index %d\n",tables[table_no]->entries[entry_index].indexes[ctr]);
  }
  else {
    printf("FUNDA HI ALAG HO REA HAI\n\n");
  }
  if(index < MAX_INDEXES){
    indices[index]->entries[table_no] = (uint16_t) entry_index;
    //add here start and end pts also
    indices[index]->start_pts = start_pts;
    indices[index]->end_pts = end_pts;
    indices[index]->delay = delay;
  }
  else
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Failed to update lookup table of indexes. Increase MAX_INDEXES macro in lsh_db.h\n"ANSI_COLOR_RESET);
  return 0;
}
 
int insert_entry_into_table(uint64_t *result,uint16_t fingerprint_index,uint64_t start_pts,uint64_t end_pts,uint16_t delay){
  int i,j,ret,entries_ctr,index_ctr;
  void *temp;
  for(i=0;i<25;i++){
    ret = search_for_entry_in_table(result[i],i);
    //    printf("%d ",i);
    if(ret == -1){ // case: table is empty
      //inc entries_ctr and insert entry
      //      printf("CASE: Table is empty\n");
      ++(tables[i]->entries_ctr);
      entries_ctr = tables[i]->entries_ctr;
      tables[i]->entries_max = NO_OF_ENTRIES_ALLOC;
      tables[i]->entries[entries_ctr].f_value = result[i];
      tables[i]->entries[entries_ctr].index_ctr = -1;
      ret = insert_index_into_entry(i,entries_ctr,fingerprint_index,start_pts,end_pts,delay); 
      if(ret != 0){
	fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Failed to add index to entry\n"ANSI_COLOR_RESET);
	return ret;
      }
      //store value of table number on index in lookup_table
    }
    else if(ret == -2){ // case: table may or may not be full
      //entry not found in table so add the entry to table
      //allocate more space for entries and insert entry
      //      printf("CASE Entry not found in table\n");      
      if(tables[i]->entries_ctr == tables[i]->entries_max - 1){ // if table is full
	fprintf(stderr,ANSI_COLOR_DEBUG"DEBUG: Table %d filled with entries. Allocating new memory\n"ANSI_COLOR_RESET,i);
	tables[i]->entries_max = tables[i]->entries_max + NO_OF_ENTRIES_ALLOC;
	if((temp = realloc(tables[i]->entries,tables[i]->entries_max * sizeof(struct table_entry))) == NULL){
	  fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Space for new entry not available %d\n"ANSI_COLOR_RESET,__LINE__);
	  return -1;
	}
	tables[i]->entries = temp;
	entries_ctr = ++(tables[i]->entries_ctr);
	tables[i]->entries[entries_ctr].f_value = result[i];
	tables[i]->entries[entries_ctr].index_ctr = -1;
	ret = insert_index_into_entry(i,entries_ctr,fingerprint_index,start_pts,end_pts,delay);
	if(ret != 0){
	  fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Failed to add index to entry %d\n"ANSI_COLOR_RESET,__LINE__);
	  return ret;
	}
      }
      else {
	entries_ctr = ++(tables[i]->entries_ctr);
	tables[i]->entries[entries_ctr].f_value = result[i];
	tables[i]->entries[entries_ctr].index_ctr = -1;
	ret = insert_index_into_entry(i,entries_ctr,fingerprint_index,start_pts,end_pts,delay);
      }
    }
    else {
      // case: entry found
      //search through indices of entry
      //and enter the index in entry
      //      printf("Case Entry found at index %d\n",ret);
      ret = insert_index_into_entry(i,ret,fingerprint_index,start_pts,end_pts,delay);
    }
  }
  return ret;
}


void print_tables(){
  int i,j,k;
  for(i=0;i<25;i++){
    int ctr = tables[i]->entries_ctr;
    fprintf(stdout,ANSI_COLOR_CYAN"table %d\nFingerprint value\tIndexes\n"ANSI_COLOR_RESET,i);
    for(j=0;j<=ctr;j++){
      fprintf(stdout,"%lu\t\t\t",tables[i]->entries[j].f_value);
      int i_ctr = tables[i]->entries[j].index_ctr;
      for(k=0;k<=i_ctr;k++)
	fprintf(stdout,"%d\t",tables[i]->entries[j].indexes[k]);
      fprintf(stdout,"\n");
    }
  }
}


int match_cur(uint64_t *result,uint16_t index){
  int i,j,ctr=0;
  for(i=0;i<25;i++){
    j = indices[index]->entries[i]; // index of entry with index as its index
    if((tables[i]->entries[j].f_value) == (result[i]))
      ctr++;
  }
  return ctr;
}

uint64_t match_cur_bitwise(uint64_t *result,uint16_t index){
  int i,j,k;
  uint64_t ret=0,temp;
  uint16_t ctr=0;
  for(i=0;i<25;i++){
    j = indices[index]->entries[i]; // index of entry with index as its index
    temp = (tables[i]->entries[j].f_value) ^ result[i];
    //applying Brian Kernighan algorithm to count number of 1 for hamming distance
    while(temp != 0){
      temp = temp & (temp - 1);
      ctr++;
    }
  }
  return ctr;
}

int search_and_match(uint64_t *result,uint16_t check_index,uint64_t start_pts){
  int i,j,ctr,k,low_index_ctr=0,ret,temp3=0;
  uint16_t entries_ctr,index_ctr;
  uint64_t ret2;
  char *temp1=NULL,*temp2=NULL;
  r_set.ctr = -1;
  for(i=0;i<25;i++){
    ret = search_for_entry_in_table(result[i],i);
    if(ret >= 0){
      index_ctr = tables[i]->entries[ret].index_ctr;
      if(r_set.ctr == -1){
	r_set.ctr = index_ctr;
	for(j=0;j<=index_ctr;j++){
	  r_set.indexes[j] = tables[i]->entries[ret].indexes[j];
	  r_set.votes[j] = 1;
	}
      }
      else{
	for(j=low_index_ctr;j<index_ctr;j++){
	  for(k=0;k<=r_set.ctr;k++){
	    if(r_set.indexes[k] == tables[i]->entries[ret].indexes[j]){
 	      r_set.votes[k] += 1;
	      low_index_ctr++;
	      break;
	    }
	  }
	  if(tables[i]->entries[ret].indexes[j] != r_set.indexes[k]) { // add new number to votes	  
	    r_set.ctr++;
	    r_set.indexes[r_set.ctr] = tables[i]->entries[ret].indexes[j];
	    r_set.votes[r_set.ctr] = 1;
	    low_index_ctr++;
 	  }
	}
      }
    }
  }
  
  printf("Vote\n");
  for(i=0;i<r_set.ctr;i++)
    if(r_set.votes[i] > 1){
      uint16_t temp = r_set.indexes[i];
      ret = match_cur(result,temp);
      //      fprintf(stdout,ANSI_COLOR_DEBUG"DEBUG: %d/25 signatures matched\n",ret);
      if(ret > 8){
	ret2 = match_cur_bitwise(result,temp);
	//	fprintf(stdout,ANSI_COLOR_DEBUG"DEBUG: %d hamming distance\n",ret2);
	if(ret2 < 20){
	  //if(300 + (start_pts/1000) > indices[r_set.indexes[i]]->start_pts/1000) // timings greater than 3 minute from edited are excluded
	  if(start_pts < indices[r_set.indexes[i]]->start_pts && (start_pts + (LONGEST_STRIDE * 1000) > indices[r_set.indexes[i]]->start_pts)){
  	  	    printf("%f match found with index\t%d\tHamming distance sum:%lu\t\t%s\t-\t%s\n",(float)ret/25,r_set.indexes[i],ret2,seconds_to_pts(indices[r_set.indexes[i]]->start_pts/1000,temp1),seconds_to_pts(indices[r_set.indexes[i]]->end_pts/1000,temp2));
		    temp3++;
		}
	}
      }
    }
  if(temp3)
    ctr_fp_match_avg++;
  sum_fp_match = sum_fp_match + temp3;  
  printf(ANSI_COLOR_MAGENTA"Avg match count for fingerprints %f No of fingerprints %f\n"ANSI_COLOR_RESET,(float) sum_fp_match/ctr_fp_match_avg,(float) ctr_fp_match_avg/check_index);
  return 0;
}

void print_fp_by_index(uint16_t index){
  int i,j;
  uint16_t idx;
  printf("DEBUG: Index %d\n\n",index);
  for(i=0;i<25;i++){
    idx = indices[index]->entries[i];
    printf("%lu\t",tables[i]->entries[idx].f_value);
    if((i+1) % 5 == 0)
      printf("\n");
  }
}

void close_db(){
  int i,j,ctr;
  for(i=0;i<25;i++){
    free(tables[i]);
  }
  for(i=0;i<MAX_INDEXES;i++)
    free(indices[i]);
  fprintf(stdout,ANSI_COLOR_DEBUG"DEBUG: All memory freed up sucessfully\n"ANSI_COLOR_RESET);
}
