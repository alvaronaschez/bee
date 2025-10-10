#include "bee.h"

#define TB_IMPL
#include "termbox2.h"

#include "text_util.h"
#include "text.h"
#include "file.h"
#include "print.h"

#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <libgen.h>

const char *mode_label[3] = {"N", "I", "C"};

static inline void change_stack_destroy(struct change_stack *cs){
  struct change_stack *aux;
  while(cs){
    aux = cs->next;
    if(cs->op == INS){
      for(int i=0; i<cs->cmd.i.txt.len; i++)
	free(cs->cmd.i.txt.p[i].p);
      free(cs->cmd.i.txt.p);
    }
    free(cs);
    cs = aux;
  }
}

static inline void autoscroll_x(struct bee *bee){
  // cursor too far to the right
  if(bee->vx + columnlen(&bee->buf.p[YY].p[XX], bee->vx) > SCREEN_WIDTH + bee->leftcol - MARGIN_LEN){
    int bx_leftcol, vx_leftcol;
    vx_to_bx(bee->buf.p[bee->y].p, bee->leftcol, &bx_leftcol, &vx_leftcol);
    while(bee->vx + columnlen(&bee->buf.p[YY].p[XX], bee->vx) > SCREEN_WIDTH + bee->leftcol - MARGIN_LEN){
      bee->leftcol += columnlen(bee->buf.p[bee->y].p + bx_leftcol, bee->leftcol);
      bx_leftcol += bytelen(bee->buf.p[bee->y].p + bx_leftcol);
    }
  }
  // cursor too far to the left
  if(bee->vx < bee->leftcol)
    bee->leftcol = bee->vx;
}

static inline void autoscroll_y(struct bee *bee){
  // cursor too far up
  if(bee->toprow > bee->y)
    bee->toprow = bee->y;
  // cursor too far down
  if(bee->y - bee->toprow >= SCREEN_HEIGHT)
    bee->toprow = bee->y-SCREEN_HEIGHT+1;
}

static inline void autoscroll(struct bee *bee){
  autoscroll_y(bee);
  autoscroll_x(bee);
}

static inline void n_h(struct bee *bee){
  if(bee->bx > 0){
    //vx_to_bx(bee->buf.p[YY].p, bee->vx-1, &bee->bx, &bee->vx);
    // that would be enough, the following is an optimization
    if ( *(&bee->buf.p[YY].p[XX]-1) == '\t' ) {
      vx_to_bx(bee->buf.p[YY].p, bee->vx-1, &bee->bx, &bee->vx);
    } else {
      bee->bx = utf8prev(bee->buf.p[YY].p, bee->bx);
      bee->vx -= columnlen(&bee->buf.p[YY].p[XX], bee->vx);
    }
    autoscroll_x(bee);
  }
  bee->vxgoal = bee->vx;
}
static inline void n_l(struct bee *bee){
  if(bee->bx + bytelen(&bee->buf.p[YY].p[XX]) < bee->buf.p[YY].len){
  // allow go till the linebreak character
  //if(bee->bx + bytelen(&bee->buf.p[YY].p[XX]) <= bee->buf.p[YY].len){
    bee->vx += columnlen(&bee->buf.p[YY].p[XX], bee->vx);
    bee->bx += bytelen(&bee->buf.p[YY].p[XX]);
    autoscroll_x(bee);
  }
  bee->vxgoal = bee->vx;
}

static inline void n_l_pastend(struct bee *bee){
  // you can go past the end of the line so you can append at the end of the line or join lines
  if(bee->bx + bytelen(&bee->buf.p[YY].p[XX]) <= bee->buf.p[YY].len){
    bee->vx += columnlen(&bee->buf.p[YY].p[XX], bee->vx);
    bee->bx += bytelen(&bee->buf.p[YY].p[XX]);
    autoscroll_x(bee);
  }
  bee->vxgoal = bee->vx;
}

static inline void n_j(struct bee *bee){
  if(bee->y +1 == bee->buf.len) return;
  bee->y++;

  // adjust column position
  vx_to_bx(bee->buf.p[YY].p, bee->vxgoal, &bee->bx, &bee->vx);

  autoscroll(bee);
}
static inline void n_k(struct bee *bee){
  if(bee->y == 0) return;
  bee->y--;

  // adjust column position
  vx_to_bx(bee->buf.p[YY].p, bee->vxgoal, &bee->bx, &bee->vx);

  autoscroll(bee);
}

