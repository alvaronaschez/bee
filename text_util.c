#ifndef TEXT_UTIL_H
#define TEXT_UTIL_H

#include "text_util.h"
#include "bee.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <assert.h>

#define bytelen utf8len
 int utf8len(const char* s){
  if((s[0]&0x80) == 0x00) return 1; // 0xxx_xxxx
  if((s[0]&0xE0) == 0xC0) return 2; // 110x_xxxx 10xx_xxxx
  if((s[0]&0xF0) == 0xE0) return 3; // 1110_xxxx 10xx_xxxx 10xx_xxxx
  if((s[0]&0xF8) == 0xF0) return 4; // 1111_0xxx 10xx_xxxx  10xx_xxxx 10xx_xxxx
  return 0;
}

int columnlen(const char* s, int col_off){
  // we need to know the col_off in order to compute the length of the tab char
  if(*s=='\t')
    return TAB_LEN-col_off%TAB_LEN;
  wchar_t wc;
  mbtowc(&wc, s, MB_CUR_MAX);
  int width = wcwidth(wc);
  assert(width>=0); // -1 -> invalid utf8 char
  return 1;
}

int utf8prevn(const char* s, int off, int n);
int utf8nextn(const char* s, int off, int n){
  if(n<0) return utf8prevn(s, off, -n);
  int len;
  for(; n>0; n--){
    len = utf8len(s+off);
    if(len == -1) return -1;
    if(s[off+len] == '\0') return off;
    off+=len;
  }
  return off;
}
int utf8next(const char* s, int off){
  return utf8nextn(s, off, 1);
}
int utf8prevn(const char* s, int off, int n){
  if(n<0) return utf8nextn(s, off, -n);
  int i;
  for(; n>0; n--){
    for(i=0; i<4 && (s[off]&0xC0)==0x80; off--, i++);
    if(utf8len(s+off) == 0) return -1;
    if(off == 0) return 0;
  }
  return off;
}
int utf8prev(const char* s, int off){
  if(*s == '\0') return 0;
  off--;
  while((s[off]&0xC0) == 0x80)
    off--;
  return off;
}

char *skip_n_col(char *s, int n){
  if(s==NULL || s[0]=='\0') return NULL;
  while(n > 0){
    n -= columnlen(s, 0);
    s += bytelen(s);
    if(*s=='\0') return NULL;
  }
  return s;
}

void vx_to_bx(const char *str, int vxgoal, int *bx, int *vx){
  *bx = *vx = 0;
  int bxold;
  if(*str == '\0') return;
  while(1){
    if(str[*bx+bytelen(str+*bx)] == '\0') return;
    if(*vx == vxgoal) return;
    if(*vx+columnlen(str+*bx, *vx) > vxgoal) return;
    bxold = *bx;
    *bx += bytelen(str+*bx);
    *vx += columnlen(str+bxold, *vx);
  }
}

// void bx_to_vx(const char *str, int vxgoal, int *bx, int *vx){}

void string_init(struct string *s){
  s->cap = 8;
  s->len = 0;
  s->p = calloc(s->cap+1, sizeof(char));
}

void string_deinit(struct string *s){
  free(s->p);
  s->p = NULL;
  s->cap = 0;
  s->len = 0;
}

struct string *string_create(void){
  struct string *s = malloc(sizeof(struct string));
  string_init(s);
  return s;
}

void string_destroy(struct string *s){
  string_deinit(s);
  free(s);
}

void string_append(struct string *s, const char *t){
  int new_len = s->len + strlen(t);
  int new_cap = s->cap;
  while(new_len > new_cap)
    new_cap *=2;
  if(new_cap > s->cap){
    s->cap = new_cap;
    s->p = realloc(s->p, s->cap+1);
  }
  strcat(s->p + s->len, t);
  s->len = new_len;
}

void string_prepend(struct string *s, const char *t){
  s->len += strlen(t);
  char *old = s->p;
  s->p = calloc(s->len + 1, sizeof(char));
  strcat(s->p, t);
  strcat(s->p, old);
  free(old);
}

void string_clone(struct string *dest, const struct string *s){
  if(dest==s || !dest || !s)
    return;
  dest->len = s->len;
  dest->cap = s->cap;
  dest->p = malloc(s->cap+1);
  memcpy(dest->p, s->p, s->cap+1);
}

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
    free(t->p[i].p);
  if(t->p)
    free(t->p);
  t->len = 0;
}

void text_destroy(struct text *t){
  text_deinit(t);
  free(t);
}

void text_append(struct text *t, struct string s){
  t->len++;
  t->p = realloc(t->p, t->len*sizeof(struct string));
  t->p[t->len-1] = s;
}

void text_prepend(struct text *t, struct string s){
  if(t->len==0){
    text_append(t, s);
    return;
  }
  t->len++;
  t->p = realloc(t->p, t->len*sizeof(struct string));
  memmove(&t->p[1], t->p, (t->len-1)*sizeof(struct string));
  t->p[0] = s;
}

void text_clone(struct text *dest, const struct text *t){
  if(dest == t || !dest || !t)
    return;
  dest->len = t->len;
  dest->p = malloc(t->len*sizeof(struct string));
  for(int i=0; i<t->len; i++){
    string_clone(&dest->p[i], &t->p[i]);
  }
}

/**
 * @brief Splits a string with linesbreak into a text struct
 *
 * @warning Takes ownership of `str`.
 * The caller must not use or free `str` after this call.
 */
struct text text_from_string(struct string *str, int nlines){
  struct text retval;
  char *s = str->p;
  retval.p = malloc(nlines*sizeof(struct string));
  retval.len = nlines;

  for(int i=0; i<nlines; i++){
    char *end = strchr(s, '\n');
    end = end ? end : s + strlen(s);
    retval.p[i].p = malloc(end-s+1);
    memcpy(retval.p[i].p, s, end-s);
    retval.p[i].p[end-s] = '\0'; // null terminated string
    retval.p[i].cap = retval.p[i].len = end-s;
    s = end+1;
  }

  string_deinit(str);
  return retval;
}

#endif

