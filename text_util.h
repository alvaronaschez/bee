#ifndef TEXT_UTIL_H
#define TEXT_UTIL_H

#include "text.h"

//#define bytelen utf8len
//int utf8len(const char* s);
//int columnlen(const char* s, int col_off);
//
//int utf8prev(const char* s, int off);
//
//int bx_to_vx(int bx, int vx0, char* s);
//void vx_to_bx(const char *str, int vxgoal, int *bx, int *vx);
//
//int vlen(char *s);

void text_init(struct text*);
struct text *text_create(void);
void text_deinit(struct text*);
void text_destroy(struct text*);

#endif

