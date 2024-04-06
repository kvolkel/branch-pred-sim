/**************************************************************************************************************************

Filename: branch.cpp

Date modified: 11/1/17

Author Kevin Volkel

Description: This file contains all of the implementations for the functions declared in the branch.h. The branch predictor simulator
is capable of simulating 3 different branch types including bimodal, gshare, and hybrid (combination of bimodal and gshare). This simulator
can also support the simulation of a BTB (branch target buffer) if it is so desired. As data is streamed in from the trace file
the simulator accumulates statistics on the predictions that are made, and the results of these predictions along with the contents
of the predictor table and BTB (if applicable) are printed out.


****************************************************************************************************************************/

#include "branch.h"
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>



//initiate the predictor with input parameters
Branch::Branch(int indicator, int k_1, int m2_1, int n_1, int m2, int BTB_size,int BTB_assoc){
  //load in branch predictor parameters
  size_of_BTB=BTB_size;
  blk_per_set=BTB_assoc;
  if(BTB_assoc>0) num_sets=(BTB_size/(4*BTB_assoc));
  else num_sets=0;
  block_size=4;
  assoc=BTB_assoc;
  //fix to LRU
  replace_policy=0;
  //calculate number of bits for each field
  if(BTB_assoc>0)set_bits=log2(num_sets);
  block_bits=log2(4);
  tag_bits=32-(set_bits+block_bits);
  
  //number of predictor table entries
  num_entries=pow(2,m2_1);
  gshare_entries=pow(2,m2_1);
  bimodal_entries=pow(2,m2);
  chooser_entries=pow(2,k_1);
  //initiate the BTB, and predictor counters
  set_array=(struct block**)malloc(num_sets*sizeof(struct block*));
  predictor_counters=(int*)malloc(num_entries*sizeof(int));
  gshare_table=(int*)malloc(gshare_entries*sizeof(int));
  chooser_table=(int*)malloc(chooser_entries*sizeof(int));
  bimodal_table=(int*)malloc(bimodal_entries*sizeof(int));
  if(BTB_assoc>0){
    for(int i=0; i<num_sets;i++){
      set_array[i]=(struct block*)malloc(blk_per_set*sizeof(struct block));
      for(int j=0;j<blk_per_set;j++){
	set_array[i][j].valid=0;
	set_array[i][j].dirty=0;
	set_array[i][j].age=0;
      }
    }
  }
  //initiate the counters for gshare and bimodal
  if(indicator==1 || indicator==0){
    for(int i=0;i<num_entries;i++){
      predictor_counters[i]=2;
    }
  }
  //initialize predictor tables for the hybrid prediction
  else if(indicator==2){
    for(int i=0; i<gshare_entries;i++) gshare_table[i]=2;
    for(int i=0; i<bimodal_entries;i++) bimodal_table[i]=2;
    for(int i=0; i<chooser_entries;i++) chooser_table[i]=1;
  }


  //init counters
  PC_bits=m2_1;
  PC_gshare=m2_1;
  PC_chooser=k_1;
  PC_bimodal=m2;
  n=n_1;
  k=k_1;
  num_branches=0;
  BTB_hits=0;
  mispredictions_BP=0;
  miss_rate=0;
  BTB_miss_misprediction=0;
  total_mispredictions=0;
  global_register=0;
  type_pred=indicator;
}
  

//function to read in a address and write/read command. figures out what to do with the request
void Branch::branch_in(unsigned long long address, char t_or_f){
  int tag;
  int set;
  int index_of_hit;
  int block_tag=block_bits+tag_bits;
  unsigned long long PT_index;
  unsigned long long top_PC_index;
  unsigned long long bottom_PC_index;
  unsigned long long PC;
  //increment branches counter
  num_branches++;
  //get index for bimodal
  if(type_pred==0) PT_index=(((address>>2)<<(32-PC_bits))&0x00000000FFFFFFFF)>>(32-PC_bits);
  //get index for gshare
  else if(type_pred==1) {
    PC=(((address>>2)<<(32-PC_bits))&0x00000000FFFFFFFF)>>(32-PC_bits);
    top_PC_index=(PC>>(PC_bits-n));
    top_PC_index=top_PC_index^global_register;
    bottom_PC_index=((PC<<(32-(PC_bits-n)))&0x00000000FFFFFFFF)>>(32-(PC_bits-n));
    PT_index=(top_PC_index<<(PC_bits-n))+bottom_PC_index;
  }
  //get indexes for the hybrid predictors
  else if(type_pred==2){
    index_bimodal=(((address>>2)<<(32-PC_bimodal))&0x00000000FFFFFFFF)>>(32-PC_bimodal);
    PC=(((address>>2)<<(32-PC_gshare))&0x00000000FFFFFFFF)>>(32-PC_gshare);
    top_PC_index=(PC>>(PC_gshare-n));
    top_PC_index=top_PC_index^global_register;
    bottom_PC_index=((PC<<(32-(PC_gshare-n)))&0x00000000FFFFFFFF)>>(32-(PC_gshare-n));
    index_gshare=(top_PC_index<<(PC_gshare-n))+bottom_PC_index;
    index_chooser=(((address>>2)<<(32-PC_chooser))&0x00000000FFFFFFFF)>>(32-PC_chooser);
  }

  //using BTB
  if(assoc!=0 &&  num_sets!=0){
    //calculate the tag and set from the PC
    tag=address>>(set_bits+block_bits);
    set=((address<<tag_bits)&0x00000000FFFFFFFF)>>(block_tag);
    index_of_hit=hit_or_miss(tag, set);
    //have a cache hit
    if(index_of_hit!=-1){
      LRU_update(tag, set, index_of_hit);
      //upon hit, besides updating LRU, a prediction needs to be made
      if(type_pred==0 || type_pred==1)predictor_table(PT_index,t_or_f);
      else hybrid_prediction(index_gshare,index_bimodal,index_chooser,t_or_f);
    }
    //we have a miss
    else{
      //increment the BTB miss misprediction counter if branch is actually taken
      if(t_or_f=='t') BTB_miss_misprediction++;
      LRU_replace(tag,set, address);
    }
  }
  //not using BTB
  else{
    if(type_pred==0 || type_pred==1)predictor_table(PT_index, t_or_f);
    else hybrid_prediction(index_gshare,index_bimodal,index_chooser,t_or_f);
  }

}




