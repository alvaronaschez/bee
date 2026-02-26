#include "normal_mode.h"

#include "termbox2.h"

#include "util.h"
#include "string.h"

#include "insert_mode.h"
#include "command_mode.h"
#include "visual_mode.h"

#define n_h bee_move_cursor_left
void bee_move_cursor_left(struct bee *bee){
  if(bee->bx > 0){
    //vx_to_bx(bee->buf.p[YY].p, bee->vx-1, &bee->bx, &bee->vx);
    // that would be enough, the following is an optimization
    if ( bee->buf.p[YY][XX-1] == '\t' ) {
      vx_to_bx(bee->buf.p[YY], bee->vx-1, &bee->bx, &bee->vx);
    } else {
      bee->bx = utf8prev(bee->buf.p[YY], bee->bx);
      bee->vx -= columnlen(&bee->buf.p[YY][XX], bee->vx);
    }
    //autoscroll_x(bee);
  }
  bee->vxgoal = bee->vx;
}

#define n_l_pastend bee_move_cursor_right 
void bee_move_cursor_right(struct bee *bee){
  // you can go past the end of the line so you can append at the end of the line or join lines
  if(bee->bx + bytelen(&bee->buf.p[YY][XX]) <= (int)strlen(bee->buf.p[YY])){
    bee->vx += columnlen(&bee->buf.p[YY][XX], bee->vx);
    bee->bx += bytelen(&bee->buf.p[YY][XX]);
  }
  bee->vxgoal = bee->vx;
}

#define n_j bee_move_cursor_down
void bee_move_cursor_down(struct bee *bee, int n){
  bee->y = MIN(bee->buf.len -1, bee->y + n);

  // adjust column position
  vx_to_bx(bee->buf.p[YY], bee->vxgoal, &bee->bx, &bee->vx);
}

#define n_k bee_move_cursor_up
void bee_move_cursor_up(struct bee *bee, int n){
  bee->y = MAX(0, bee->y - n);

  // adjust column position
  vx_to_bx(bee->buf.p[YY], bee->vxgoal, &bee->bx, &bee->vx);
}

static inline void n_x(struct bee *bee){
  change_stack_destroy(bee->redo_stack);
  bee->redo_stack = NULL;
  struct change_stack *old_undo_stack = bee->undo_stack;

  struct change_stack *change = malloc(sizeof(struct change_stack));
  *change = (struct change_stack){
    .y = bee->y, .bx = bee->bx, .vx = bee->vx,
    .op = INS,
  };
  int blen =  bytelen(&bee->buf.p[bee->y][bee->bx]);
  change->cmd.i = text_delete(&bee->buf,
      (struct delete_cmd){.x=bee->bx, .y=bee->y, .xx=bee->bx+blen-1, .yy=bee->y});
  bee->undo_stack = change;
  bee->undo_stack->next = old_undo_stack;

  if(bee->bx != 0 && bee->bx == (int)strlen(bee->buf.p[bee->y]))
    n_h(bee);
  if(bee->y == bee->buf.len){
    n_k(bee, 1);
  }
}

#define n_a bee_append
static inline void bee_append(struct bee *bee){
  n_l_pastend(bee);
  to_insert_mode(bee);
}

static inline void bee_restore_cursor(struct bee *bee, const struct change_stack *ch){
  bee->y = ch->y; bee->bx = ch->bx; bee->vx = ch->vx;
}

/*
 * @brief returns the change needed to revert the applied change
 */
static inline struct change_stack *apply_change(struct bee *bee, struct change_stack *ch){
  struct change_stack *retval = malloc(sizeof(struct change_stack));
  *retval = (struct change_stack){
    .y = bee->y, .bx = bee->bx, .vx = bee->vx,
    //.leftcol=bee->leftcol, .toprow=bee->toprow,
  };
 
  switch(ch->op){
    case INS:
      retval->op = DEL;
      retval->cmd.d = text_insert(&bee->buf, ch->cmd.i);
      break;
    case DEL:
      retval->op = INS;
      retval->cmd.i = text_delete(&bee->buf, ch->cmd.d);
      break;
  }
  bee_restore_cursor(bee, ch);
  return retval;
}

#define n_u(bee) bee_undo(bee)
static inline void bee_undo(struct bee *bee){
  if(bee->undo_stack == NULL)
    return;
  struct change_stack *c = bee->undo_stack;
  struct change_stack *cc = apply_change(bee, c);

  bee->undo_stack = bee->undo_stack->next;
  // we should not free c->cmd.i.txt as it has been owned after `apply_change`
  //c->next = NULL;
  //change_stack_destroy(c);
  free(c);

  //cc->next = bee->redo_stack ? bee->redo_stack->next : NULL;
  cc->next = bee->redo_stack;
  bee->redo_stack = cc;
}

#define n_Cr(bee) bee_redo(bee)
static inline void bee_redo(struct bee *bee){
  if(bee->redo_stack == NULL)
    return;
  struct change_stack *c = bee->redo_stack;
  struct change_stack *cc = apply_change(bee, c);

  bee->redo_stack = bee->redo_stack->next;
  // TODO: review
  // we should not free c->cmd.i.txt as it has been owned after applied
  //c->next = NULL;
  //change_stack_destroy(c);
  free(c);

  //cc->next = bee->undo_stack ? bee->undo_stack->next : NULL;
  cc->next = bee->undo_stack;
  bee->undo_stack = cc;
}

void normal_read_key(struct bee *bee){
  struct tb_event ev;
  tb_poll_event(&ev);
  if(ev.type == TB_EVENT_RESIZE) return;
  else if(ev.ch!=0) switch(ev.ch){
  case 'Z':
    tb_poll_event(&ev);
    if(ev.ch == 'Q') 
      bee->quit = 1;
    break;
  case 'i':
    to_insert_mode(bee); break;
  case 'v':
    to_visual_mode(bee); break;
  case 'a':
    n_a(bee); break;
  case 'h':
    n_h(bee); break;
  case 'j':
    n_j(bee, 1); break;
  case 'k':
    n_k(bee, 1); break;
  case 'l':
    n_l_pastend(bee); break;
  case 'x':
    n_x(bee); break;
  case 'u':
    n_u(bee); break;
  case ':':
    to_command_mode(bee); break;
  case '0':
    bee->bx = bee->vx = bee->vxgoal = 0;
    break;
  case '$':
    while(bee->buf.p[bee->y][bee->bx] != '\0'){
      bee->vx += columnlen(bee->buf.p[bee->y]+bee->bx, bee->vx);
      bee->bx += bytelen(bee->buf.p[bee->y]+bee->bx);
    }
    n_h(bee);
    break;
  }
  else if(ev.key!=0) switch(ev.key){
  case TB_KEY_CTRL_R:
    n_Cr(bee); break;
  case TB_KEY_CTRL_D:
    n_j(bee, (SCREEN_HEIGHT-1)/2); break;
  case TB_KEY_CTRL_U:
    n_k(bee, (SCREEN_HEIGHT-1)/2); break;
  }
}
