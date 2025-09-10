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

//#define DEBUG
#define LOCALE "en_US.UTF-8"
#define fg_color TB_YELLOW
#define bg_color TB_BLACK
#define tablen 8
#define footerheight 1
const char *footer_format = "<%s>  \"%s\"  [=%d]  C%d L%d";
const char *debug_footer_format = "<%s> \"%s\" [=%d] C%d L%d";
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

  int toprow, leftcol;
  int y, bx, vx, vxgoal;

  struct string ins_buf;
};

void string_init(struct string *s){
  s->cap = 64;
  s->len = 0;
  s->chars = calloc(s->cap+1, sizeof(char));
}

void string_destroy(struct string *s){
  free(s->chars);
  s->chars = NULL;
  s->cap = 0;
  s->len = 0;
}

void string_append(struct string *s, const char *t){
  if(s->len + (int)strlen(t) > s->cap){
    char *aux = s->chars;
    s->chars = malloc(s->cap*2);
    strcpy(s->chars, aux);
    s->cap *= 2;
  }
  strcat(s->chars + s->len, t);
  s->len += strlen(t);
}

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

static inline int columnlen(const char* s, int col_off){
  if(*s=='\t')
    return tablen-(col_off%tablen);
  wchar_t wc;
  mbtowc(&wc, s, MB_CUR_MAX);
  int width = wcwidth(wc);
  assert(width>=0); // -1 -> invalid utf8 char
  return 1;
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
  if(*s == '\0') return 0;
  off--;
  while((s[off]&0xC0) == 0x80)
    off--;
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

    j += linelen+1;
  }
  free(fcontent);
  // calle should free buf
  return buf;
}

static inline void print_tb(int x, int y, char* c){
  if(bytelen(c)==1){
      if(*c=='\t')
	tb_print(x, y, fg_color, bg_color, "        ");
      else
	tb_set_cell(x, y, *c, fg_color, bg_color);
  } else {
      wchar_t wc;
      mbstowcs(&wc, c, 1);
      //tb_set_cell(x, y, '*', fg_color, bg_color);
      tb_set_cell(x, y, wc, fg_color, bg_color);
      //tb_set_cell_ex(x, y, c, 2, fg_color, bg_color);
  }
}

static inline void print_row(const struct bee *bee, int j){
  int vi=0, bi=0;
  while(bee->buf[bee->toprow+j].chars[bi] && vi < bee->leftcol+screen_width){
    char *c = bee->buf[bee->toprow+j].chars + bi;
    if(vi >= bee->leftcol){
      print_tb(vi - bee->leftcol, j, c);
    } else if(vi + columnlen(c, vi) > bee->leftcol){ // vi < bee->leftcol
      for(int i=0; i< vi + columnlen(c, vi) - bee->leftcol; i++){
        tb_print(bee->leftcol + i, j, bg_color, fg_color, " ");
      }
    }
    bi += bytelen(c);
    vi += columnlen(c, vi);
  }
}

static inline void print_screen(const struct bee *bee){
  tb_clear();
  // print file
  for(int j=0; j < screen_height && j+bee->toprow < bee->buf_len; j++){
    //while(bi < bee->buf[bee->toprow+j].len && vi < bee->leftcol+screen_width){
    print_row(bee, j);
  }
  // print footer
#ifndef DEBUG 
  tb_printf(0, tb_height() - 1, bg_color, fg_color, footer_format,
            mode_label[bee->mode], bee->filename, bee->buf_len, bee->y, bee->vx);
#else
  tb_printf(0, tb_height() - 1, bg_color, fg_color, debug_footer_format,
            mode_label[bee->mode], bee->filename, bee->buf_len, bee->y, bee->vx);
#endif
  // print cursor
  tb_set_cursor(bee->vx - bee->leftcol, bee->y - bee->toprow);
  tb_present();
}

 static inline void vx_to_bx(const char *str, int vxgoal, int *bx, int *vx){
  *bx = *vx = 0;
  int bxold;
  if(*str == '\0') return;
  while(1){
    if(str[*bx+bytelen(str+*bx)] == '\0') return;
    if(*vx == vxgoal) return; 
    if(*vx+columnlen(str+*bx, *vx) > vxgoal) return;
    bxold = *bx;
    *bx += bytelen(str+*bx);
    *vx += columnlen(str+bxold, *vx);
  }
}

//static inline void bx_to_vx(const char *str, int vxgoal, int *bx, int *vx){}