//function for hybrid prediction
void Branch::hybrid_prediction(int ind_gshare,int ind_bimodal, int ind_chooser, char t_or_n){
  int gshare_prediction;
  int bimodal_prediction;
  int gshare_correct;
  int bimodal_correct;
  int chooser;
  pred_from_predictor++;
  //get prediction values
  gshare_prediction=gshare_table[ind_gshare];
  bimodal_prediction=bimodal_table[ind_bimodal];
  chooser=chooser_table[ind_chooser];
  //use gshare
  if(chooser>=2){
    //prdict taken
    if(gshare_prediction>=2){
      if(t_or_n=='n') {
	mispredictions_BP++;
      }
    }

    //gshare predict not taken
    else{

      if(t_or_n=='t'){
	mispredictions_BP++;
      }
    }
    
    //update the gshare predictor
    if(t_or_n=='t') {
      if(gshare_table[ind_gshare]<3) gshare_table[ind_gshare]++;
    }
    else{
      if(gshare_table[ind_gshare]>0)gshare_table[ind_gshare]--;
    }

    //update the global register
    global_reg_update(t_or_n);
  }

  //use bimodal
  else if(chooser<2){
    //bimodal predict taken
    if(bimodal_prediction>=2){
      if(t_or_n=='n'){
	mispredictions_BP++;
      }

    }

    //bimodal predict not taken
    else{

      if(t_or_n=='t') {
	mispredictions_BP++;
      }
    }
    //update the predictor and global register
    if(t_or_n=='t') {
      if(bimodal_table[ind_bimodal]<3)bimodal_table[ind_bimodal]++;
    }
    else {
      if(bimodal_table[ind_bimodal]>0)bimodal_table[ind_bimodal]--;
    }
    global_reg_update(t_or_n);

  }
  
  //check to see the results of both predictions from bimodal and gshare
  if(gshare_prediction>=2 && t_or_n=='t') gshare_correct=1;
  else if(gshare_prediction<2 && t_or_n=='n') gshare_correct=1;
  else if(gshare_prediction>=2 && t_or_n=='n') gshare_correct=0;
  else if(gshare_prediction<2 && t_or_n=='t') gshare_correct=0;

  if(bimodal_prediction>=2 && t_or_n=='t') bimodal_correct=1;
  else if(bimodal_prediction<2 && t_or_n=='n') bimodal_correct=1;
  else if(bimodal_prediction>=2 && t_or_n=='n') bimodal_correct=0;
  else if(bimodal_prediction<2 && t_or_n=='t') bimodal_correct=0;
  
  
  //update chooser table
  if(gshare_correct && !bimodal_correct){
    if(chooser_table[ind_chooser]<3)chooser_table[ind_chooser]++;
  }
  else if( bimodal_correct && !gshare_correct){
    if(chooser_table[ind_chooser]>0) chooser_table[ind_chooser]--;
  }
    
}
    







//function to update predictor table
void Branch::predictor_table(int index, char t_or_f){
  int counter_value;
  pred_from_predictor++;
  //get counter value from the index
  counter_value=predictor_counters[index];
  //predicted to take branch
  if(counter_value>=2){
    if(t_or_f=='t'){
      
    }
    else if(t_or_f=='n'){
      //predicted incorrectly
      mispredictions_BP++;
    }
    
  }
  //predict to not take branch
  else{
    if(t_or_f=='t'){
      //predicted incorrectly
      mispredictions_BP++;
    }
    else if(t_or_f=='n'){
    }
  }

  //update table 
  if(t_or_f=='t'){
    if(predictor_counters[index]<3) predictor_counters[index]++;
  }
  else if(t_or_f=='n'){
    if(predictor_counters[index]>0) predictor_counters[index]--;
  }
  //update global register
  if(n>0) global_reg_update(t_or_f);

}
    
