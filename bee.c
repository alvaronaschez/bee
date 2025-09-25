#define TB_IMPL
#include "termbox2.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>

#define LOCALE "en_US.UTF-8"
#define fg_color TB_BLACK
#define bg_color TB_WHITE
#define tablen 8
#define footerheight 1
#define screen_height (tb_height() - footerheight)
#define screen_width (tb_width())
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

enum mode {
  NORMAL, INSERT, COMMAND
};

char *mode_label[] = {"N", "I", "C"};

struct string {
  char *chars; // null terminated
  int len, cap; // length and capacity
};

struct delete_command{
  int x, y, xx, yy;
};
struct insert_command{
  int x, y, len;
  struct string *txt;
};
enum operation { INS, DEL};
struct change_stack{
  enum operation op;
  int y, bx, vx, leftcol, toprow;
  struct change_stack *next;
  union {
    struct insert_command i;
    struct delete_command d;
  } cmd;
};

struct bee {
  enum mode mode;
  struct string *buf;
  int buf_len;
  char *filename;

  int toprow, leftcol;
  int y, bx, vx, vxgoal;

  struct string ins_buf;
  int ins_y, ins_bx, ins_vx;
  int ins_toprow, ins_leftcol;

  struct change_stack *undo_stack, *redo_stack;
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
  if(s->len + (int)strlen(t) > s->cap)
    s->chars = realloc(s->chars, s->cap*=2);
  strcat(s->chars + s->len, t);
  s->len += strlen(t);
}

/**
 * @brief Splits a string with linesbreak into an array of strings w/o linebreaks
 *
 * @warning Takes ownership of `str`.
 * The caller must not use or free `str` after this call.
 */
struct string * string_split_lines(struct string *str, int nlines){
  char *s = str->chars;
  struct string *inserted_lines = malloc(nlines*sizeof(struct string));

  for(int i=0; i<nlines; i++){
    char *end = strchrnul(s, '\n');
    inserted_lines[i].chars = malloc(end-s+1);
    memcpy(inserted_lines[i].chars, s, end-s);
    inserted_lines[i].chars[end-s] = '\0'; // null terminated string
    inserted_lines[i].cap = inserted_lines[i].len = end-s;
    s = end+1;
  }

  string_destroy(str);