static inline void n_x(struct bee *bee){
  change_stack_destroy(bee->redo_stack);
  bee->redo_stack = NULL;
  struct change_stack *old_undo_stack = bee->undo_stack;

  struct change_stack *change = malloc(sizeof(struct change_stack));
  *change = (struct change_stack){
    .y = bee->y, .bx = bee->bx, .vx = bee->vx,
    .leftcol=bee->leftcol, .toprow=bee->toprow,
    .op = INS,
  };
  int blen =  bytelen(&bee->buf.p[YY].p[XX]);
  change->cmd.i = text_delete(&bee->buf,
      (struct delete_cmd){.x=bee->bx, .y=bee->y, .xx=bee->bx+blen-1, .yy=bee->y});
  bee->undo_stack = change;
  bee->undo_stack->next = old_undo_stack;

  if(bee->bx != 0 && bee->bx == bee->buf.p[bee->y].len)
    n_h(bee);
  if(bee->y == bee->buf.len){
    n_k(bee);
  }
}

static inline void n_i(struct bee *bee){
  string_init(&bee->ins_buf);
  bee->ins_y = bee->y; bee->ins_bx = bee->bx; bee->ins_vx = bee->vx;
  bee->ins_toprow = bee->toprow; bee->ins_leftcol = bee->leftcol;
  bee->mode = INSERT;
}

static inline void n_a(struct bee *bee){
  n_l_pastend(bee);
  n_i(bee);
}

static inline void n_colon(struct bee *bee){
  string_init(&bee->cmd_buf);
  string_append(&bee->cmd_buf, ":");
  bee->mode = COMMAND;
}

static inline void bee_save_cursor(const struct bee*bee, struct change_stack *ch){
  ch->y = bee->y; ch->bx = bee->bx; ch->vx = bee->vx;
  ch->toprow = bee->toprow; ch->leftcol = bee->leftcol;
}
static inline void bee_restore_cursor(struct bee *bee, const struct change_stack *ch){
  bee->y = ch->y; bee->bx = ch->bx; bee->vx = ch->vx;
  bee->toprow = ch->toprow; bee->leftcol = ch->leftcol;
  autoscroll(bee);
}

/*
 * @brief returns the change needed to revert the applied change
 */
static inline struct change_stack *apply_change(struct bee *bee, struct change_stack *ch){
  struct change_stack *retval = malloc(sizeof(struct change_stack));
  *retval = (struct change_stack){
    .y = bee->y, .bx = bee->bx, .vx = bee->vx,
    .leftcol=bee->leftcol, .toprow=bee->toprow,
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

static inline void normal_read_key(struct bee *bee){
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
    n_i(bee); break;
  case 'a':
    n_a(bee); break;
  case 'h':
    n_h(bee); break;
  case 'j':
    n_j(bee); break;
  case 'k':
    n_k(bee); break;
  case 'l':
    n_l_pastend(bee); break;
  case 'x':
    n_x(bee); break;
  case 'u':
    n_u(bee); break;
  case ':':
    n_colon(bee); break;
  }
  else if(ev.key!=0) switch(ev.key){
  case TB_KEY_CTRL_R:
    n_Cr(bee); break;
  }
}

static inline void i_esc(struct bee *bee){
  if(bee->ins_buf.len == 0){
    bee->mode = NORMAL;
    return;
  }
  change_stack_destroy(bee->redo_stack);
  bee->redo_stack = NULL;
  struct change_stack *old_undo_stack = bee->undo_stack;

  int num_lines_inserted = bee->y - bee->ins_y;
  int num_lines_ins_buf = num_lines_inserted +1;
  struct text inserted_lines = text_from_string(&bee->ins_buf, num_lines_ins_buf);
  bee->ins_buf.len = bee->ins_buf.cap = 0; bee->ins_buf.p = NULL;

  struct change_stack *change = malloc(sizeof(struct change_stack));
  *change = (struct change_stack){
    .y = bee->ins_y, .bx = bee->ins_bx, .vx = bee->ins_vx,
    .leftcol=bee->leftcol, .toprow=bee->toprow,
    .op = DEL,
  };
  change->cmd.d = text_insert(&bee->buf, 
      (struct insert_cmd){.x=bee->ins_bx, .y=bee->ins_y, .txt=inserted_lines});
  bee->undo_stack = change;
  bee->undo_stack->next = old_undo_stack;

  bee->vxgoal = bee->vx;
  bee->mode = NORMAL;
}

static inline void i_backspace(struct bee *bee){
  if(bee->ins_buf.len == 0 || bee->bx == 0)
    return;

  bee->bx -= bytelen(&bee->ins_buf.p[bee->ins_buf.len-1]);
  bee->vx -= columnlen(&bee->ins_buf.p[bee->ins_buf.len-1], bee->ins_vx);
  bee->ins_buf.p[bee->ins_buf.len-1] = '\0';
  bee->ins_buf.len--;
}

static inline void insert_read_key(struct bee *bee){
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
      bee->y++;
      bee->bx = bee->vx = 0;
      string_append(&bee->ins_buf, "\n");
      break;
  }
  else if(ev.ch){
    char s[7];
    tb_utf8_unicode_to_char(s, ev.ch);
    string_append(&bee->ins_buf, s);
    bee->bx += strlen(s);
    bee->vx += columnlen(s, bee->vx);
  }
}

