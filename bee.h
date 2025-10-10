#ifndef BEE_H
#define BEE_H

#include "text.h"

#define LOCALE "en_US.UTF-8"
#define FG_COLOR TB_WHITE
#define BG_COLOR TB_BLACK
#define TAB_LEN 8
#define FOOTER_HEIGHT 1
#define FOOTER_FG TB_MAGENTA
#define FOOTER_BG TB_BLACK
#define MARGIN_LEN 4
#define MARGIN_FG TB_MAGENTA
#define MARGIN_BG TB_BLACK

#define SCREEN_HEIGHT (tb_height() - FOOTER_HEIGHT)
#define SCREEN_WIDTH (tb_width())

#define YY bee->y
#define XX bee->bx
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define ABS(a) ((a)>=0?(a):-(a))

enum mode {
  NORMAL, INSERT, COMMAND
};

extern const char *mode_label[3];

enum operation {INS, DEL};
struct change_stack{
  int y, bx, vx, leftcol, toprow;
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

  int toprow, leftcol;
  int y, bx, vx, vxgoal;

  struct string ins_buf;
  int ins_y, ins_bx, ins_vx;
  int ins_toprow, ins_leftcol;

  struct string cmd_buf;

  struct change_stack *undo_stack, *redo_stack;

  char quit;
};

int bee(const char*);

#endif