static inline void autoscroll_x(struct bee* bee){
  // cursor too far to the right
  if(bee->vx + columnlen(current_char_ptr(bee), bee->vx) > screen_width + bee->leftcol){
    // TODO
    int bx_leftcol, vx_leftcol;
    vx_to_bx(bee->buf[bee->y].chars, bee->leftcol, &bx_leftcol, &vx_leftcol);
    while(bee->vx + columnlen(current_char_ptr(bee), bee->vx) > screen_width + bee->leftcol){
      bee->leftcol += columnlen(bee->buf[bee->y].chars + bx_leftcol, bee->leftcol);
      bx_leftcol += bytelen(bee->buf[bee->y].chars + bx_leftcol);
    }
  }
  // cursor too far to the left
  if(bee->vx < bee->leftcol){
    bee->leftcol = bee->vx;
  }
}

static inline void n_h(struct bee *bee){
  if(bee->bx > 0){
    //vx_to_bx(current_line_ptr(bee)->chars, bee->vx-1, &bee->bx, &bee->vx);
    // that would be enough, the following is an optimization
    if ( *(current_char_ptr(bee)-1) == '\t' ) {
      vx_to_bx(current_line_ptr(bee)->chars, bee->vx-1, &bee->bx, &bee->vx);
    } else {
      bee->bx = utf8prev(current_line_ptr(bee)->chars, bee->bx);
      bee->vx -= columnlen(current_char_ptr(bee), bee->vx);
    }
    autoscroll_x(bee);
  }
  bee->vxgoal = bee->vx;
}
static inline void n_l(struct bee *bee){
  if(bee->bx + bytelen(current_char_ptr(bee)) < current_line_ptr(bee)->len){
    bee->vx += columnlen(current_char_ptr(bee), bee->vx);
    bee->bx += bytelen(current_char_ptr(bee));
    autoscroll_x(bee);
  }
  bee->vxgoal = bee->vx;
}

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
  int len = bee->buf[bee->y].len;
  if(len==0) return;
  char *s = bee->buf[bee->y].chars;
  memmove(
    //dest
    s + bee->bx,
    // src
    s + bee->bx + bytelen(s+bee->bx),
    // n
    len - (bee->bx + bytelen(s+bee->bx))
    );
  bee->buf[bee->y].len -= bytelen(s+bee->bx);
  current_line_ptr(bee)->chars[current_line_ptr(bee)->len] = '\0';
  // if bx at the end of the line bx--
  if(bee->bx == bee->buf[bee->y].len){
    //bee->bx = utf8prev(s, bee->bx);
    vx_to_bx(current_line_ptr(bee)->chars, bee->vx==0? 0:bee->vx-1, &bee->bx, &bee->vx);
  }
}

static inline void n_i(struct bee *bee){
  bee->mode = INSERT;
}

static inline void resize(const struct bee *bee){
}

static inline char normal_read_key(struct bee *bee){
  struct tb_event ev;
  tb_poll_event(&ev);
  if(ev.type == TB_EVENT_RESIZE) resize(bee);
  else if(ev.ch!=0) switch(ev.ch){
  case 'Z':
    tb_poll_event(&ev);
    return ev.ch != 'Q';
    //return ev.ch == 'Q' ? 0 : 1;
  case 'i':
    n_i(bee); break;
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
  else if(ev.key!=0) switch(ev.key){
  case TB_KEY_CTRL_Q:
    return 0;
  }
  return 1;
}


static inline void i_esc(struct bee *bee){
  bee->mode = NORMAL;
}

static inline char insert_read_key(struct bee *bee){
  struct tb_event ev;
  tb_poll_event(&ev);
  if(ev.type == TB_EVENT_RESIZE) resize(bee);
  else if(ev.key!=0) switch(ev.key){
  case TB_KEY_ESC:
    i_esc(bee); break;
  }
  return 1;
}

static inline void c_esc(struct bee *bee){
  bee->mode = NORMAL;
}

static inline char command_read_key(struct bee *bee){
  struct tb_event ev;
  tb_poll_event(&ev);
  if(ev.type == TB_EVENT_RESIZE) resize(bee);
  else if(ev.key!=0) switch(ev.key){
  case TB_KEY_ESC:
    c_esc(bee); break;
  }
  return 1;
}

static inline char read_key(struct bee *bee){
  switch(bee->mode){
  case NORMAL:
    return normal_read_key(bee);
  case INSERT:
    return insert_read_key(bee);
  case COMMAND:
    return command_read_key(bee);
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

  bee.filename = realpath(argv[1], NULL);
  if(bee.filename == NULL) {
    printf("wrong file name\naborting\n");
    return 1;
  }


  bee.buf = load_file(bee.filename, &bee.buf_len);

  tb_init();
  //tb_set_input_mode(TB_INPUT_ESC); // default
  //tb_set_input_mode(TB_INPUT_ALT);

  do {
    print_screen(&bee);
  } while(read_key(&bee));

  tb_shutdown();

  return 0;
}
