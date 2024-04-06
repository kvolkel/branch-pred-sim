/*************************************************************************************************

Filename: main.cpp

Date Modified: 11/01/2017

Author: Kevin Volkel

Description: This file is the main file for the branch predictor. This main file handles the inputs that
are entered from the command line and puts the appropriate values into the right variables to be inputs to
the setup function for the branch predictor. After parsing the command line, this file reads the appropraite trace
file and hands two pieces of information to a function in the branch predictor class. The two pieces
of information are the PC of the current branch and the outocome of that branch. At the end of this file,
a function is called that prints out the appropriate results for the desired branch predictor.  
*****************************************************************************************************/
#include "branch.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>





int main(int argc, char** argv ){
  FILE* file;
  unsigned long long address;
  unsigned long long  holder;
  char t_or_f;
  char LINE_IN[1024];
  int m2_1, m2,n, BTB_size, BTB_assoc,k,indicator;
  char* filename;
  printf("COMMAND\n");
  //parse the command line inputs depending on the predictor type
  if(strcmp(argv[1],"bimodal")==0){
    m2_1=atoi(argv[2]);
    BTB_size=atoi(argv[3]);
    BTB_assoc=atoi(argv[4]);
    m2=0;
    n=0;
    k=0;
    indicator=0;
    filename=argv[5];
    printf("%s %s %s %s %s %s", argv[0],argv[1],argv[2],argv[3],argv[4],argv[5]);
  }
  else if(strcmp(argv[1],"gshare")==0){
    m2_1=atoi(argv[2]);
    n=atoi(argv[3]);
    BTB_size=atoi(argv[4]);
    BTB_assoc=atoi(argv[5]);
    filename=argv[6];
    k=0;
    indicator=1;
    m2=0;
    printf("%s %s %s %s %s %s %s", argv[0],argv[1],argv[2],argv[3],argv[4],argv[5],argv[6]);    
  }
  else if(strcmp(argv[1],"hybrid")==0){
    m2_1=atoi(argv[3]);
    n=atoi(argv[4]);
    BTB_size=atoi(argv[6]);
    BTB_assoc=atoi(argv[7]);
    filename=argv[8];
    k=atoi(argv[2]);
    indicator=2;
    m2=atoi(argv[5]);
    printf("%s %s %s %s %s %s %s %s %s", argv[0],argv[1],argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8]);    
  }
  printf("\nOUTPUT\n");
    
  

  //initiate the branch predictor
  Branch B1(indicator,k,m2_1,n,m2,BTB_size,BTB_assoc);
  //open the file and start passing data into the branch predictor
  file=fopen(filename,"r");
  if(file==NULL){
    printf("Error opening file\n");
    return 0;
  }
  while(fgets(LINE_IN,1024,file)!=NULL){
    sscanf(LINE_IN, "%llX %c",&holder,&t_or_f);
    address=(unsigned long long)holder;
    //give PC and taken or not taken data to the branch predictor
    B1.branch_in(address,t_or_f);
  }
  //close the file
  fclose(file);
  //report final results of branch predictor
  B1.report();
  return 0;
}
