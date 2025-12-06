#include "text.h"

#include <stdlib.h>
#include <string.h>

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

static void text_init_from_text(struct text* this, const struct text* other){
  if(this==NULL) return;
  if(other == NULL) {
    this->len = 0; this->p = NULL;
    return;
  }
  this->len = other->len;
  this->p = malloc(this->len*sizeof(char*));
  for(int i=0; i<other->len; i++){
    this->p[i] = malloc(strlen(other->p[i])+1);
    strcpy(this->p[i], other->p[i]);
  }
}


static inline struct insert_cmd delete_cmd_inverse(const struct text*, const struct delete_cmd*);
static inline struct delete_cmd insert_cmd_inverse(const struct text*, const struct insert_cmd*);

void _text_insert(struct text *txt, const struct insert_cmd cmd) {
  int x = cmd.x; int y = cmd.y;
  struct text ntxt;
  text_init_from_text(&ntxt, &cmd.txt);

  // last line
  ntxt.p[ntxt.len-1] = realloc(
      ntxt.p[ntxt.len-1], 
      strlen(ntxt.p[ntxt.len-1]) + strlen(txt->p[y]) -x +1);
  strcat(ntxt.p[ntxt.len-1], &txt->p[y][x]);

  // first line
  int old_len = strlen(ntxt.p[0]);
  ntxt.p[0] = realloc(ntxt.p[0], strlen(ntxt.p[0]) + x +1);
  memmove(&ntxt.p[0][x], ntxt.p[0], old_len +1);
  memcpy(ntxt.p[0], txt->p[y], x);

  // copy
  free(txt->p[y]);
  old_len = txt->len;
  txt->len += ntxt.len -1;
  txt->p = realloc(txt->p, txt->len*sizeof(char*));
  // what if y+1 out of bounds??
  if(y<old_len-1)
    memmove(&txt->p[y+ntxt.len], &txt->p[y+1], (old_len-y-1)*sizeof(char*));
  memcpy(&txt->p[y], ntxt.p, ntxt.len*sizeof(char*));
  free(ntxt.p);
}

struct delete_cmd text_insert(struct text *txt, struct insert_cmd cmd) {
  _text_insert(txt, cmd);
  struct delete_cmd retval = insert_cmd_inverse(txt, &cmd);
  //text_deinit(&cmd.txt);
  // TODO BEGIN: this is only for backwards compatibility, get rid of it
  for(int i=0; i<cmd.txt.len; i++)
    free(cmd.txt.p[i]);
  free(cmd.txt.p);
  cmd.txt.p = NULL;
  cmd.txt.len = 0;
  // TODO END: this is only for backwards compatibility, get rid of it
  return retval;
}

// takes ownership of cmd.txt
struct delete_cmd text_insert_old(struct text *txt, struct insert_cmd cmd) {
  int x = cmd.x; int y = cmd.y; struct text ntxt = cmd.txt;
  int yy = y+ntxt.len-1;
  //int xx = ntxt.p[ntxt.len-1].len-1 + (y==yy? x : 0);

  // backup the rest of the insertion line for later
  int aux_len = strlen(txt->p[y]) - x;
  char *aux = NULL;
  if(aux_len){
    int l = strlen(txt->p[y]) - x +1;
    aux = malloc(l);
    strcpy(aux, &txt->p[y][x]);
  }

  // copy the first line
  txt->p[y][x] = '\0';
  int l = strlen(ntxt.p[0]) + x + 1;
  txt->p[y] = realloc(txt->p[y], l);
  strcat(txt->p[y], ntxt.p[0]);

  // copy rest of lines
  if(ntxt.len > 1){
    // make room
    txt->p = realloc(txt->p, (txt->len + ntxt.len -1)*sizeof(char*));
    memmove(&txt->p[yy+1], &txt->p[y+1], (txt->len - y -1)*sizeof(char*));
    // copy
    memcpy(&txt->p[y+1], &ntxt.p[1], (ntxt.len -1)*sizeof(char*));
    txt->len += ntxt.len -1;
  }

