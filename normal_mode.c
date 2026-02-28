#include "normal_mode.h"

#include "termbox2.h"

#include "string.h"

#include "insert_mode.h"
#include "command_mode.h"
#include "visual_mode.h"

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
    bee_move_cursor_left(bee);
  if(bee->y == bee->buf.len){
    bee_move_cursor_up(bee, 1);
  }
}

static inline void bee_append(struct bee *bee){
  bee_move_cursor_right(bee);
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
    bee_append(bee); break;
  case 'h':
    bee_move_cursor_left(bee); break;
  case 'j':
    bee_move_cursor_down(bee, 1); break;
  case 'k':
    bee_move_cursor_up(bee, 1); break;
  case 'l':
    bee_move_cursor_right(bee); break;
  case 'x':
    n_x(bee); break;
  case 'u':
    bee_undo(bee); break;
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
    bee_move_cursor_left(bee);
    break;
  }
  else if(ev.key!=0) switch(ev.key){
  case TB_KEY_CTRL_R:
    bee_redo(bee); break;
  case TB_KEY_CTRL_D:
    bee_move_cursor_down(bee, (SCREEN_HEIGHT-1)/2); break;
  case TB_KEY_CTRL_U:
    bee_move_cursor_up(bee, (SCREEN_HEIGHT-1)/2); break;
  }
}
