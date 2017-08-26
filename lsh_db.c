#include<stdint.h>
#include<sqlite3.h>
#include<inttypes.h>
#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
#include "config.h"
#include "lsh_db.h"

sqlite3 **db=NULL;
char *filename=NULL;
char *tablename;
int number_of_hash_tables = 25;
static struct table tables[25];
static struct votes r_set; 

int callback_debug(void *p, int *result_columns, char **col_text, char **col_names){
  int i = 0;
  for(i;i<(*result_columns);i++){
    fprintf(stderr,"%s = %s\n",col_names[i],col_text[i] ? col_text : "NULL");
  }
  fprintf(stderr,"\n");
  return 0;
}

void initialise_database(void){
  int ret;
  char *err_msg=0;
  ret = sqlite3_open(filename, db);
  if(ret == SQLITE_OK){
    fprintf(stdout,"DEBUG:Sucessfully created database with name %s\n",filename);
  }
  else if(db == NULL){
    fprintf(stderr,"ERROR: %s unable to allocate memory for database\n",__FUNCTION__);
    return ;
  }
  else{
    fprintf(stderr,"ERROR: %s sqlite3_open returned %d %s\n",__FUNCTION__,ret,sqlite3_errstr(ret));
  }

  //create tables and other data
  ret = sqlite3_exec(db,			\
		     "CREATE TABLE orignal (\
                         hash long,	    \
                         subtitle_index int \
                         );",\
		     callback_debug,			\
		     0,				\
		     &err_msg);

  if(ret != 0){
    fprintf(stderr,"ERROR: sqlite3 returned %s\n",err_msg);
  }
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
  int i = 0;
  for(i;i<25;i++)
    tables[i].entries_ctr = 0;
}

/*
searches for a long int in table table_no 
Returns index of entry if sucessful else -1
 */
int search_for_entry_in_table_i(uint64_t entry,int table_no){
  if(tables[table_no].entries_ctr == 0){
    //    fprintf(stdout,"DEBUG: Table %d is empty\n",table_no);
    return -1;
  }
  else {
    int ctr = tables[table_no].entries_ctr;
    //    fprintf(stdout,"DEBUG: %s no of entries in this table %d\n",__FUNCTION__,ctr);
    int i,j;
    for(i=0;i<ctr;i++){
      if(entry == tables[table_no].entries[i].f_value){
	//	fprintf(stdout,"DEBUG: Entry found at index %d\n",i);
	return i;
      }
    }
    return -1;
  }
}

int insert_result_into_table(uint64_t *result,uint16_t fingerprint_index){
  int i,j,ret,entries_ctr,index_ctr;
  for(i=0;i<25;i++){ // outer loop for tables
    if((ret = search_for_entry_in_table_i(result[i],i)) == -1){
      entries_ctr = (tables[i].entries_ctr++);
      fprintf(stdout,"entries counter %lu\n",entries_ctr);
      tables[i].entries[entries_ctr].f_value = result[i];
      index_ctr = tables[i].entries[entries_ctr].index_ctr = 0;
      tables[i].entries[entries_ctr].indexes[index_ctr] = fingerprint_index;
    }
    else {
      if(ret > MAX_ENTRIES_PER_TABLE){
	fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Limit of entries per table exceeded.\n"ANSI_COLOR_RESET);
	return -1;
      }
      else{
	index_ctr = (tables[i].entries[ret].index_ctr++);
	tables[i].entries[ret].indexes[index_ctr] = fingerprint_index;
      }
    }
  }
}

void print_tables(){
  int i,j,k;
  for(i=0;i<25;i++){
    int ctr = tables[i].entries_ctr;
    fprintf(stdout,"table %d\nFingerprint value\tIndexes\n",i);
    for(j=0;j<ctr;j++){
      fprintf(stdout,"%lu\t\t",tables[i].entries[j].f_value);
      int i_ctr = tables[i].entries[j].index_ctr;
      for(k=0;k<i_ctr;k++)
	fprintf(stdout,"%d\t",tables[i].entries[j].indexes[k]);
      fprintf(stdout,"\n");
    }
  }
}

int search_and_match(uint64_t *result,uint16_t index){
  int i,j,ret,ctr,k;
  r_set.ctr = -1;
  for(i=0;i<25;i++){
    ret = search_for_entry_in_table_i(result[i],i);
    if(ret >= 0){ // copy indexes of entry
      ctr = tables[i].entries[ret].index_ctr;
      if(r_set.ctr == -1){
	fprintf(stdout,"DEBUG: Copying all indices of entry\n");
	r_set.ctr = tables[i].entries[ret].index_ctr;
	for(j=0;j<tables[i].entries[ret].index_ctr;j++){ 
	  r_set.indexes[j] = tables[i].entries[ret].indexes[j];
	  r_set.votes[j] = 1;
	}
      }
      else { // there are indices present already
	for(j=0;j<r_set.ctr;j++){
	  for(k=0;k<ctr;k++){
	    if(r_set.indexes[k] == tables[i].entries[ret].indexes[j]){
	      r_set.votes[k] = r_set.votes[k] + 1;
	      break;
	    }
	    
	  }
	}
      }
    }
    
  }
  fprintf(stdout,"INDEX\tVOTES\n");
  for(i=0;i<r_set.ctr;i++)
    fprintf(stdout,"%d\t%d\n",r_set.indexes[i],r_set.votes[i]);
}
