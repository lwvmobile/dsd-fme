/*-------------------------------------------------------------------------------
 * dsd_time.c
 * Time and Date Functions
 *
 * LWVMOBILE
 * 2024-04 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

//TODO: Make sure everything still works as intended, every free(timestr) has a NULL check first,
//make sure no other loose or random time functions embedded in other files or functions, etc

#include "dsd.h"

//get HHmmss timestamp no colon (file operations)
char * getTime()
{
  char * curr = (char *) malloc(7);
  time_t t = time(NULL);
  struct tm * ptm = localtime(& t);
  sprintf(curr,"%02d%02d%02d", ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
  return curr;
}

//get HH:mm:ss timestamp with colon (Sync/Console Display)
char * getTimeC()
{
  char * curr = (char *) malloc(9);
  time_t t = time(NULL);
  struct tm * ptm = localtime(& t);
  sprintf(curr, "%02d:%02d:%02d", ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
  return curr;
}

//get HH:mm:ss timestamp with colon (Ncurses Call History)
char * getTimeN(time_t t)
{
  char * curr = (char *) malloc(9);
  struct tm * ptm = localtime(& t);
  sprintf(curr, "%02d:%02d:%02d", ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
  return curr;
}

//get YYYYMMDD without hyphen (file operations)
char * getDate()
{
  char * curr = (char *) malloc(25);
  time_t t = time(NULL);
  struct tm * ptm = localtime(& t);
  sprintf(curr,"%04d%02d%02d", ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday);
  return curr;
}

//get YYYY-MM-DD with hyphen (Sync/Console Display)
char * getDateH()
{
  char * curr = (char *) malloc(27);
  time_t t = time(NULL);
  struct tm * ptm = localtime(& t);
  sprintf(curr, "%04d-%02d-%02d", ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday);
  return curr;
}

//get YYYY-MM-DD with hyphen (Ncurses Call History)
char * getDateN(time_t t)
{
  char * curr = (char *) malloc(27);
  struct tm * ptm = localtime(& t);
  sprintf(curr, "%04d-%02d-%02d", ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday);
  return curr;
}

