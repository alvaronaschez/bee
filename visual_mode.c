#include "visual_mode.h"

#include "termbox2.h"

#include "normal_mode.h"
#include "string.h"

void to_visual_mode(struct bee *bee){
  bee->y0 = bee->y;
  bee->bx0 = bee->bx;
  bee->vx0 = bee->vx;
  bee->vxgoal0 = bee->vxgoal;

  bee->mode = VISUAL;
}

static inline void exit_visual_mode(struct bee  *bee){
  //bee->y = bee->y0;
  //bee->bx = bee->bx0;
  //bee->vx = bee->vx0;
  //bee->vxgoal = bee->vxgoal0;

  bee->y0 = bee->bx0 = bee->vx0 = bee->vxgoal0 = -1;

  bee->mode = NORMAL;
}

static inline void v_x(struct bee *bee){
  change_stack_destroy(bee->redo_stack);
  bee->redo_stack = NULL;
  struct change_stack *old_undo_stack = bee->undo_stack;

  struct change_stack *change = malloc(sizeof(struct change_stack));
  *change = (struct change_stack){
    .y = bee->y0, .bx = bee->bx0, .vx = bee->vx0,
    .op = INS,
  };
  int blen =  bytelen(&bee->buf.p[bee->y][bee->bx]);
  change->cmd.i = text_delete(&bee->buf,
      (struct delete_cmd){.x=bee->bx0, .y=bee->y0, .xx=bee->bx+blen-1, .yy=bee->y});
  bee->undo_stack = change;
  bee->undo_stack->next = old_undo_stack;

  bee->y = bee->y0;
  bee->bx = bee->bx0;
  bee->vx = bee->vx0;
  bee->vxgoal = bee->vxgoal0;

  if(bee->bx != 0 && bee->bx == (int)strlen(bee->buf.p[bee->y]))
    bee_move_cursor_left(bee);
  if(bee->y == bee->buf.len){
    bee_move_cursor_up(bee, 1);
  }

  exit_visual_mode(bee);
}

void visual_read_key(struct bee *bee){
  struct tb_event ev;
  tb_poll_event(&ev);
  if(ev.type == TB_EVENT_RESIZE) return;
  else if(ev.ch!=0) switch(ev.ch){
  case 'h':
    bee_move_cursor_left(bee); break;
  case 'j':
    bee_move_cursor_down(bee, 1); break;
  case 'k':
    bee_move_cursor_up(bee, 1); break;
  case 'l':
    bee_move_cursor_right(bee); break;
  case 'x':
  case 'd':
    v_x(bee); break;
  }
  else if(ev.key!=0) switch(ev.key){
  case TB_KEY_ESC:
    exit_visual_mode(bee); break;
  }
}
