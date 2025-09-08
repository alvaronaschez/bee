/*
  minimal features:
  h j k l
  d D i I c C a A
  x X
  zz z<CR>
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

#define DEBUG
#define LOCALE "en_US.UTF-8"
#define fg_color TB_YELLOW
#define bg_color TB_BLACK
#define tablen 8
#define footerheight 1
const char *footer_format = "<%s> \"%s\"  [=%d] L%d C%d";
const char *debug_footer_format = "<%s> \"%s\"  [=%d] L%d C%d bx:%d leftcol:%d vxgoal:%d";
#define screen_height (tb_height() - footerheight)
#define screen_width (tb_width())

enum mode {
  NORMAL, INSERT, COMMAND
};

char *mode_label[] = {"N", "I", "C"};

struct string {
  char *chars; // null terminated
  int len, cap; // length and capacity
  int columnlen; // length in number of columns in the screen
  int codepointlen; // number of unicode codepoints
};

struct bee {
  enum mode mode;
  struct string *buf;
  int buf_len;
  char *filename;

  int toprow, leftcol;
  int y, bx, vx, vxgoal;
};

static inline struct string *current_line_ptr(struct bee* bee){
  return &bee->buf[bee->y];
}
static inline char *current_char_ptr(struct bee* bee){
  return bee->buf[bee->y].chars + bee->bx;
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
  //return utf8prevn(s, off, 1);
  if(*s == '\0') return 0;
  else off--;
  for(;(s[off]&0xC0) == 0x80; off--);
  return off;
}

static inline struct string *load_file(const char *filename, int *len){
  if (filename == NULL) return 0;
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
  *len = nlines;
  // copy all lines from fcontent into buf
  struct string *buf = malloc(nlines * sizeof(struct string));
  int linelen;
  for(int i = 0, j = 0; i<nlines && j<fsize; i++){
    // count line length
    for(linelen = 0; j+linelen<fsize-1 && fcontent[j+linelen]!='\n'; linelen++);
    // copy line in buffer
    buf[i].chars = calloc(linelen+1, sizeof(char));
    if(linelen>0)
      memcpy(buf[i].chars , &fcontent[j], linelen);
    buf[i].len = buf[i].cap = linelen;
    // count numcolumns
    buf[i].columnlen = 0;
    buf[i].codepointlen = 0;
    for(char*c = buf[i].chars; *c!='\0'; c+=bytelen(c)){
      buf[i].columnlen += columnlen(c);
      buf[i].codepointlen ++;
    }
    j += linelen+1;
  }
  free(fcontent);
  // calle should free buf
  return buf;
}

static inline void print_tb(int x, int y, char* c){
  switch(bytelen(c)){
    case 1:
      if(*c=='\t')
	tb_print(x, y, fg_color, bg_color, "        ");
      else
	tb_set_cell(x, y, *c, fg_color, bg_color);
      break;
    // TODO
    case 2:
      ;
      wchar_t wc;
      mbstowcs(&wc, c, 1);
      //tb_set_cell(x, y, '*', fg_color, bg_color);
      tb_set_cell(x, y, wc, fg_color, bg_color);
      //tb_set_cell_ex(x, y, c, 2, fg_color, bg_color);
      break;
    case 3:
      break;
    case 4:
      break;
  }
}

static inline void print_screen(const struct bee *bee){
  tb_clear();
  // print file
  for(int j=0; j < screen_height && j+bee->toprow < bee->buf_len; j++){
    int vi=0, bi=0;
    while(bi < bee->buf[bee->toprow+j].len && vi < bee->leftcol+screen_width){
      char *c = bee->buf[bee->toprow+j].chars + bi;
      if(vi >= bee->leftcol){
        print_tb(vi - bee->leftcol, j, c);
      } else if(vi + columnlen(c) > bee->leftcol){ // vi < bee->leftcol
        for(int i=0; i< vi + columnlen(c) - bee->leftcol; i++){
          tb_print(bee->leftcol + i, j, bg_color, fg_color, " ");
        }
      }
      bi += bytelen(c);
      vi += columnlen(c);
    }
  }
  // print footer
#ifdef DEBUG 
  tb_printf(0, tb_height() - 1, bg_color, fg_color, debug_footer_format,
            mode_label[bee->mode], bee->filename, bee->buf_len, bee->y, bee->vx,
            bee->bx, bee->leftcol, bee->vxgoal);
#else
  tb_printf(0, tb_height() - 1, bg_color, fg_color, footer_format,
            mode_label[bee->mode], bee->filename, bee->buf_len, bee->y, bee->vx);
#endif
  // print cursor
  tb_set_cursor(bee->vx - bee->leftcol, bee->y - bee->toprow);
  tb_present();
}
 
static inline void autoscroll_x(struct bee* bee){
  // cursor too far to the right
  while(bee->vx + columnlen(current_char_ptr(bee)) > screen_width + bee->leftcol){
    // TODO
    bee->leftcol += columnlen(current_line_ptr(bee)->chars+bee->leftcol);
  }
  // cursor too far to the left
  if(bee->vx < bee->leftcol){
    bee->leftcol = bee->vx;
  }
}

static inline void n_h(struct bee *bee){
  if(bee->bx > 0){
    bee->bx = utf8prev(current_line_ptr(bee)->chars, bee->bx);
    bee->vx -= columnlen(current_char_ptr(bee));
    autoscroll_x(bee);
  }
  bee->vxgoal = bee->vx;
}
static inline void n_l(struct bee *bee){
  if(bee->bx + bytelen(current_char_ptr(bee)) < current_line_ptr(bee)->len){
    bee->vx += columnlen(current_char_ptr(bee));
    bee->bx += bytelen(current_char_ptr(bee));
    autoscroll_x(bee);
  }
  bee->vxgoal = bee->vx;
}

static inline void vx_to_bx(const char *str, int vxgoal, int *bx, int *vx){
  *bx = *vx = 0;
  if(*str == '\0') return;
  while(str[*bx + bytelen(str+*bx)]  != '\0' && *vx < vxgoal){
    *bx+=bytelen(str+*bx); *vx+=columnlen(str+*bx);
  }
}
//static inline void bx_to_vx(const char *str, int vxgoal, int *bx, int *vx){}

static inline void n_j(struct bee *bee){
  if(bee->y +1 == bee->buf_len) return;
  
  if(bee->y +1 == bee->toprow+screen_height)
    bee->toprow++;
  bee->y++;

  // adjust column position
  vx_to_bx(current_line_ptr(bee)->chars, bee->vxgoal, &bee->bx, &bee->vx);
  // adjust horizontal-offset/scroll
  autoscroll_x(bee);
}
static inline void n_k(struct bee *bee){
  if(bee->y == 0) return;

  if(bee->toprow == bee->y)
      bee->toprow--;
  bee->y--;

  // adjust column position
  vx_to_bx(current_line_ptr(bee)->chars, bee->vxgoal, &bee->bx, &bee->vx);
  // adjust horizontal-offset/scroll
  autoscroll_x(bee);
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
  bee->y = 0;
  bee->leftcol = bee->toprow = 0;
  bee->bx = bee->vx = bee->vxgoal = 0;
}

int main(int argc, char **argv){
  if(argc < 2){
    printf("missing file name\naborting\n");
    return 1;
  }

  setlocale(LC_CTYPE, LOCALE);

  struct bee bee;
  bee_init(&bee);

  bee.filename = calloc(strlen(argv[1])+1, sizeof(char));
  strcpy(bee.filename, argv[1]);

  bee.buf = load_file(bee.filename, &bee.buf_len);

  tb_init();

  do {
    print_screen(&bee);
  } while(read_key(&bee));

  tb_shutdown();

  return 0;
}
