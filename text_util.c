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
  int n = mbtowc(&wc, s, MB_CUR_MAX);
  assert(n>=0); // -1 -> invalid utf8 char
  if(n==0) return 0; // s points to a null byte ('\0')
  int width = wcwidth(wc);
  return width;
}

int utf8prev(const char* s, int off){
  if(*s == '\0') return 0;
  off--;
  while((s[off]&0xC0) == 0x80)
    off--;
  return off;
}

int bx_to_vx(int bx, int vx0, char* s){
  int vx = vx0;
  while(bx > 0 && *s != '\0'){
    vx += columnlen(s, vx);
    int bn = bytelen(s);
    bx -= bn;
    s += bn;
  }
  return vx;
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

char *str_cat(const char *s1, const char *s2){
  char *s = malloc(strlen(s1) + strlen(s2) + 1);
  s[0] = '\0';
  strcat(s, s1);
  strcat(s, s2);
  return s;
}

char *str_cat3(const char *s1, const char *s2, const char *s3){
  char *s = malloc(strlen(s1) + strlen(s2) + strlen(s3) + 1);
  s[0] = '\0';
  strcat(s, s1);
  strcat(s, s2);
  strcat(s, s3);
  return s;
}

// inclusive range in both ends
char *str_range(const char *s0, int begin, int end){
  int n0 = strlen(s0);
  if(end < 0)
    end = n0 + end;
  int n = end - begin +1;
  if(n > n0)
    n = n0;
  if(n < 0)
    return NULL;
  char *s = malloc(n +1);
  strncpy(s, &s0[begin], n);
  return s;
}

