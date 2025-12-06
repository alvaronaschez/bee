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
        free(cs->cmd.i.txt.p[i]);
      free(cs->cmd.i.txt.p);
    }
    free(cs);
    cs = aux;
  }
}

static inline void n_h(struct bee *bee){
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

static inline void n_l_pastend(struct bee *bee){
  // you can go past the end of the line so you can append at the end of the line or join lines
  if(bee->bx + bytelen(&bee->buf.p[YY][XX]) <= (int)strlen(bee->buf.p[YY])){
    bee->vx += columnlen(&bee->buf.p[YY][XX], bee->vx);
    bee->bx += bytelen(&bee->buf.p[YY][XX]);
  }
  bee->vxgoal = bee->vx;
}

static inline void n_j(struct bee *bee){
  if(bee->y +1 == bee->buf.len) return;
  bee->y++;

  // adjust column position
  vx_to_bx(bee->buf.p[YY], bee->vxgoal, &bee->bx, &bee->vx);
}
static inline void n_k(struct bee *bee){
  if(bee->y == 0) return;
  bee->y--;

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
  int blen =  bytelen(&bee->buf.p[YY][XX]);
  change->cmd.i = text_delete(&bee->buf,
      (struct delete_cmd){.x=bee->bx, .y=bee->y, .xx=bee->bx+blen-1, .yy=bee->y});
  bee->undo_stack = change;
  bee->undo_stack->next = old_undo_stack;

  if(bee->bx != 0 && bee->bx == (int)strlen(bee->buf.p[bee->y]))
    n_h(bee);
  if(bee->y == bee->buf.len){
    n_k(bee);
  }
}

static inline void n_i(struct bee *bee){
  bee->ins_buf.p = malloc(1*sizeof(char*));
  bee->ins_buf.len = 1;
  bee->ins_buf.p[0] = calloc(1,1);

  bee->mode = INSERT;
}

static inline void n_a(struct bee *bee){
  n_l_pastend(bee);
  n_i(bee);
}

static inline void n_colon(struct bee *bee){
  bee->cmd_buf = malloc(2*sizeof(char));
  bee->cmd_buf[0] = ':';
  bee->cmd_buf[1] = '\0';
  bee->mode = COMMAND;
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

  // TODO // vx?
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

  // TODO // vx?
  bee->y = new_y;
  bee->bx = new_bx;
  bee->vx = new_bx; // TODO

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
      ;
      bee->ins_buf.p = realloc(bee->ins_buf.p, (++bee->ins_buf.len)*sizeof(char*));
      bee->ins_buf.p[bee->ins_buf.len-1] = calloc(1,1);
      break;
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

static inline void c_esc(struct bee *bee){
  free(bee->cmd_buf);
  bee->mode = NORMAL;
}

static inline void c_backspace(struct bee *bee){
  if(strlen(bee->cmd_buf) == 1)
    c_esc(bee);
  else {
    bee->cmd_buf[strlen(bee->cmd_buf)-1] = '\0';
  }
}

static inline void c_enter(struct bee *bee){
  if(!strcmp(bee->cmd_buf, ":q")){
    bee->quit = 1;
  }
  else if(!strcmp(bee->cmd_buf, ":w")){
    save_file(&bee->buf, bee->filename);
  }
  else if(!strcmp(bee->cmd_buf, ":wq")){
    save_file(&bee->buf, bee->filename);
    bee->quit = 1;
  }
  free(bee->cmd_buf);
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
    //string_append(&bee->cmd_buf, s);
    bee->cmd_buf = realloc(bee->cmd_buf, strlen(bee->cmd_buf) + 7);
    strcat(bee->cmd_buf, s);
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
  //bee->leftcol = bee->toprow = 0;
  bee->bx = bee->vx = bee->vxgoal = 0;
  bee->undo_stack = bee->redo_stack = NULL;
}

static inline void bee_destroy(struct bee *bee){
  for(int i=0; i<bee->buf.len; i++)
    free(bee->buf.p[i]);
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

