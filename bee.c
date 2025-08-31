/*
  minimal features:
  h j k l x i
  #gg #G :#
  gh gj gk gl
  f t F T %
  :w :q :q! :#
  / . , ; n N
*/

#define TB_IMPL
#include "termbox2.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum mode {
  NORMAL, INSERT, COMMAND
};

char *mode_label[] = {"N", "I", "C"};

struct string {
  char *data;
  int len, cap;
};

struct bee {
  enum mode mode;
  struct string *buf;
  int buf_len;
  char *filename;
  int y, yoff;
  int x, bx, vx;
  int xoff;
};

#define footerheight 1
#define screen_height (tb_height() - footerheight)
#define screen_width (tb_width())
#define tablen 8
//#define fg_color TB_WHITE
//#define fg_color TB_CYAN
#define fg_color TB_MAGENTA
#define bg_color TB_BLACK
const char *footer_format = "<%s> \"%s\"  [=%d] L%d C%d";

static inline int utf8_char_len(const char* s){
  if((s[0]&0x80) == 0x00) return 1; // 0xxx_xxxx
  if((s[0]&0xE0) == 0xC0) return 2; // 110x_xxxx 10xx_xxxx
  if((s[0]&0xF0) == 0xE0) return 3; // 1110_xxxx 10_xxxx 10xx_xxxx
  if((s[0]&0xF8) == 0xF0) return 4; // 1111_0xxx 10xx_xxxx  10xx_xxxx 10xx_xxxx
  return 0;
}

static inline	void load_file(struct bee *bee, const char *filename){
  assert(bee->filename == NULL);
  bee->filename = calloc(strlen(filename)+1, sizeof(char));
  strcpy(bee->filename, filename);
  
  FILE *fp = fopen(filename, "r");
  assert(fp);
  int res = fseek(fp, 0, SEEK_END);
  assert(res == 0);
  long fsize = ftell(fp);
  assert(fsize >= 0);
  rewind(fp);
  char *fcontent = (char*) malloc(fsize * sizeof(char));
  fread(fcontent, 1, fsize, fp);
  res = fclose(fp);
  assert(res == 0);
  // count lines in file
  int nlines = 0;
  for(int i = 0; i<fsize; i++)
    if(fcontent[i] == '\n') nlines++;
  if(fcontent[fsize-1] != '\n')
    nlines++;
  bee->buf_len = nlines;
  // copy all lines from fcontent into buf
  bee->buf = malloc(nlines * sizeof(struct string));
  int linelen;
  for(int i = 0, j = 0; i<nlines && j<fsize; i++){
    // count line length
    for(linelen = 0; j+linelen<fsize-1 && fcontent[j+linelen]!='\n'; linelen++);
    // copy line in buffer
    bee->buf[i].data = calloc(linelen, sizeof(char));
    if(linelen>0)
      memcpy(bee->buf[i].data, &fcontent[j], linelen);
    bee->buf[i].len = bee->buf[i].cap = linelen;
    j += linelen+1;
  }
  free(fcontent);
}

static inline void print_screen(const struct bee *bee){
  tb_clear();
  // print file
  for(int j=bee->yoff; j< bee->yoff+screen_height && j<bee->buf_len; j++){
    for(int vi=0, bi=0; bi<bee->buf[j].len && vi<bee->xoff+screen_width;){
      char c = *(bee->buf[j].data+bi+bee->xoff);
      char char_len = utf8_char_len(&c);
      // print char
      if(vi >= bee->xoff){
        if(c=='\t')
          tb_print(vi, j - bee->yoff, fg_color, bg_color, "        ");
        else {
          switch(char_len){
            case 1:
              tb_set_cell(vi, j - bee->yoff, c, fg_color, bg_color);
              break;
            // TODO
            case 2:
              tb_set_cell(vi, j -bee->yoff, '*', fg_color, bg_color);
              break; 
            case 3:
              break;
            case 4:
              break;
          }
        }
      }
      // advance pointers
      vi += c=='\t' ? tablen : 1;
      bi += char_len;
    }
  }
  // print footer
  tb_printf(0, tb_height() - 1, bg_color, fg_color,
            footer_format, mode_label[bee->mode], bee->filename,
            bee->buf_len, bee->yoff + bee->y, bee->xoff + bee->x);
  // print cursor
  tb_set_cursor(bee->vx, bee->y);
  tb_present();
}

static inline void n_h(struct bee *bee){
  if(bee->x > 0)
    bee->x--;
  else if(bee->xoff > 0)
    bee->xoff--;
}
static inline void n_j(struct bee *bee){
  if(bee->y+1<screen_height)
    bee->y++;
  else if(bee->yoff + bee->y +1 < bee->buf_len)
    bee->yoff++;
}
static inline void n_k(struct bee *bee){
  if(bee->y>0)
    bee->y--;
  else {
    if(bee->yoff>0)
      bee->yoff--;
  }
}
static inline void n_l(struct bee *bee){
  if(bee->vx < screen_width && bee->bx+1<bee->buf[bee->yoff+bee->y].len)
    bee->x++;
}
static inline void n_x(struct bee *bee){
}

static inline char normal_read_key(struct bee *bee){
  struct tb_event ev;
  tb_poll_event(&ev);
  if(ev.ch == 'q')
    return 0;
  switch(ev.ch){
  case 'h':
    n_h(bee); break;
  case 'j':
    n_j(bee); break;
  case 'k':
    n_k(bee); break;
  case 'l':
    n_l(bee); break;
  case 'x':
    n_x(bee); break;
  }
  return 1;
}

static inline char read_key(struct bee *bee){
  switch(bee->mode){
  case NORMAL:
    return normal_read_key(bee);
  default:
    return 0;
  }
}

static inline void bee_init(struct bee *bee){
  bee->mode = NORMAL;
  bee->filename = NULL;
  bee->buf = NULL;
  bee->buf_len = 0;
  bee->x = bee->y = 0;
  bee->xoff = bee->yoff = 0;
  bee->bx = bee->vx = 0;
}

int main(int argc, char **argv){
  if(argc < 2){
    printf("missing file name\naborting\n");
    return 1;
  }

  struct bee bee;
  bee_init(&bee);

  load_file(&bee, argv[1]);

  tb_init();

  do{
    print_screen(&bee);
  }while(read_key(&bee));

  tb_shutdown();
  return 0;
}
