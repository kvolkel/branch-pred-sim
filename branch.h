/**************************************************************************************************************

Filename:     branch.h


Date Modified: 11/1/17


Author: Kevin Volkel


Description: This file is the header file for the predictor class. All variables that pertain to a predictors statistics
are declared in this file. Functions also used for branch prediction are declared in this file.

*****************************************************************************************************************/
#include <stdint.h>

//structure that represents an entry in the BTB
struct block{
  int tag;
  int age;
  int dirty;
  int valid;
};



//Class that represents an instance of the branch predictor
class Branch{
 private:
  //variables that hold the configuration of the branch predictor and BTB
  //BTB characteristics that are input through the command line
  int assoc;
  int block_size;
  int blk_per_set;
  int num_sets;
  int set_bits;
  int tag_bits;
  int block_bits;
  //replacement policy for BTB
  int replace_policy;
  //variables for holding characteristics of the branch predictor
  int num_entries;
  //gshare_entries, bimodal_entries, and chooser_entries are all used for implementing the hybrid predictor
  int gshare_entries;
  int bimodal_entries;
  int chooser_entries;
  int size_of_BTB;
  int PC_bits;
  int n;
  int k;
  //variables used for indexing when using the hybrid predictors
  unsigned long long index_bimodal;
  unsigned long long index_gshare;
  unsigned long long index_chooser;
  //variables for holding the number of bits that need to be taken from the PC
  int PC_gshare;
  int PC_bimodal;
  int PC_chooser;
  //This pointer is used to point to the BTB. Each element is a pointer to an array of block structures. Each element represents a set. 
  struct block** set_array;
  //counters for the predictor tables
  int* predictor_counters;
  int* gshare_table;
  int* bimodal_table;
  int* chooser_table;
  //variable that holds the type of prediction being made
  int type_pred;
 public:
  //variables that are used to calculate statistics about the predictor
  int num_branches;
  int BTB_hits;
  int mispredictions_BP;
  int misses;
  float miss_rate;
  int BTB_miss_misprediction;
  int total_mispredictions;
  int global_register;
  int pred_from_predictor;



  //functions that are used to implement the replace and write policies. 
  Branch(int indicator,int k_1,int m2_1,int n_1,int m2,int BTB_size,int BTB_assoc);
  //update and replacement functions for the BTB
  void LRU_update(int tag, int set, int hit);
  void LRU_replace(int tag, int set, unsigned long long address);
  //funciton that inputs a PC to the predictor
  void branch_in(unsigned long long address, char t_or_f);
  //function to update the global register
  void global_reg_update(char t_or_f);
  //function to check to see if there is a hit on a PC for the BTB
  int hit_or_miss(int tag, int set);
  void predictor_table(int index, char t_or_f);
  //function for implementing hybrid predictor
  void hybrid_prediction(int ind_gshare, int ind_bimodal, int ind_chooser, char t_or_n);
  //reports out the statistics of the branch predictor and BTB if applicable
  void report();
};
