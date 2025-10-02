#include "text.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

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
void test_del5(void){
  struct text *o = text_from((char *[]){"one", "two", "three"}, 3);
  struct text *t = text_from((char *[]){"one", "two", "three"}, 3);
  struct text *expected = text_from((char *[]){""}, 1);
  struct delete_cmd del_cmd = {.y=0, .x=0, .yy=2, .xx=5};
  struct insert_cmd ins_cmd = text_delete(t, del_cmd);
  assert_text_equals(t, expected);

  // test undo
  text_insert(t, ins_cmd);
  assert_text_equals(t, o);
}
void test_del6(void){
  struct text *o = text_from((char *[]){""}, 1);
  struct text *t = text_from((char *[]){""}, 1);
  struct text *expected = text_from((char *[]){""}, 1);
  struct delete_cmd del_cmd = {.y=0, .x=0, .yy=0, .xx=0};
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
void test_ins2(void){
  struct text *o = text_from((char *[]){"foo", "bar", "jam"}, 3);
  struct text *t = text_from((char *[]){"foo", "bar", "jam"}, 3);
  struct text *expected = text_from((char *[]){"foo", "barbar", "jam"}, 3);
  struct text *ins_txt = text_from((char*[]){"bar"}, 1);
  struct insert_cmd ins_cmd = {.y=1, .x=0, .txt=*ins_txt};
  struct delete_cmd del_cmd = text_insert(t, ins_cmd);
  assert_text_equals(t, expected);

  // test undo
  text_delete(t, del_cmd); 
  assert_text_equals(t, o);
}
void test_ins3(void){
  struct text *o = text_from((char *[]){"foo", "bar", "jam"}, 3);
  struct text *t = text_from((char *[]){"foo", "bar", "jam"}, 3);
  struct text *expected = text_from((char *[]){"foo", "barcar", "jam"}, 3);
  struct text *ins_txt = text_from((char*[]){"car"}, 1);
  struct insert_cmd ins_cmd = {.y=1, .x=3, .txt=*ins_txt};
  struct delete_cmd del_cmd = text_insert(t, ins_cmd);
  assert_text_equals(t, expected);

  // test undo
  text_delete(t, del_cmd); 
  assert_text_equals(t, o);
}
void test_ins4(void){
  struct text *o = text_from((char *[]){"foo", "bar", "jam"}, 3);
  struct text *t = text_from((char *[]){"foo", "bar", "jam"}, 3);
  struct text *expected = text_from((char *[]){"foo", "bar", "bla", "bla", "jam"}, 5);
  struct text *ins_txt = text_from((char*[]){"", "bla", "bla"}, 3);
  struct insert_cmd ins_cmd = {.y=1, .x=3, .txt=*ins_txt};
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
  test_del4();
  test_del5();
  test_del6();

  test_ins1();
  test_ins2();
  test_ins3();
  test_ins4();

  return 0;
}