static inline void c_esc(struct bee *bee){
  free(bee->cmd_buf.p);
  bee->cmd_buf.len = bee->cmd_buf.cap = 0;
  bee->mode = NORMAL;
}

static inline void c_backspace(struct bee *bee){
  if(bee->cmd_buf.len == 1)
    c_esc(bee);
  else {
    bee->cmd_buf.p[bee->cmd_buf.len-1] = '\0';
    bee->cmd_buf.len--;
  }
}

static inline void c_enter(struct bee *bee){
  if(!strcmp(bee->cmd_buf.p, ":q")){
    bee->quit = 1;
  }
  else if(!strcmp(bee->cmd_buf.p, ":w")){
    save_file(&bee->buf, bee->filename);
  }
  else if(!strcmp(bee->cmd_buf.p, ":wq")){
    save_file(&bee->buf, bee->filename);
    bee->quit = 1;
  }
  bee->mode = NORMAL;
}

static inline void command_read_key(struct bee *bee){
  struct tb_event ev;
  tb_poll_event(&ev);
  if(ev.type == TB_EVENT_RESIZE) return;
  else if(ev.key!=0) switch(ev.key){
  case TB_KEY_ESC:
    c_esc(bee); break;
  case TB_KEY_BACKSPACE:
  case TB_KEY_BACKSPACE2:
    c_backspace(bee); break;
  case TB_KEY_ENTER:
    c_enter(bee); break;
  }
  else if(ev.ch){
    char s[7];
    tb_utf8_unicode_to_char(s, ev.ch);
    string_append(&bee->cmd_buf, s);
  }
}

static inline void read_key(struct bee *bee){
  switch(bee->mode){
  case NORMAL:
    normal_read_key(bee); break;
  case INSERT:
    insert_read_key(bee); break;
  case COMMAND:
    command_read_key(bee); break;
  default:
    break;
  }
}

static inline void bee_init(struct bee *bee){
  bee->quit = 0;
  bee->mode = NORMAL;
  bee->filename = NULL;
  bee->buf.len = 0;
  bee->y = 0;
  bee->leftcol = bee->toprow = 0;
  bee->bx = bee->vx = bee->vxgoal = 0;
  bee->ins_buf.p = NULL;
  bee->undo_stack = bee->redo_stack = NULL;
}

static inline void bee_destroy(struct bee *bee){
  for(int i=0; i<bee->buf.len; i++)
    string_deinit(&bee->buf.p[i]);
  free(bee->buf.p);
  if(bee->filename)
    free(bee->filename);
  change_stack_destroy(bee->undo_stack);
  change_stack_destroy(bee->redo_stack);
}

int bee(const char* filename){
  setlocale(LC_CTYPE, LOCALE);

  struct bee bee;
  bee_init(&bee);

  bee.filename = realpath(filename, NULL);
  if(bee.filename == NULL) {
    printf("wrong file name\naborting\n");
    return 1;
  }

  bee.buf.p = load_file(bee.filename, &bee.buf.len);

  tb_init();
  tb_set_clear_attrs(FG_COLOR, BG_COLOR);
  //tb_set_input_mode(TB_INPUT_ESC); // default
  //tb_set_input_mode(TB_INPUT_ALT);

  while(!bee.quit){
    print_screen(&bee);
    read_key(&bee);
  }

  tb_shutdown();
  bee_destroy(&bee);

  return 0;
}

