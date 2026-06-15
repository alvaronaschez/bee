#include "bee.h"

#include "termbox2.h"

#include "string.h"
#include "text.h"
#include "util.h"

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

// TODO: it is too complex, we should refactor it to make it readable
static inline void v_x(struct bee *bee){
  // clear redo stack
  change_stack_destroy(bee->redo_stack);
  bee->redo_stack = NULL;

  // ct = cursor_tail; ch = cursor_head; if ct > ch -> swap(ct, ch)
  int y0, y1, bx0, bx1, vx0, vx1, vxgoal0, vxgoal1;
  y0 = bee->y0; y1 = bee->y; bx0 = bee->bx0; bx1 = bee->bx; vx0 = bee->vx0; vx1 = bee->vx;
  vxgoal0 = bee->vxgoal0; vxgoal1 = bee->vxgoal;
  if(y0>y1 || (y0==y1 && bx0 > bx1)){
    SWAP_INT(y0,y1); SWAP_INT(bx0, bx1); SWAP_INT(vx0, vx1);
    SWAP_INT(vxgoal0, vxgoal1);
  }

  // apply change and save change to undo_stack
  struct change_stack *change = malloc(sizeof(struct change_stack));
  *change = (struct change_stack){
    .y = y0, .bx = bx0, .vx = vx0,
    .op = INS,
  };
  int blen =  bytelen(&bee->buf.p[bee->y][bee->bx]);
  change->cmd.i = text_delete(&bee->buf,
      (struct delete_cmd){.x=bx0, .y=y0, .xx=bx1+blen-1, .yy=y1});
  struct change_stack *old_undo_stack = bee->undo_stack;
  bee->undo_stack = change;
  bee->undo_stack->next = old_undo_stack;

  bee->y = y0;
  bee->bx = bx0;
  bee->vx = vx0;
  bee->vxgoal = bee->vxgoal0;

  if(bee->bx != 0 && bee->bx == (int)strlen(bee->buf.p[bee->y]))
    bee_move_cursor_left(bee);
  if(bee->y == bee->buf.len){
    bee_move_cursor_up(bee, 1);
  }

  exit_visual_mode(bee);
}

static inline void bee_copy(struct bee *bee){
  int y, yy, x, xx;
  y = bee->y0; yy = bee->y; x = bee->bx0; xx = bee->bx;
  if(y>yy || (y==yy && x > xx)){
    SWAP_INT(y,yy); SWAP_INT(x, xx);
  }

  // in case bx is past eol
  x = MIN(MAX(0,(int)strlen(bee->buf.p[y])-1), x);
  xx = MIN(MAX(0,(int)strlen(bee->buf.p[yy])-1), xx);

  text_deinit(&bee->clipboard);
  text_init(&bee->clipboard);

  // TODO: this could be refactor into:
  // text_copy_range_into(&bee->buf, &bee->clipboard, y, x, yy, xx);
  // if a function with the following signature would exist:
  // void text_copy_range_into(struct text *from, struct text *to, int, int, int, int);
  struct text *copied = text_copy_range(&bee->buf, y, x, yy, xx);
  bee->clipboard.p = copied->p;
  bee->clipboard.len = copied->len;
  free(copied);

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
  case 'y':
    bee_copy(bee);
  }
  else if(ev.key!=0) switch(ev.key){
  case TB_KEY_ESC:
    exit_visual_mode(bee); break;
  }
}
