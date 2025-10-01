#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

struct string {
  char *p;
  int len, cap;
};

struct text {
  struct string *p;
  int len;
};

struct insert_cmd {
  struct text txt;
  int x, y;
};

struct delete_cmd {
  int x, y, xx, yy;
};

struct text *text_from(char **arr, int len){
  struct text *t = malloc(sizeof(struct text));
  t->p = malloc(len*sizeof(struct string));

  for(int i=0; i<len; i++){
    t->p[i].len = t->p[i].cap = strlen(arr[i]);
    t->p[i].p = malloc(t->p[i].len+1);
    strcpy(t->p[i].p, arr[i]);
  }
  t->len = len;
  return t;
}

struct insert_cmd delete_cmd_inverse(const struct text *txt, const struct delete_cmd *cmd) {
  return (struct insert_cmd){
  };
}

struct delete_cmd insert_cmd_inverse(const struct text *txt, const struct insert_cmd *cmd) {
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

// takes ownership of cmd.txt
struct delete_cmd text_insert(struct text *txt, struct insert_cmd cmd) {
  int x = cmd.x; int y = cmd.y; struct text ntxt = cmd.txt;
  int yy = y+ntxt.len-1;
  int xx = ntxt.p[ntxt.len-1].len-1 + (y==yy? x : 0);

  // backup the rest of the insertion line for later
  int aux_len = txt->p[y].len - x;
  char *aux = malloc(txt->p[y].len - x +1);
  strcpy(aux, &txt->p[y].p[x]);

  // copy the first line
  txt->p[y].p[x] = '\0';
  txt->p[y].p = realloc(txt->p[y].p, x + ntxt.p[0].len + 1);
  strcat(txt->p[y].p, ntxt.p[0].p);
  free(ntxt.p[0].p);

  // copy rest of lines
  if(ntxt.len > 1){
    // make room
    txt->p = realloc(txt->p, (txt->len + ntxt.len -1)*sizeof(struct string));
    memmove(&txt->p[yy+1], &txt->p[y+1], (txt->len - y -1)*sizeof(struct string));
    // copy
    memcpy(&txt->p[y+1], &ntxt.p[1], (ntxt.len -1)*sizeof(struct string));
    txt->len += ntxt.len -1;
  }
  free(ntxt.p);

  // append the line we backed up before
  txt->p[yy].len = txt->p[yy].cap = txt->p[yy].len + aux_len;
  txt->p[yy].p = realloc(txt->p[yy].p, txt->p[yy].len +1);
  strcat(txt->p[yy].p, aux);
  free(aux);

  struct delete_cmd retval = insert_cmd_inverse(txt, &cmd);
  return retval;
}

struct insert_cmd text_delete(struct text *txt, struct delete_cmd cmd) {
  struct insert_cmd retval = delete_cmd_inverse(txt, &cmd);
  return retval;
}

void test_insert(void) {
  struct text *t = text_from((char *[]){"hola", "que", "tal"}, 3);
  struct text *n = text_from((char *[]){"hoho", "I am Santa!", ""}, 3);
  struct text *expected = text_from((char *[]){"hohoho", "I am Santa!", "la", "que", "tal"}, 5);
  struct insert_cmd cmd = { .txt = *n, .y = 0, .x = 2 };
  struct delete_cmd expected_cmd = {.x=2, .y=0, .xx=11, .yy=1};
  struct delete_cmd out_cmd = text_insert(t, cmd); 

  for(int i=0; i<t->len; i++){
    assert(!strcmp(expected->p[i].p, t->p[i].p));
  }

  assert(out_cmd.y == expected_cmd.y);
  assert(out_cmd.x == expected_cmd.x);
  assert(out_cmd.yy == expected_cmd.yy);
  assert(out_cmd.xx == expected_cmd.xx);
}

int main(void) {
  test_insert();

  return 0;
}
