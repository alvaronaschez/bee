#include "bee.h"

#define TB_IMPL
#include "termbox2.h"

#include "text.h"
#include "string.h"
#include "file.h"
#include "print.h"
#include "normal_mode.h"
#include "insert_mode.h"
#include "command_mode.h"
#include "visual_mode.h"

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
//#include <assert.h>
//#include <limits.h>
//#include <wchar.h>
//#include <libgen.h>

const char *mode_label[NUM_MODES] = {
  "N", "I", "C", "V", "S"};

void change_stack_destroy(struct change_stack *cs){
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

static inline void read_key(struct bee *bee){
  switch(bee->mode){
  case NORMAL:
    normal_read_key(bee); break;
  case INSERT:
    insert_read_key(bee); break;
  case COMMAND:
    command_read_key(bee); break;
  case VISUAL:
    visual_read_key(bee); break;
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

int bee_run(const char* filename){
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

