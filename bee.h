#ifndef BEE_H
#define BEE_H

#include "text.h"

#define LOCALE "en_US.UTF-8"
#define FG_COLOR TB_BLACK
#define BG_COLOR TB_WHITE
#define TAB_LEN 8
#define FOOTER_HEIGHT 1
#define FOOTER_FG BG_COLOR
#define FOOTER_BG FG_COLOR
#define MARGIN_LEN 4
#define MARGIN_FG BG_COLOR
#define MARGIN_BG FG_COLOR

#define SCREEN_HEIGHT (tb_height() - FOOTER_HEIGHT)
#define SCREEN_WIDTH (tb_width() - MARGIN_LEN)

#define YY (bee->y)
#define XX (bee->bx)
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define ABS(a) ((a)>=0?(a):-(a))

enum mode {
  NORMAL, INSERT, COMMAND
};

extern const char *mode_label[3];

enum operation {INS, DEL};
struct change_stack{
  int y, bx, vx;
  struct change_stack *next;
  enum operation op;
  union {
    struct insert_cmd i;
    struct delete_cmd d;
  } cmd;
};

struct bee {
  enum mode mode;
  struct text buf;
  char *filename;

  int y, bx, vx, vxgoal;

  struct text ins_buf;

  char* cmd_buf;

  struct change_stack *undo_stack, *redo_stack;

  char quit;
};

int bee(const char*);

#endif

