#include "insert_mode.h"

#include "termbox2.h"

#include <stdlib.h>
#include <string.h>

#include "text_util.h"

static inline void i_esc(struct bee *bee){
  if(bee->ins_buf.len == 0){
    bee->mode = NORMAL;
    return;
  }

  int new_y = bee->y + bee->ins_buf.len-1;
  int new_bx = strlen(bee->ins_buf.p[bee->ins_buf.len-1]) + (bee->ins_buf.len == 1 ? bee->bx : 0);

  struct change_stack *change = malloc(sizeof(struct change_stack));
  *change = (struct change_stack){
    .y = bee->y, .bx = bee->bx, .vx = bee->vx,
    .op = DEL,
  };

  change->cmd.d = text_insert(&bee->buf, 
      (struct insert_cmd){.x=bee->bx, .y=bee->y, .txt=bee->ins_buf});

  change_stack_destroy(bee->redo_stack);
  bee->redo_stack = NULL;
  struct change_stack *old_undo_stack = bee->undo_stack;
  bee->undo_stack = change;
  bee->undo_stack->next = old_undo_stack;

  bee->y = new_y;
  bee->bx = new_bx;
  bee->vx = bx_to_vx(new_bx, bee->buf.p[YY]);

  bee->vxgoal = bee->vx;
  bee->mode = NORMAL;
  //text_deinit(&bee->ins_buf);
}

static inline void i_backspace(struct bee *bee){
  if(bee->ins_buf.len == 0)
    return;
  char *s = bee->ins_buf.p[bee->ins_buf.len-1];

  int n = strlen(s);
  if(n == 0)
    return;

  // TODO: this is not gonna work with utf8, assuming all chars len = 1 (ascii)
  s[n-1] = '\0';
}

void insert_read_key(struct bee *bee){
  struct tb_event ev;
  tb_poll_event(&ev);
  if(ev.type == TB_EVENT_RESIZE) return;
  else if(ev.key!=0) switch(ev.key){
    case TB_KEY_ESC:
      i_esc(bee); break;
    case TB_KEY_BACKSPACE:
    case TB_KEY_BACKSPACE2:
      i_backspace(bee); break;
    case TB_KEY_ENTER:
      ;
      bee->ins_buf.p = realloc(bee->ins_buf.p, (++bee->ins_buf.len)*sizeof(char*));
      bee->ins_buf.p[bee->ins_buf.len-1] = calloc(1,1);
      break;
    case TB_KEY_TAB:
      bee->ins_buf.p[bee->ins_buf.len-1] = realloc(
          bee->ins_buf.p[bee->ins_buf.len-1],
          strlen(bee->ins_buf.p[bee->ins_buf.len-1]) + 1);
      strcat(bee->ins_buf.p[bee->ins_buf.len-1], "\t");
  }
  else if(ev.ch){
    char s[7];
    tb_utf8_unicode_to_char(s, ev.ch);
    bee->ins_buf.p[bee->ins_buf.len-1] = realloc(
        bee->ins_buf.p[bee->ins_buf.len-1],
        strlen(bee->ins_buf.p[bee->ins_buf.len-1]) + 7);
    strcat(bee->ins_buf.p[bee->ins_buf.len-1], s);
  }
}

