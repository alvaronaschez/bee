#ifndef TEXT_UTIL_H
#define TEXT_UTIL_H

#include "text.h"

void string_init(struct string*);
void string_deinit(struct string*);
struct string *string_create(void);
void string_destroy(struct string*);
void string_append(struct string*, const char*);
void string_prepend(struct string *s, const char *t);
void string_clone(struct string *dest, const struct string *s);

void text_init(struct text*);
struct text *text_create(void);
void text_deinit(struct text*);
void text_destroy(struct text*);
void text_append(struct text*, struct string);
void text_prepend(struct text*, struct string);
void text_clone(struct text *dest, const struct text *t);

/**
 * @brief Splits a string with linesbreak into a text struct
 *
 * @warning Takes ownership of `str`.
 * The caller must not use or free `str` after this call.
 */
struct text text_from_string(struct string *str, int nlines);

#define bytelen utf8len
int utf8len(const char* s);
int columnlen(const char* s, int col_off);

int utf8prevn(const char* s, int off, int n);
int utf8nextn(const char* s, int off, int n);
int utf8next(const char* s, int off);
int utf8prevn(const char* s, int off, int n);
int utf8prev(const char* s, int off);

char *skip_n_col(char *s, int n);

void vx_to_bx(const char *str, int vxgoal, int *bx, int *vx);
//void bx_to_vx(const char *str, int vxgoal, int *bx, int *vx){}

#endif

