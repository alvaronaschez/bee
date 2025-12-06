#ifndef BEE_TEXT_H
#define BEE_TEXT_H

//struct string {
//  char *p; // null terminated
//  int len, cap;
//};

struct text {
  char** p;
  int len;
};

struct insert_cmd {
  struct text txt;
  int x, y;
};

struct delete_cmd {
  int x, y, xx, yy;
};

// takes ownership of cmd.txt
struct delete_cmd text_insert(struct text*, struct insert_cmd); 

struct insert_cmd text_delete(struct text*, const struct delete_cmd); 

#endif