//function for updating the global history register for gshare and hybrid predictors
void Branch::global_reg_update(char t_or_f){
  int actual_result;
  if(t_or_f=='t') actual_result=1;
  else actual_result=0;
  //shift the global register right by 1 and insert the actual result into the register
  actual_result=actual_result<<(n-1);
  global_register=((global_register)>>1)+actual_result;
}


//function for updating on the LRU policy
void Branch::LRU_update(int tag, int set, int hit){
  int old_age=set_array[set][hit].age;
  set_array[set][hit].age=0;
  //update the entry counters who are valid and less than the old counter of the accessed entry
  for(int i=0;i<blk_per_set;i++){
    if(set_array[set][i].age<old_age && set_array[set][i].valid==1 && i!=hit) set_array[set][i].age++;
  }
}


    


//function to find the right entry to evict in the set on LRU policy (only used when sim BTB)
void Branch::LRU_replace(int tag, int set, unsigned long long address){
  int index0;
  int found0;
  int oldest_age;
  int oldest_index;
  unsigned long long write_back_address;
  found0=0;
  oldest_age=set_array[set][0].age;
  oldest_index=0;
  //look to see if vacant spot in the set
  for(int i=0; i<blk_per_set;i++){
    if(found0==0){
      //found a vacant block
      if(set_array[set][i].valid==0){
	index0=i;
	found0=1;
	set_array[set][i].tag=tag;
	//set age back to zero
	set_array[set][i].age=0;
	set_array[set][i].valid=1;

      }
    }
  }
  //if did not find invalid to evict, look for a valid one
  if(found0==0){
    //look through all the entry in the set and find the one with the highest counter
    for(int i=0; i<blk_per_set; i++){
      if(set_array[set][i].age>oldest_age){
	oldest_age=set_array[set][i].age;
	oldest_index=i;
      }
    }
    //now we know the oldest entry in the set
    //update the entry with new information
    set_array[set][oldest_index].tag=tag;
    set_array[set][oldest_index].age=0;
  }
  if(found0==1) oldest_index=index0;
  //go through and update the age of all other valid entries
  for(int i=0; i<blk_per_set; i++){
    if(set_array[set][i].valid==1 && i!= oldest_index)set_array[set][i].age++;
  }
}








//function to find the index of the BTB hit
int Branch::hit_or_miss(int tag, int set){
  //scan through all the entry in the correct set
  //if tag matches and entry is valid return the index for it
  for(int i=0; i<blk_per_set;i++){
    if(set_array[set][i].tag==tag && set_array[set][i].valid==1) return i;
  }
  //no match.... return -1
  return -1;
}







// print the contents for the branch predictor and BTB
void Branch::report(){
  //calculate the total mispredictions and the misprediction rate
  total_mispredictions=mispredictions_BP+BTB_miss_misprediction;
  miss_rate=(float)total_mispredictions/(float)num_branches;
  //print stats bimodal
  if(num_sets==0 && assoc==0){
    printf("number of predictions: %i\n",num_branches);
    printf("number of mispredictions: %i\n",total_mispredictions);
    printf("misprediction rate: %.2f %\n",miss_rate*100);
    
  }
  //print results if using BTB
  else{
    printf("size of BTB:  %i\n",size_of_BTB);
    printf("number of branches: %i\n",num_branches);
    printf("number of predictions from branch predictor:   %i\n",pred_from_predictor);
    printf("number of mispredictions from branch predictor: %i\n",mispredictions_BP);
    printf("number of branches miss in BTB and taken: %i\n",BTB_miss_misprediction);
    printf("total mispredictions: %i\n",total_mispredictions);
    printf("misprediction rate: %.2f %\n",miss_rate*100);
    printf("\n");
    printf("FINAL BTB CONTENTS\n");
  }

  //only print if using BTB
  if(num_sets!=0 && assoc!=0){
    //print the set, all the tags in the set
    for(int i=0;i<num_sets;i++){
      printf("set    %i:     ",i);
      for(int j=0; j<blk_per_set;j++){
	printf(" %X   ",set_array[i][j].tag);
      }
      printf("\n");
    }
    printf("\n");
  }


  //print predictor counters for the gshare and bimodal predictors
  if(type_pred==0 || type_pred==1){
    //print predictor counters
    if(type_pred==0)printf("FINAL BIMODAL CONTENTS");
    else printf("FINAL GSHARE CONTENTS");
    for(int i=0;i<num_entries;i++){
      printf("\n%i        %i",i,predictor_counters[i]);
    }
  }
  //print predictor counters for the hybrid predictor
  else if(type_pred==2){
    //print chooser, gshare, and bimodal 
    printf("FINAL CHOOSER CONTENTS");
    for(int i=0;i<chooser_entries;i++) printf("\n%i      %i",i,chooser_table[i]);
    printf("\nFINAL GSHARE CONTENTS");
    for(int i=0;i<gshare_entries;i++) printf("\n%i      %i",i,gshare_table[i]);
    printf("\nFINAL BIMODAL CONTENTS");
    for(int i=0;i<bimodal_entries;i++) printf("\n%i      %i",i,bimodal_table[i]);
  }

}



