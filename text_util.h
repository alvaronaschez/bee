#ifndef TEXT_UTIL_H
#define TEXT_UTIL_H

#include "bee.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <assert.h>

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
  if(s->len + (int)strlen(t) > s->cap)
    s->p = realloc(s->p, s->cap*=2);
  strcat(s->p + s->len, t);
  s->len += strlen(t);
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

#define bytelen utf8len
static inline int utf8len(const char* s){
  if((s[0]&0x80) == 0x00) return 1; // 0xxx_xxxx
  if((s[0]&0xE0) == 0xC0) return 2; // 110x_xxxx 10xx_xxxx
  if((s[0]&0xF0) == 0xE0) return 3; // 1110_xxxx 10xx_xxxx 10xx_xxxx
  if((s[0]&0xF8) == 0xF0) return 4; // 1111_0xxx 10xx_xxxx  10xx_xxxx 10xx_xxxx
  return 0;
}

static inline int columnlen(const char* s, int col_off){
  // we need to know the col_off in order to compute the length of the tab char
  if(*s=='\t')
    return TAB_LEN-col_off%TAB_LEN;
  wchar_t wc;
  mbtowc(&wc, s, MB_CUR_MAX);
  int width = wcwidth(wc);
  assert(width>=0); // -1 -> invalid utf8 char
  return 1;
}

static inline int utf8prevn(const char* s, int off, int n);
static inline int utf8nextn(const char* s, int off, int n){
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
static inline int utf8next(const char* s, int off){
  return utf8nextn(s, off, 1);
}
static inline int utf8prevn(const char* s, int off, int n){
  if(n<0) return utf8nextn(s, off, -n);
  int i;
  for(; n>0; n--){
    for(i=0; i<4 && (s[off]&0xC0)==0x80; off--, i++);
    if(utf8len(s+off) == 0) return -1;
    if(off == 0) return 0;
  }
  return off;
}
static inline int utf8prev(const char* s, int off){
  if(*s == '\0') return 0;
  off--;
  while((s[off]&0xC0) == 0x80)
    off--;
  return off;
}

static inline char *skip_n_col(char *s, int n, int *remainder){
  *remainder = 0;
  if(s==NULL || s[0]=='\0') return NULL;
  while(n > 0){
    *remainder = n - columnlen(s, 0);
    n -= columnlen(s, 0);
    s += bytelen(s);
    if(*s=='\0') return NULL;
  }
  return s;
}

static inline void vx_to_bx(const char *str, int vxgoal, int *bx, int *vx){
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

//static inline void bx_to_vx(const char *str, int vxgoal, int *bx, int *vx){}

#endif

