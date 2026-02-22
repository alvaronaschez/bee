#include "text.h"

#include <stdlib.h>
#include <string.h>

#include "string.h"

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
    this->p[i] = str_copy(other->p[i]);
  }
}

void text_delete_line_range(struct text *txt, int begin, int len){
  for(int i=0; i<len; i++)
    if(begin+i < txt->len)
      free(txt->p[begin+i]);
  if(begin + len < txt->len)
    memmove(&txt->p[begin], &txt->p[begin + len], (txt->len - len)*sizeof(char*));
  txt->len -= len;
  txt->p = realloc(txt->p, txt->len*sizeof(char*));
}

static inline struct insert_cmd delete_cmd_inverse(const struct text*, const struct delete_cmd*);
static inline struct delete_cmd insert_cmd_inverse(const struct text*, const struct insert_cmd*);

void _text_insert(struct text *this, const struct insert_cmd cmd) {
  int x = cmd.x; int y = cmd.y;
  struct text other;
  text_init_from_text(&other, &cmd.txt);

  // first line
  char *first = str_copy_n(this->p[y], x);
  str_prepend(other.p[0], first);
  free(first);

  // last line
  str_append(other.p[other.len-1], &this->p[y][x]);

  // copy new text
  free(this->p[y]);
  int old_len = this->len;
  this->len += other.len -1;
  this->p = realloc(this->p, this->len*sizeof(char*));
  // what if y+1 out of bounds??
  if(y<old_len-1)
    memmove(&this->p[y+other.len], &this->p[y+1], (old_len-y-1)*sizeof(char*));
  memcpy(&this->p[y], other.p, other.len*sizeof(char*));
  free(other.p);
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

struct insert_cmd text_delete(struct text *txt, const struct delete_cmd cmd) {
  struct insert_cmd retval = delete_cmd_inverse(txt, &cmd);

  int x = cmd.x; int y = cmd.y; int xx = cmd.xx; int yy = cmd.yy;

  // if the cursor tail is at the last line and past the last character
  if(yy == txt->len-1 && xx == (int)strlen(txt->p[yy]))
    xx--;

  if(x == (int)strlen(txt->p[y])){} // TODO: is there anything to do here
  // if the cursor tail is past the last character of the line
  if(xx == (int)strlen(txt->p[yy])){
    yy++;
    xx=-1;
  }

  if(y < yy){
    str_delete_from(txt->p[y], x);
    str_append(txt->p[y], &txt->p[yy][xx+1]);
    text_delete_line_range(txt, y+1, yy - y);
  } else { // y == yy
    str_delete_range(txt->p[y], x, xx);
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
    ret.txt.p[i] = str_copy(txt->p[y+i]);
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

