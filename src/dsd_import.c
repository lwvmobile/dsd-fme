

#include "dsd.h"
#define BSIZE 999

int csvGroupImport(dsd_opts * opts, dsd_state * state)
{
  char filename[1024] = "filename.csv"; 
  sprintf (filename, "%s", opts->group_in_file); 
  //filename[1023] = '\0'; //necessary?
  char buffer[BSIZE];
  FILE * fp;
  fp = fopen(filename, "r");
  if (fp == NULL) {
    printf("Unable to open file '%s'\n", filename);
    exit(1);
  }
  int row_count = 0;
  int field_count = 0;
  long int group_number = 0; //local group number for array index value
  int i = 0;
  while (fgets(buffer, BSIZE, fp)) {
    field_count = 0;
    row_count++;
    if (row_count == 1)
      continue; //don't want labels
    char * field = strtok(buffer, ","); //seperate by comma
    while (field) {
      
      if (field_count == 0)
      {
        //group_number = atol(field);
        state->group_array[i].groupNumber = atol(field);
        fprintf (stderr, "%ld, ", state->group_array[i].groupNumber);
      }
      if (field_count == 1)
      {
        strcpy(state->group_array[i].groupMode, field);
        fprintf (stderr, "%s, ", state->group_array[i].groupMode);
      }
      if (field_count == 2)
      {
        strcpy(state->group_array[i].groupName, field);
        fprintf (stderr, "%s ", state->group_array[i].groupName);
      }
 
      field = strtok(NULL, ",");
      field_count++;
    }
    fprintf (stderr, "\n");
    i++;
    state->group_tally++; 
  }
  fclose(fp);
  return 0;
}

int csvLCNImport(dsd_opts * opts, dsd_state * state) //LCN/LSN import for EDACS/DMR/NXDN?
{
  char filename[1024] = "filename.csv"; 
  sprintf (filename, "%s", opts->lcn_in_file); 
  //filename[1023] = '\0'; //necessary?
  char buffer[BSIZE];
  FILE * fp;
  fp = fopen(filename, "r");
  if (fp == NULL) {
    printf("Unable to open file '%s'\n", filename);
    //have this return -1 and handle it inside of main
    exit(1);
  }
  int row_count = 0;
  int field_count = 0;

  while (fgets(buffer, BSIZE, fp)) {
    field_count = 0;
    row_count++;
    if (row_count == 1)
      continue; //don't want labels
    char * field = strtok(buffer, ","); //seperate by comma
    while (field) {

      state->trunk_lcn_freq[field_count] = atol (field);
      state->lcn_freq_count++; //keep tally of number of Frequencies imported
      fprintf (stderr, "LCN [%d] [%ld]", field_count+1, state->trunk_lcn_freq[field_count]);
      fprintf (stderr, "\n");

      
      field = strtok(NULL, ",");
      field_count++;
    }
    fprintf (stderr, "LCN Count %d\n", state->lcn_freq_count);
    
  }
  fclose(fp);
  return 0;
}