  return inserted_lines;
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
  // we need to know the col_off in order to compute the length of the tab char
  if(*s=='\t')
    return tablen-col_off%tablen;
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

/**
 * @warning Takes ownership of `s`.
 * The caller must not use or free `s` after this call.
 */
static inline struct change_stack *bee_insert(struct bee *bee, int x, int y, struct string *s, int nlines){
  assert(nlines > 0);

/* BEGIN save to undo stack */
  struct change_stack *ch = malloc(sizeof(struct change_stack));
  // TODO: review these two lines and the whole thing of bee->ins_toprow...
  // are we using insert in places where these will break?
  // should we manage this outside this function?
  ch->y = bee->ins_y; ch->bx = bee->ins_bx; ch->vx = bee->ins_vx;
  ch->toprow = bee->ins_toprow; ch->leftcol = bee->ins_leftcol;
  ch->op = DEL;
  ch->cmd.d = (struct delete_command){
    .x = x, .y = y,
    .xx = s[nlines-1].len -1 + (nlines>1 ? 0 : x),
    .yy = y+nlines -1
  };
  //ch->next = bee->undo_stack;
  //bee->undo_stack = ch;
/* END save to undo stack */

  // append end of insertion line to end of s
  s[nlines-1].chars = realloc(
      s[nlines-1].chars,
      s[nlines-1].len 
      + bee->buf[y].len - x
      + 1); 
  strcat(s[nlines-1].chars,
     bee->buf[y].chars + x);
  s[nlines-1].len += bee->buf[y].len - x;
  s[nlines-1].cap = s[nlines-1].len;

  // insert first line
  bee->buf[y].chars[x] = '\0';
  bee->buf[y].chars = realloc(bee->buf[y].chars,
      x + s[0].len +1);
  strcat(bee->buf[y].chars, s[0].chars);
  bee->buf[y].len = x + s[0].len;
  bee->buf[y].cap += s[0].len;
  free(s[0].chars);

  if(nlines > 1){
    // make room for the insertion
    bee->buf = realloc(bee->buf, sizeof(struct string)*(bee->buf_len+nlines-1));
    memmove(
	&bee->buf[y+nlines],
	&bee->buf[y+1],
	(bee->buf_len - y -1) * sizeof(struct string));

    for(int i=1; i<nlines; i++){
      bee->buf[y+i] = s[i];
    }

    bee->buf_len += nlines-1;
  }
  return ch;
}

static inline struct change_stack *bee_delete(struct bee *bee, int x, int y, int xx, int yy){
  // TODO: properly prevent cases where we delete the whole thing
  // we always have to preserve at least one empty line
  if(bee->buf_len == 1 && x == 0 && xx == 0 && y == 0 && yy == 0 && bee->buf[0].len==0)
    return NULL;

/* BEGIN save to undo stack */
  struct change_stack *ch = malloc(sizeof(struct change_stack));
  ch->y = bee->y; ch->bx = bee->bx; ch->vx = bee->vx;
  ch->toprow = bee->toprow; ch->leftcol = bee->leftcol;
  ch->op = INS;
  ch->cmd.i = (struct insert_command){
    .x = x, .y = y, .txt = NULL, .len = yy - y +1
  };
  struct insert_command *icmd = &ch->cmd.i;
  int extra_line = xx == bee->buf[yy].len ? 1 : 0; // we want to delete the linebreak // TODO: it's not that easy
  icmd->len += extra_line;
  icmd->txt = malloc(sizeof(struct string)*icmd->len);
  if(icmd->len == 1 + extra_line){
    icmd->txt[0].len = icmd->txt[0].cap = xx - x + 1;
    icmd->txt[0].chars = malloc(icmd->txt[0].len + 1);
    memcpy(icmd->txt[0].chars, &bee->buf[y].chars[x], xx - x + 1);
    icmd->txt[0].chars[icmd->txt[0].len] = '\0';
    if(extra_line)
      string_init(&icmd->txt[icmd->len-1]);
  } else {
    // copy first part
    icmd->txt[0].len = icmd->txt[0].cap = bee->buf[y].len - x + 1;
    icmd->txt[0].chars = malloc(icmd->txt[0].len + 1);
    strcpy(icmd->txt[0].chars, &bee->buf[y].chars[x]);
    // copy middle part
    for(int i=1; i<icmd->len-1-extra_line; i++){
      // we can try to avoid copying
      // icmd->txt[i] = bee->buf[y+i];
      icmd->txt[i].cap = icmd->txt[i].len = bee->buf[y+i].len;
      icmd->txt[i].chars = malloc(icmd->txt[i].len + 1);
      strcpy(icmd->txt[i].chars, bee->buf[y+i].chars);
    }
    if(extra_line)
      string_init(&icmd->txt[icmd->len-1]);
    // copy last part
    icmd->txt[icmd->len-1].len = icmd->txt[icmd->len-1].cap = bee->buf[yy].len - xx;
    icmd->txt[icmd->len-1].chars = malloc(icmd->txt[icmd->len-1].len + 1);
    memcpy(icmd->txt[icmd->len-1].chars, bee->buf[yy].chars, xx);
    icmd->txt[icmd->len-1].chars[icmd->txt[icmd->len-1].len] = '\0';
  }
  //ch->next = bee->undo_stack;
  //bee->undo_stack = ch;
/* END save to undo stack */

  assert(x<=xx && y<=yy);
  if(xx == bee->buf[yy].len){
    /* we want to delete the last linebreak, so we join the whole next line to 
    * the end of the last line and we delete one more line */
    yy++; // WARNING: what if now `yy == buf_len`
    xx=-1;
  }

  int len = x + (bee->buf[yy].len - 1 - xx);
  bee->buf[y].chars = realloc(bee->buf[y].chars, len +1);
  bee->buf[y].len = bee->buf[y].cap = len;
  bee->buf[y].chars[x]='\0';
  if(yy < bee->buf_len)
    strcat(&bee->buf[y].chars[x], &bee->buf[yy].chars[xx+1]);

  int lines_to_delete = yy - y;
  if(lines_to_delete > 0){
    // we need to keep these for the undo/redo stack
    //for(int i=0; i<lines_to_delete; i++)
    //  if(y+1+i < bee->buf_len)
    //    string_destroy(&bee->buf[y+1+i]);
    if(yy+1 < bee->buf_len)
      memmove(
        &bee->buf[y+1],
        &bee->buf[yy+1],
        (bee->buf_len-yy)*sizeof(struct string));

    bee->buf_len -= lines_to_delete;
  }
  return ch;
}

void change_stack_destroy(struct change_stack *cs){
  struct change_stack *aux;
  while(cs){
    aux = cs->next;
    if(cs->op == INS){
      for(int i=0; i<cs->cmd.i.len; i++)
	free(cs->cmd.i.txt[i].chars);
      free(cs->cmd.i.txt);
    }
    free(cs);
    cs = aux;
  }
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

// TODO
static inline void save_file(){
}

static inline void print_tb(int x, int y, char* c){
  // print nothing, it doesn't matter
  if(*c == '\t')
    return;
  if(bytelen(c)==1)
    tb_set_cell(x, y, *c, fg_color, bg_color);
  else {
      wchar_t wc;
      mbstowcs(&wc, c, 1);
      tb_set_cell(x, y, wc, fg_color, bg_color);
  }
}

static inline char *skip_n_col(char *s, int n, int *remainder){
  *remainder = 0;
  if(s==NULL || s[0]=='\0') return NULL;
  while(n > 0){
    *remainder = n - columnlen(s, 0);
    n -= columnlen(s, 0);
    s += bytelen(s);
    if(*s=='\0') return NULL;
  }
  return s;
}

static inline void println(int x, int y, char* s, int maxx){
  if(s == NULL) return;
  while(x<maxx && *s != '\0'){
    print_tb(x, y, s);
    x += columnlen(s, x);
    s += bytelen(s);
  }
}

const char *footer_format = "<%s>  \"%s\"  [=%d]  L%d C%d";
static inline void print_footer(const struct bee *bee){
  uintattr_t bg = fg_color;
  uintattr_t fg = TB_MAGENTA;
  for(int x=0; x<tb_width(); x++)
    tb_print(x, tb_height()-1, fg, bg, " ");

  char *mode = mode_label[bee->mode];
  int buf_len = bee->mode == INSERT ?
    bee->buf_len + bee->y - bee->ins_y : bee->buf_len;
  tb_printf(0, tb_height() - 1, fg, bg, footer_format,
            mode, bee->filename, buf_len, bee->y, bee->vx);
}

static inline void print_insert_buffer(const struct bee *bee, int *x, int *y){
  char *c = bee->ins_buf.chars;
  while(*c){
    if(*c == '\n'){
      (*y)++;
      *x = 0;
      // TODO: skip chars at the left of bee->leftcol 
    } else {
      print_tb(*x, *y, c);
      (*x) += columnlen(c, *x);
    }
    c += bytelen(c);
  }
}

static inline void print_screen(const struct bee *bee){
  tb_clear();
  int remainder;
  char *s;
  if(bee->mode == INSERT){
    // before insert buffer
    for(int j=0; bee->toprow+j<bee->buf_len && bee->toprow+j<bee->ins_y; j++){
      s = skip_n_col(bee->buf[bee->toprow+j].chars, bee->leftcol, &remainder);
      println(remainder, j, s, screen_width);
    }

    // insert buffer
    s = skip_n_col(bee->buf[bee->ins_y].chars, bee->leftcol, &remainder);
    println(remainder, bee->ins_y-bee->toprow, bee->buf[bee->ins_y].chars, bee->ins_vx);
    int xx = bee->ins_vx - bee->leftcol; // relative to the screen
    int yy = bee->ins_y - bee->toprow; // relative to the screen
    print_insert_buffer(bee, &xx, &yy);
    println(xx, yy, bee->buf[bee->ins_y].chars + bee->ins_bx, screen_width);
    tb_set_cursor(xx, yy);

    // after insert buffer
    //int num_lines_inserted = bee->y - bee->ins_y;
    //for(int j = yy+1; j<screen_height && bee->toprow+j < bee->buf_len; j++){
    //  s = skip_n_col(
    //      bee->buf[bee->toprow+j-num_lines_inserted].chars, bee->leftcol, &remainder);
    //  println(remainder, j, s, screen_width);
    //}
    for(int j=0; j+yy+1<screen_height && bee->ins_y+j < bee->buf_len; j++){
      s = skip_n_col(
	  bee->buf[bee->ins_y+j+1].chars, bee->leftcol, &remainder);
      println(remainder, yy+1+j, s, screen_width);
    }
  }
  else { // mode != INSERT
    for(int j=0; j < screen_height && j+bee->toprow < bee->buf_len; j++){
      s = skip_n_col(bee->buf[bee->toprow+j].chars, bee->leftcol, &remainder);
      //s = bee->buf[bee->toprow+j].chars; remainder = 0;
      if(s)
      println(remainder, j, s, screen_width);
    }
    tb_set_cursor(bee->vx - bee->leftcol, bee->y - bee->toprow);
  }
  
  print_footer(bee);
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

static inline void autoscroll_x(struct bee *bee){
  // cursor too far to the right
  if(bee->vx + columnlen(current_char_ptr(bee), bee->vx) > screen_width + bee->leftcol){
    int bx_leftcol, vx_leftcol;
    vx_to_bx(bee->buf[bee->y].chars, bee->leftcol, &bx_leftcol, &vx_leftcol);
    while(bee->vx + columnlen(current_char_ptr(bee), bee->vx) > screen_width + bee->leftcol){
      bee->leftcol += columnlen(bee->buf[bee->y].chars + bx_leftcol, bee->leftcol);
      bx_leftcol += bytelen(bee->buf[bee->y].chars + bx_leftcol);
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
  if(bee->y - bee->toprow >= screen_height)
    bee->toprow = bee->y-screen_height+1;
}

static inline void autoscroll(struct bee *bee){
  autoscroll_y(bee);
  autoscroll_x(bee);
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
  //if(bee->bx + bytelen(current_char_ptr(bee)) < current_line_ptr(bee)->len){
  // allow go till the linebreak character
  if(bee->bx + bytelen(current_char_ptr(bee)) <= current_line_ptr(bee)->len){
    bee->vx += columnlen(current_char_ptr(bee), bee->vx);
    bee->bx += bytelen(current_char_ptr(bee));
    autoscroll_x(bee);
  }
  bee->vxgoal = bee->vx;
}

static inline void n_l_pastend(struct bee *bee){
  // you can go past the end of the line so you can append at the end of the line
  if(bee->bx + bytelen(current_char_ptr(bee)) <= current_line_ptr(bee)->len){
    bee->vx += columnlen(current_char_ptr(bee), bee->vx);
    bee->bx += bytelen(current_char_ptr(bee));
    autoscroll_x(bee);
  }
  bee->vxgoal = bee->vx;
}

static inline void n_j(struct bee *bee){
  if(bee->y +1 == bee->buf_len) return;
  bee->y++;

  // adjust column position
  vx_to_bx(current_line_ptr(bee)->chars, bee->vxgoal, &bee->bx, &bee->vx);

  autoscroll(bee);
}
static inline void n_k(struct bee *bee){
  if(bee->y == 0) return;
  bee->y--;

  // adjust column position
  vx_to_bx(current_line_ptr(bee)->chars, bee->vxgoal, &bee->bx, &bee->vx);

  autoscroll(bee);
}

static inline void n_x(struct bee *bee){
  change_stack_destroy(bee->redo_stack);
  bee->redo_stack = NULL;
  struct change_stack *old_undo_stack = bee->undo_stack;

  bee->undo_stack = bee_delete(bee, bee->bx, bee->y, bee->bx, bee->y);
  bee->undo_stack->next = old_undo_stack;

  if(bee->bx != 0 && bee->bx == bee->buf[bee->y].len)
    n_h(bee);
  if(bee->y == bee->buf_len){
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

static inline void resize(const struct bee *bee){
}

static inline void bee_save_cursor(const struct bee*bee, struct change_stack *ch){
  ch->y = bee->y; ch->bx = bee->bx; ch->vx = bee->vx;
  ch->toprow = bee->toprow; ch->leftcol = bee->leftcol;
}
static inline void bee_restore_cursor(struct bee *bee, const struct change_stack *ch){
  bee->y = ch->y; bee->bx = ch->bx; bee->vx = ch->vx;
  bee->toprow = ch->toprow; bee->leftcol = ch->leftcol;
}

static inline struct change_stack *apply_cmd_ins(struct bee *bee, struct insert_command c){
  return bee_insert(bee, c.x, c.y, c.txt, c.len);
}
static inline struct change_stack *apply_cmd_del(struct bee *bee, struct delete_command c){
  return bee_delete(bee, c.x, c.y, c.xx, c.yy);
}



/*
 * @brief returns the change needed to revert the applied change
 */
static inline struct change_stack *apply_change(struct bee *bee, struct change_stack *ch){
  struct change_stack *result;
  switch(ch->op){
    case INS:
      result = apply_cmd_ins(bee, ch->cmd.i);
      break;
    case DEL:
      result = apply_cmd_del(bee, ch->cmd.d);
      break;
  }
  bee_restore_cursor(bee, ch);
  return result;
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

  cc->next = bee->redo_stack ? bee->redo_stack->next : NULL;
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

  cc->next = bee->redo_stack ? bee->undo_stack->next : NULL;
  bee->undo_stack = cc;
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
  case 'a':
    n_a(bee); break;
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
  case 'u':
    n_u(bee); break;
  }
  else if(ev.key!=0) switch(ev.key){
  case TB_KEY_CTRL_Q:
    return 0;
  case TB_KEY_CTRL_R:
    n_Cr(bee); break;
  }
  return 1;
}

static inline void i_esc(struct bee *bee){
  change_stack_destroy(bee->redo_stack);
  bee->redo_stack = NULL;
  struct change_stack *old_undo_stack = bee->undo_stack;

  int num_lines_inserted = bee->y - bee->ins_y;
  int num_lines_ins_buf = num_lines_inserted +1;
  struct string *inserted_lines = string_split_lines(&bee->ins_buf, num_lines_ins_buf);
  bee->undo_stack = bee_insert(bee, bee->ins_bx, bee->ins_y, inserted_lines, num_lines_ins_buf);
  bee->undo_stack->next = old_undo_stack;

  bee->vxgoal = bee->vx;
  bee->mode = NORMAL;
}

static inline char insert_read_key(struct bee *bee){
  struct tb_event ev;
  tb_poll_event(&ev);
  if(ev.type == TB_EVENT_RESIZE) resize(bee);
  else if(ev.key!=0) switch(ev.key){
    case TB_KEY_ESC:
      i_esc(bee); break;
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
  bee->ins_buf.chars = NULL;
  bee->undo_stack = bee->redo_stack = NULL;
}

static inline void bee_destroy(struct bee *bee){
  for(int i=0; i<bee->buf_len; i++)
    string_destroy(&bee->buf[i]);
  free(bee->buf);
  if(bee->filename)
    free(bee->filename);
  change_stack_destroy(bee->undo_stack);
  change_stack_destroy(bee->redo_stack);
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
  tb_set_clear_attrs(fg_color, bg_color);
  //tb_set_input_mode(TB_INPUT_ESC); // default
  //tb_set_input_mode(TB_INPUT_ALT);

  do {
    print_screen(&bee);
  } while(read_key(&bee));

  tb_shutdown();
  bee_destroy(&bee);

  return 0;
}
