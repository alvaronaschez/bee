#include "text.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static inline struct string *load_file(const char *filename, int *len){
  if (filename == NULL) return 0;
  FILE *fp = fopen(filename, "r");
  assert(fp);
  int res = fseek(fp, 0, SEEK_END);
  assert(res == 0);
  long fsize = ftell(fp);
  assert(fsize >= 0);
  if(fsize==0){
    *len = 1;
    struct string *retval = malloc(sizeof(struct string));
    retval[0].cap = retval[0].len = 0;
    retval[0].p = calloc(1,1);
    return retval;
  }
  rewind(fp);
  char *fcontent = (char*) malloc(fsize * sizeof(char));
  fread(fcontent, 1, fsize, fp);
  res = fclose(fp);
  assert(res == 0);
  // count lines in file
  int nlines = 0;
  for(int i = 0; i<fsize; i++)
    if(fcontent[i] == '\n') nlines++;
  if(fcontent[fsize-1] != '\n')
    nlines++;
  *len = nlines;
  // copy all lines from fcontent into buf
  struct string *buf = malloc(nlines * sizeof(struct string));
  int linelen;
  for(int i = 0, j = 0; i<nlines && j<fsize; i++){
    // count line length
    for(linelen = 0; j+linelen<fsize-1 && fcontent[j+linelen]!='\n'; linelen++);
    // copy line in buffer
    buf[i].p = calloc(linelen+1, sizeof(char));
    if(linelen>0)
      memcpy(buf[i].p , &fcontent[j], linelen);
    buf[i].len = buf[i].cap = linelen;

    j += linelen+1;
  }
  free(fcontent);
  // calle should free buf
  return buf;
}

static inline void save_file(const struct text *txt, const char *filename){
  struct stat st;
  assert(0==stat(filename, &st));
  char *tmp = malloc(strlen(filename)+strlen(".bee.bak")+1);
  sprintf(tmp, "%s.bee.bak", filename);
  assert(0==rename(filename, tmp));
  FILE *f = fopen(filename, "a");
  assert(f);
  for(int i=0; i<txt->len; i++)
    fprintf(f, "%s\n", txt->p[i].p);
  chmod(filename, st.st_mode);
  chown(filename, st.st_uid, st.st_gid);
  assert(0==fclose(f));
  assert(0==remove(tmp));
  free(tmp);
}


