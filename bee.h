#ifndef BEE_H
#define BEE_H

#include "text.h"

#define LOCALE "en_US.UTF-8"

#define FG_COLOR TB_WHITE
#define BG_COLOR TB_BLACK

#define TAB_LEN 8

#define FOOTER_HEIGHT 1
#define FOOTER_FG ALT_FG_COLOR
#define FOOTER_BG BG_COLOR

#define MARGIN_LEN 4
#define MARGIN_FG ALT_FG_COLOR
#define MARGIN_BG BG_COLOR

#define ALT_FG_COLOR TB_GREEN
#define ALT_BG_COLOR TB_BLACK

#define SCREEN_HEIGHT (tb_height() - FOOTER_HEIGHT)
#define SCREEN_WIDTH (tb_width() - MARGIN_LEN)

#define YY (bee->y)
#define XX (bee->bx)

enum mode {
  NORMAL, INSERT, COMMAND,
  VISUAL, SEARCH,
};
#define NUM_MODES 5

extern const char *mode_label[NUM_MODES];

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

  // cursor position
  // bx is the x position in the file (byte x)
  // vx is the x position in the screen (visual x)
  int y, bx, vx, vxgoal;

  // anchor or cursor tail in visual mode
  int y0, bx0, vx0, vxgoal0;

  struct text ins_buf;

  char* cmd_buf;

  struct change_stack *undo_stack, *redo_stack;

  char quit;
};

int bee(const char*);

void change_stack_destroy(struct change_stack *cs);
#endif

