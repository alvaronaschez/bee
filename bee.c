/*
  minimal features:
  h j k l
  d D i I c C a A
  x X
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
#include <wchar.h>
#include <locale.h>

#define LOCALE "en_US.UTF-8"
#define fg_color TB_YELLOW
#define bg_color TB_BLACK
#define tablen 8
#define footerheight 1
const char *footer_format = "<%s> \"%s\"  [=%d] L%d C%d";
#define screen_height (tb_height() - footerheight)
#define screen_width (tb_width())

enum mode {
  NORMAL, INSERT, COMMAND
};

char *mode_label[] = {"N", "I", "C"};

struct string {
  char *chars; // null terminated
  int len, cap; // length and capacity
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

static inline struct string *current_line_ptr(struct bee* bee){
  return &bee->buf[bee->yoff+bee->y];
}
static inline char *current_char_ptr(struct bee* bee){
  return bee->buf[bee->yoff+bee->y].chars + bee->bx;
}

#define bytelen utf8len
static inline int utf8len(const char* s){
  if((s[0]&0x80) == 0x00) return 1; // 0xxx_xxxx
  if((s[0]&0xE0) == 0xC0) return 2; // 110x_xxxx 10xx_xxxx
  if((s[0]&0xF0) == 0xE0) return 3; // 1110_xxxx 10xx_xxxx 10xx_xxxx
  if((s[0]&0xF8) == 0xF0) return 4; // 1111_0xxx 10xx_xxxx  10xx_xxxx 10xx_xxxx
  return 0;
}

static inline int columnlen(const char* s){
  if(*s=='\t')
    return tablen;
  wchar_t wc;
  mbtowc(&wc, s, MB_CUR_MAX);
  int width = wcwidth(wc);
  assert(width>=0); // -1 -> invalid utf8 char
  return 1;
}

int columndist(const char* s, int off1, int off2){
  if(off1 == off2) return 0;
  if(off1 > off2) return columndist(s, off2, off1);
  return columnlen(s+off1) + columndist(s, off1 +utf8len(s+off1), off2);
}

static inline int utf8prevn(const char* s, int off, int n);
static inline int utf8nextn(const char* s, int off, int n){
  if(n<0) return utf8prevn(s, off, -n);
  int len;
  for(; n>0; n--){
    len = utf8len(s+off);
    if(len == -1) return -1;
    if(s[off+len] == '\0') return off;
    off+=len;
  }
  return off;
}
static inline int utf8next(const char* s, int off){
  return utf8nextn(s, off, 1);
}
static inline int utf8prevn(const char* s, int off, int n){
  if(n<0) return utf8nextn(s, off, -n);
  int i;
  for(; n>0; n--){
    for(i=0; i<4 && (s[off]&0xC0)==0x80; off--, i++);
    if(utf8len(s+off) == 0) return -1;
    if(off == 0) return 0;
  }
  return off;
}
static inline int utf8prev(const char* s, int off){
  return utf8prevn(s, off, 1);
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
    bee->buf[i].chars = calloc(linelen+1, sizeof(char));
    if(linelen>0)
      memcpy(bee->buf[i].chars , &fcontent[j], linelen);
    bee->buf[i].len = bee->buf[i].cap = linelen;
    j += linelen+1;
  }
  free(fcontent);
}

static inline void print_screen(const struct bee *bee){
  tb_clear();
  for(int j=bee->yoff; j< bee->yoff+screen_height && j<bee->buf_len; j++){
    int vi=0, bi=0;
    char *c;
    while(vi < bee->xoff){
      c = bee->buf[j].chars + bi + bee->xoff;
      vi += columnlen(c);
      bi += bytelen(c);
    }
    while(bi<bee->buf[j].len && vi<bee->xoff+screen_width){
      c = bee->buf[j].chars + bi + bee->xoff;
      switch(bytelen(c)){
        case 1:
          if(*c=='\t')
            tb_print(vi - bee->xoff, j - bee->yoff, fg_color, bg_color, "        ");
          else
            tb_set_cell(vi - bee->xoff, j - bee->yoff, *c, fg_color, bg_color);
          break;
        // TODO
        case 2:
          tb_set_cell(vi - bee->xoff, j -bee->yoff, '*', fg_color, bg_color);
          break;
        case 3:
          break;
        case 4:
          break;
      }
      vi += columnlen(c);
      bi += bytelen(c);
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
  if(bee->x == 0)
    return;
  // bee->x > 0
  char *c = current_char_ptr(bee);
  bee->x--;
  bee->vx -= columnlen(c);
  bee->bx -= bytelen(c);
  if(bee->vx+columnlen(c) > screen_width){
    // TODO: decrease bee->xoff
  }
}
static inline void n_l(struct bee *bee){
  const char *c = current_char_ptr(bee);
  int bl = bytelen(c);
  if(bee->bx+bl == current_line_ptr(bee)->len)
    return;
  // bee->bx+bl < current_line_ptr(bee)->len
  bee->x++;
  bee->bx += bl;
  bee->vx += columnlen(c);
  c += bl;

  if(bee->vx+columnlen(c) > screen_width){
    int x = bee->vx+columnlen(c) - screen_width;
    bee->xoff += x;
    bee->vx -= x;
  }
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

  setlocale(LC_CTYPE, LOCALE);

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
