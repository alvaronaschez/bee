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
  int x = cmd->x; int y = cmd->y; int xx = cmd->xx; int yy = cmd->yy;
  struct insert_cmd ret;
  ret.x = x; ret.y = y;

  // copy all lines involved
  int extra_line = xx == txt->p[yy].len ? 1:0; 
  int len = ret.txt.len =  yy - y + 1 + extra_line;
  ret.txt.p = malloc(len*sizeof(struct string));
  for(int i=0; i<len-extra_line; i++){
    ret.txt.p[i].p = malloc(txt->p[y+i].len +1);
    strcpy(ret.txt.p[i].p, txt->p[y+i].p);
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
  ret.txt.p[0].p = malloc(ret.txt.p[0].len+1);
  strcpy(ret.txt.p[0].p, &aux[x]);
  free(aux);
  // last line
  if(!extra_line){
    ret.txt.p[len-1].p = realloc(ret.txt.p[len-1].p, xx+1);
    ret.txt.p[len-1].p[xx] = '\0';
    ret.txt.p[len-1].len = xx;
    ret.txt.p[len-1].cap = ret.txt.p[len-1].len;
  }

  return ret;
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
  txt->p[y].len = txt->p[y].cap = x + ntxt.p[0].len;
  txt->p[y].p = realloc(txt->p[y].p, txt->p[y].len + 1);
  strcat(txt->p[y].p, ntxt.p[0].p);

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
  txt->p[yy].len = txt->p[yy].cap = txt->p[yy].len + aux_len;
  txt->p[yy].p = realloc(txt->p[yy].p, txt->p[yy].len +1);
  strcat(txt->p[yy].p, aux);

  struct delete_cmd retval = insert_cmd_inverse(txt, &cmd);
  free(ntxt.p[0].p);
  free(ntxt.p);
  free(aux);
  return retval;
}

struct insert_cmd text_delete(struct text *txt, struct delete_cmd cmd) {
  struct insert_cmd retval = delete_cmd_inverse(txt, &cmd);

  int x = cmd.x; int y = cmd.y; int xx = cmd.xx; int yy = cmd.yy;

  if(x == txt->p[y].len){} // TODO: is there anything to do here
  if(xx == txt->p[yy].len){
    yy++;
    xx=-1;
  } 

  int len = x + (txt->p[yy].len - 1 - xx);
  txt->p[y].p = realloc(txt->p[y].p, len + 1);
  txt->p[y].len = txt->p[y].cap = len;
  txt->p[y].p[x] = '\0';
  if(yy < txt->len)
    strcat(&txt->p[y].p[x], &txt->p[yy].p[xx+1]);

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

  return retval;
}

void assert_text_equals(const struct text *t1, const struct text *t2){
  assert(t1->len == t2->len);
  for(int i=0; i<t1->len; i++){
    assert(!strcmp(t1->p[i].p, t2->p[i].p));
    assert(t1->p[i].len == t2->p[i].len);
    assert(t1->p[i].cap == t2->p[i].cap);
  }
}

void assert_insert_cmd_equals(const struct insert_cmd *c1, const struct insert_cmd *c2) {
  assert(c1->x == c2->x);
  assert(c1->y == c2->y);
  assert_text_equals(&c1->txt, &c2->txt);
}

void assert_delete_cmd_equals(const struct delete_cmd *c1, const struct delete_cmd *c2){
  assert(c1->x == c2->x);
  assert(c1->y == c2->y);
  assert(c1->xx == c2->xx);
  assert(c1->yy == c2->yy);
}

void test_1(void) {
  struct text *original_t = text_from((char *[]){"Hola", "que", "tal"}, 3);
  struct text *t = text_from((char *[]){"Hola", "que", "tal"}, 3);
  struct text *n = text_from((char *[]){"hoho", "I am Santa!", ""}, 3);
  struct text *expected = text_from((char *[]){"Hohoho", "I am Santa!", "la", "que", "tal"}, 5);
  struct insert_cmd cmd = { .txt = *n, .y = 0, .x = 2 };
  struct delete_cmd expected_cmd = {.x=2, .y=0, .xx=11, .yy=1};

  struct delete_cmd out_cmd = text_insert(t, cmd); 

  assert_text_equals(expected, t);
  assert_delete_cmd_equals(&out_cmd, &expected_cmd);

  // undo test
  struct insert_cmd out_cmd2 = text_delete(t, out_cmd);

  assert_text_equals(original_t, t);
  assert_insert_cmd_equals(&out_cmd2, &cmd);
}
void test_del1(void){
  struct text *o = text_from((char *[]){"foo", "bar", "jam"}, 3);
  struct text *t = text_from((char *[]){"foo", "bar", "jam"}, 3);
  struct text *expected = text_from((char *[]){"foo", "br", "jam"}, 3);
  struct delete_cmd del_cmd = {.y=1, .x=1, .yy=1, .xx=1};
  struct insert_cmd ins_cmd = text_delete(t, del_cmd);
  assert_text_equals(t, expected);
  // undo
  text_insert(t, ins_cmd);
  assert_text_equals(t, o);
}
void test_del2(void){
  struct text *o = text_from((char *[]){"foo", "bar", "jam"}, 3);
  struct text *t = text_from((char *[]){"foo", "bar", "jam"}, 3);
  struct text *expected = text_from((char *[]){"foo", "barjam"}, 2);
  struct delete_cmd del_cmd = {.y=1, .x=3, .yy=1, .xx=3};
  struct insert_cmd ins_cmd = text_delete(t, del_cmd);
  assert_text_equals(t, expected);

  // test undo
  text_insert(t, ins_cmd);
  assert_text_equals(t, o);
}
void test_del3(void){
  struct text *o = text_from((char *[]){"one", "two", "three"}, 3);
  struct text *t = text_from((char *[]){"one", "two", "three"}, 3);
  struct text *expected = text_from((char *[]){"onethree"}, 1);
  struct delete_cmd del_cmd = {.y=0, .x=3, .yy=1, .xx=3};
  struct insert_cmd ins_cmd = text_delete(t, del_cmd);
  assert_text_equals(t, expected);

  // test undo
  text_insert(t, ins_cmd);
  assert_text_equals(t, o);
}
void test_del4(void){
  struct text *o = text_from((char *[]){"one", "two", "three"}, 3);
  struct text *t = text_from((char *[]){"one", "two", "three"}, 3);
  struct text *expected = text_from((char *[]){""}, 1);
  struct delete_cmd del_cmd = {.y=0, .x=0, .yy=2, .xx=4};
  struct insert_cmd ins_cmd = text_delete(t, del_cmd);
  assert_text_equals(t, expected);

  // test undo
  text_insert(t, ins_cmd);
  assert_text_equals(t, o);
}

void test_ins1(void){
  struct text *o = text_from((char *[]){"foo", "bar", "jam"}, 3);
  struct text *t = text_from((char *[]){"foo", "bar", "jam"}, 3);
  struct text *expected = text_from((char *[]){"foo", "baar", "jam"}, 3);
  struct text *ins_txt = text_from((char*[]){"a"}, 1);
  struct insert_cmd ins_cmd = {.y=1, .x=2, .txt=*ins_txt};
  struct delete_cmd del_cmd = text_insert(t, ins_cmd);
  assert_text_equals(t, expected);

    // test undo
  text_delete(t, del_cmd); 
  assert_text_equals(t, o);
}
int main(void) {
  test_1();
  test_del1();
  test_del2();
  test_del3();
  test_ins1();

  return 0;
}
