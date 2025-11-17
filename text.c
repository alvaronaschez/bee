#include "text.h"

#include <stdlib.h>
#include <string.h>

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))


static inline struct insert_cmd delete_cmd_inverse(const struct text*, const struct delete_cmd*);
static inline struct delete_cmd insert_cmd_inverse(const struct text*, const struct insert_cmd*);

// takes ownership of cmd.txt
struct delete_cmd text_insert(struct text *txt, struct insert_cmd cmd) {
  int x = cmd.x; int y = cmd.y; struct text ntxt = cmd.txt;
  int yy = y+ntxt.len-1;
  //int xx = ntxt.p[ntxt.len-1].len-1 + (y==yy? x : 0);

  // backup the rest of the insertion line for later
  int aux_len = txt->p[y].len - x;
  char *aux = NULL;
  if(aux_len){
    int l = txt->p[y].len - x +1;
    aux = malloc(l);
    strlcpy(aux, &txt->p[y].p[x], l);
  }

  // copy the first line
  txt->p[y].p[x] = '\0';
  txt->p[y].len = txt->p[y].cap = x + ntxt.p[0].len;
  int l = txt->p[y].len + 1;
  txt->p[y].p = realloc(txt->p[y].p, l);
  strlcat(txt->p[y].p, ntxt.p[0].p, l);

  // copy rest of lines
  if(ntxt.len > 1){
    // make room
    txt->p = realloc(txt->p, (txt->len + ntxt.len -1)*sizeof(struct string));
    memmove(&txt->p[yy+1], &txt->p[y+1], (txt->len - y -1)*sizeof(struct string));
    // copy
    memcpy(&txt->p[y+1], &ntxt.p[1], (ntxt.len -1)*sizeof(struct string));
    txt->len += ntxt.len -1;
  }

  // append the line we backed up before
  if(aux){
    txt->p[yy].len = txt->p[yy].cap = txt->p[yy].len + aux_len;
    int l = txt->p[yy].len +1;
    txt->p[yy].p = realloc(txt->p[yy].p, l);
    strlcat(txt->p[yy].p, aux, l);
    free(aux);
  }

  struct delete_cmd retval = insert_cmd_inverse(txt, &cmd);
  free(ntxt.p[0].p);
  free(ntxt.p);
  return retval;
}

struct insert_cmd text_delete(struct text *txt, const struct delete_cmd cmd) {
  struct insert_cmd retval = delete_cmd_inverse(txt, &cmd);

  int x = cmd.x; int y = cmd.y; int xx = cmd.xx; int yy = cmd.yy;
  if(yy == txt->len-1 && xx == txt->p[yy].len)
    xx--;

  if(x == txt->p[y].len){} // TODO: is there anything to do here
  if(xx == txt->p[yy].len){
    yy++;
    xx=-1;
  }

  int len_a = x;
  int len_b = (txt->p[yy].len - 1 - xx);
  int len = len_a + len_b;
  if(yy < txt->len)
    memmove(&txt->p[y].p[x], &txt->p[yy].p[xx+1], len_b);
    // alternative way:
    //memmove(&txt->p[y].p[x], &txt->p[yy].p[xx+1], strlen(&txt->p[yy].p[xx+1]));
  txt->p[y].p[len] = '\0';
  txt->p[y].p = realloc(txt->p[y].p, len + 1); // here
  txt->p[y].len = txt->p[y].cap = len;

  if(y < yy){
    int lines_to_delete = yy - y;
    for(int i=0; i<lines_to_delete; i++)
      if(y+1+i < txt->len)
	free(txt->p[y+1+i].p);
    if(yy+1 < txt->len){
      memmove(
	  &txt->p[y+1],
	  &txt->p[yy+1],
	  (txt->len-yy)*sizeof(struct string));
    }
    txt->len -=lines_to_delete;
    txt->p = realloc(txt->p, txt->len*sizeof(struct string));
  }

  if(txt->len==0){
    txt->len=1;
    txt->p = malloc(sizeof(struct string));
    txt->p[0].p = calloc(1,1);
    txt->p[0].len = txt->p[0].cap = 0;
  }

  return retval;
}

static inline struct insert_cmd delete_cmd_inverse(const struct text *txt, const struct delete_cmd *cmd) {
  int x = cmd->x; int y = cmd->y; int xx = cmd->xx; int yy = cmd->yy;
  struct insert_cmd ret;
  ret.x = x; ret.y = y;

  // copy all lines involved
  int extra_line = xx == txt->p[yy].len && yy<txt->len-1 ? 1:0;
  int len = ret.txt.len =  yy - y + 1 + extra_line;
  ret.txt.p = malloc(len*sizeof(struct string));
  for(int i=0; i<len-extra_line; i++){
    int l = txt->p[y+i].len +1;
    ret.txt.p[i].p = malloc(l);
    strlcpy(ret.txt.p[i].p, txt->p[y+i].p, l);
    ret.txt.p[i].len = txt->p[y+i].len;
    ret.txt.p[i].cap = txt->p[y+i].cap;
  }
  if(extra_line){
    ret.txt.p[len-1].p = calloc(1,1);
    ret.txt.p[len-1].len = ret.txt.p[len-1].cap = 0;
  }

  // delete parts we don't need
  // first line
  char *aux = ret.txt.p[0].p;
  ret.txt.p[0].len -= x;
  ret.txt.p[0].cap = ret.txt.p[0].len;
  int l = ret.txt.p[0].len+1;
  ret.txt.p[0].p = malloc(l);
  strlcpy(ret.txt.p[0].p, &aux[x], l);
  free(aux);
  // last line
  if(!extra_line){
    if(y==yy) xx -= x;
    int xxx = MIN(xx+1, ret.txt.p[len-1].len);
    ret.txt.p[len-1].p = realloc(ret.txt.p[len-1].p, xxx+1);
    ret.txt.p[len-1].p[xxx] = '\0';
    ret.txt.p[len-1].len = xxx;
    ret.txt.p[len-1].cap = ret.txt.p[len-1].len;
  }

  return ret;
}

static inline struct delete_cmd insert_cmd_inverse(const struct text *txt, const struct insert_cmd *cmd) {
  struct text ntxt = cmd->txt;
  int x = cmd->x; int y = cmd->y;
  int yy = y + ntxt.len-1;
  int xx = ntxt.p[ntxt.len-1].len -1 + (y==yy? x: 0);
  if(xx == -1){
    yy--;
    xx = txt->p[yy].len;
  }
  return (struct delete_cmd){.x = x, .y = y, .xx = xx, .yy = yy};
}