  // append the line we backed up before
  if(aux){
    int l = strlen(txt->p[yy]) + aux_len +1;
    txt->p[yy] = realloc(txt->p[yy], l);
    strcat(txt->p[yy], aux);
    free(aux);
  }

  struct delete_cmd retval = insert_cmd_inverse(txt, &cmd);
  free(ntxt.p[0]);
  free(ntxt.p);
  return retval;
}

struct insert_cmd text_delete(struct text *txt, const struct delete_cmd cmd) {
  struct insert_cmd retval = delete_cmd_inverse(txt, &cmd);

  int x = cmd.x; int y = cmd.y; int xx = cmd.xx; int yy = cmd.yy;
  if(yy == txt->len-1 && xx == (int)strlen(txt->p[yy]))
    xx--;

  if(x == (int)strlen(txt->p[y])){} // TODO: is there anything to do here
  if(xx == (int)strlen(txt->p[yy])){
    yy++;
    xx=-1;
  }

  int len_a = x;
  int len_b = (strlen(txt->p[yy]) - 1 - xx);
  int len = len_a + len_b;
  txt->p[y] = realloc(txt->p[y], len + 1);
  if(yy < txt->len)
    memmove(&txt->p[y][x], &txt->p[yy][xx+1], len_b);
  txt->p[y][len] = '\0';

  if(y < yy){
    int lines_to_delete = yy - y;
    for(int i=0; i<lines_to_delete; i++)
      if(y+1+i < txt->len)
	free(txt->p[y+1+i]);
    if(yy+1 < txt->len){
      memmove(
	  &txt->p[y+1],
	  &txt->p[yy+1],
	  (txt->len-yy)*sizeof(char*));
    }
    txt->len -=lines_to_delete;
    txt->p = realloc(txt->p, txt->len*sizeof(char*));
  }

  if(txt->len==0){
    txt->len=1;
    txt->p = malloc(sizeof(char*));
    txt->p[0] = calloc(1,1);
  }

  return retval;
}

static inline struct insert_cmd delete_cmd_inverse(const struct text *txt, const struct delete_cmd *cmd) {
  int x = cmd->x; int y = cmd->y; int xx = cmd->xx; int yy = cmd->yy;
  struct insert_cmd ret;
  ret.x = x; ret.y = y;

  // copy all lines involved
  int extra_line = xx == (int)strlen(txt->p[yy]) && yy<txt->len-1 ? 1:0;
  int len = ret.txt.len =  yy - y + 1 + extra_line;
  ret.txt.p = malloc(len*sizeof(char*));
  for(int i=0; i<len-extra_line; i++){
    int l = strlen(txt->p[y+i]) +1;
    ret.txt.p[i] = malloc(l);
    strcpy(ret.txt.p[i], txt->p[y+i]);
  }
  if(extra_line){
    ret.txt.p[len-1] = calloc(1,1);
  }

  // delete parts we don't need
  // first line
  char *aux = ret.txt.p[0];
  int l = strlen(ret.txt.p[0]) -x +1;
  ret.txt.p[0] = malloc(l);
  strcpy(ret.txt.p[0], &aux[x]);
  free(aux);
  // last line
  if(!extra_line){
    if(y==yy) xx -= x;
    int xxx = MIN(xx+1, (int)strlen(ret.txt.p[len-1]));
    ret.txt.p[len-1] = realloc(ret.txt.p[len-1], xxx+1);
    ret.txt.p[len-1][xxx] = '\0';
  }

  return ret;
}

static inline struct delete_cmd insert_cmd_inverse(const struct text *txt, const struct insert_cmd *cmd) {
  struct text ntxt = cmd->txt;
  int x = cmd->x; int y = cmd->y;
  int yy = y + ntxt.len-1;
  int xx = strlen(ntxt.p[ntxt.len-1]) -1 + (y==yy? x: 0);
  if(xx == -1){
    yy = MAX(yy-1, 0);
    xx = strlen(txt->p[yy]);
  }
  return (struct delete_cmd){.x = x, .y = y, .xx = xx, .yy = yy};
}

