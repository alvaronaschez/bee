#include "text_util.h"
#include "bee.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <assert.h>

void text_init(struct text *t){
  t->len = 0;
  t->p = NULL;
}

struct text *text_create(void){
  struct text *t = malloc(sizeof(struct text));
  text_init(t);
  return t;
}

void text_deinit(struct text *t){
  for(int i=0; i<t->len; i++)
    free(t->p[i]);
  free(t->p);
  t->p = NULL;
  t->len = 0;
}

void text_destroy(struct text *t){
  text_deinit(t);
  free(t);
}

