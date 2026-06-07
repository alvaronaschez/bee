#include "visual_mode.h"

#include "termbox2.h"

#include "normal_mode.h"
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

// TODO: it fails when cursor tail > cursor head
// implement something like: if ct > ch -> swap(ct, ch)
// something similar is done in print.c:199
// it is too complex anyway, we should refactor it to make it readable
static inline void v_x(struct bee *bee){
  // clear redo stack
  change_stack_destroy(bee->redo_stack);
  bee->redo_stack = NULL;

  // apply change and save change to undo_stack
  struct change_stack *change = malloc(sizeof(struct change_stack));
  *change = (struct change_stack){
    .y = bee->y0, .bx = bee->bx0, .vx = bee->vx0,
    .op = INS,
  };
  int blen =  bytelen(&bee->buf.p[bee->y][bee->bx]);
  change->cmd.i = text_delete(&bee->buf,
      (struct delete_cmd){.x=bee->bx0, .y=bee->y0, .xx=bee->bx+blen-1, .yy=bee->y});
  struct change_stack *old_undo_stack = bee->undo_stack;
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

static inline void bee_copy(struct bee *bee){
  // TODO: copy to clipboard and exit visual mode
  int y = bee->y0;
  int yy = bee->y;
  int x = bee->bx0;
  int xx = bee->bx;

  // in case bx is past eol
  x = MIN(MAX(0,(int)strlen(bee->buf.p[y])-1), x);
  xx = MIN(MAX(0,(int)strlen(bee->buf.p[yy])-1), xx);

  text_destroy(bee->clipboard);
  bee->clipboard = text_create();
  int len = bee->clipboard->len = yy - y + 1;
  bee->clipboard->p = malloc(len * sizeof(char*));

  if(len == 1)
    bee->clipboard->p[0] = str_copy_range(bee->buf.p[y], x, xx + 1);
  else
    for(int i=0; i<len; i++) {
      if(i==0){
        bee->clipboard->p[i] = str_copy_from(bee->buf.p[y+i], x);
      }
      else if(i==len-1){
        bee->clipboard->p[i] = str_copy_n(bee->buf.p[y+i], xx);
      }
      else
        bee->clipboard->p[i] = str_copy(bee->buf.p[y+i]);

    }
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
