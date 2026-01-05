#include "visual_mode.h"

#include "termbox2.h"

#include "normal_mode.h"

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
  }
  else if(ev.key!=0) switch(ev.key){
  case TB_KEY_ESC:
    exit_visual_mode(bee); break;
  }
}
