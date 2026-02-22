//#ifndef STRING_H
//#define STRING_H

#include "bee.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <wchar.h>

#define COMMENT(ignored)

static inline char *str_cat(const char *s1, const char *s2){
  char *s = malloc(strlen(s1) + strlen(s2) + 1);
  s[0] = '\0';
  strcat(s, s1);
  strcat(s, s2);
  return s;
}

static inline char *str_cat3(const char *s1, const char *s2, const char *s3){
  char *s = malloc(strlen(s1) + strlen(s2) + strlen(s3) + 1);
  s[0] = '\0';
  strcat(s, s1);
  strcat(s, s2);
  strcat(s, s3);
  return s;
}

static inline char *str_copy(const char* str){
  char *retval = malloc(strlen(str) + 1);
  strcpy(retval, str);
  return retval;
}

static inline char *str_copy_from(const char *this, int from){
  /* copy 'this' starting from position 'from' */
  int new_len = strlen(this) - from;
  char *new = malloc(new_len + 1);
  memcpy(new, &this[from], new_len);
  new[new_len] = '\0';
  return new;
}

static inline char *str_copy_n(const char *this, int n){
  /* copy first 'n' characters in 'this' */
  int new_len = n;
  char *new = malloc(new_len + 1);
  memcpy(new, this, new_len);
  new[new_len] = '\0';
  return new;
}

static inline char *str_copy_range(const char *this, int begin, int end){
  assert(begin >= 0);
  int n0 = strlen(this);
  assert(end >= 0 && end <= n0);

  int n = end - begin;
  if(n <= 0){
    char *s = calloc(1, 1);
    return s;
  }
  char *s = malloc(n +1);
  strncpy(s, &this[begin], n);
  s[n] = '\0';
  return s;
}

#define str_delete_from(this, from) {\
  this = realloc(this, from + 1);\
  this[from] = '\0';\
}

#define str_delete_n(this, n) {\
  int old_len = strlen(this);\
  int new_len = old_len - n;\
  memmove(this, &this[n], old_len - n);\
  this = realloc(this, new_len + 1);\
}

#define str_delete_range(this, begin, end) {\
  int old_len = strlen(this);\
  int range_len = end - begin + 1;\
  int new_len = old_len - range_len;\
  memmove(&this[begin], &this[end + 1], old_len - end); COMMENT(we copy the null byte)\
  this = realloc(this, new_len + 1);\
}

#define str_append(this, other) {\
  this = realloc(this, strlen(this) + strlen(other) + 1);\
  strcat(this, other);\
}

#define str_prepend(this, other) {\
  int this_len = strlen(this);\
  int other_len = strlen(other);\
  this = realloc(this, this_len + other_len + 1);\
  memmove(&this[other_len], this, this_len + 1);\
  memcpy(this, other, other_len);\
}

//static inline void str_insert(char *this, const char *other, int i){
//  // TODO
//  // generalized str_append and str_prepend
//}

/* misc */

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
  int n = mbtowc(&wc, s, MB_CUR_MAX);
  assert(n>=0); // -1 -> invalid utf8 char
  if(n==0) return 0; // s points to a null byte ('\0')
  int width = wcwidth(wc);
  return width;
}

static inline int utf8prev(const char* s, int off){
  if(*s == '\0') return 0;
  off--;
  while((s[off]&0xC0) == 0x80)
    off--;
  return off;
}

static inline int bx_to_vx(int bx, int vx0, char* s){
  int vx = vx0;
  while(bx > 0 && *s != '\0'){
    vx += columnlen(s, vx);
    int bn = bytelen(s);
    bx -= bn;
    s += bn;
  }
  return vx;
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

static inline int vlen(char *s){
  int vx = 0;
  while(*s != '\0'){
    vx += columnlen(s, vx);
    s += bytelen(s);
  }
  return vx;
}

//#endif
