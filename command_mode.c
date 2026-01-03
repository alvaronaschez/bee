#include "command_mode.h"

#include "termbox2.h"

#include <stdlib.h>
#include <string.h>

#include "file.h"

void c_esc(struct bee *bee){
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

void command_read_key(struct bee *bee){
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

