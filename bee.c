#include "text.h"
#define TB_IMPL
#include "termbox2.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <libgen.h>

#define LOCALE "en_US.UTF-8"
#define FG_COLOR TB_WHITE
#define BG_COLOR TB_BLACK
#define tablen 8
#define footerheight 1
#define FOOTER_FG TB_MAGENTA
#define FOOTER_BG TB_BLACK
#define MARGIN_LEN 4
#define MARGIN_FG TB_MAGENTA
#define MARGIN_BG TB_BLACK
#define screen_height (tb_height() - footerheight)
#define screen_width (tb_width())
#define YY bee->y
#define XX bee->bx
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define ABS(a) ((a)>=0?(a):-(a))

//#define DEBUG
#ifdef DEBUG
#include <time.h>
inline static void flog(const char *msg){
  time_t t = time(NULL);
  FILE *f = fopen("./bee.log", "a");
  fprintf(f, "%s - %s\n", ctime(&t), msg);
  assert(0 == fclose(f));
}
#else
inline static void flog(const char *msg){}
#endif

enum mode {
  NORMAL, INSERT, COMMAND
};

char *mode_label[] = {"N", "I", "C"};

enum operation { INS, DEL};
struct change_stack{
  int y, bx, vx, leftcol, toprow;
  struct change_stack *next;
  enum operation op;
  union {
    struct insert_cmd i;
    struct delete_cmd d;
  } cmd;
};

struct bee {
  enum mode mode;
  struct text buf;
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
  s->p = calloc(s->cap+1, sizeof(char));
}

void string_destroy(struct string *s){
  free(s->p);
  s->p = NULL;
  s->cap = 0;
  s->len = 0;
}

void string_append(struct string *s, const char *t){
  if(s->len + (int)strlen(t) > s->cap)
    s->p = realloc(s->p, s->cap*=2);
  strcat(s->p + s->len, t);
  s->len += strlen(t);
}

/**
 * @brief Splits a string with linesbreak into a text struct
 *
 * @warning Takes ownership of `str`.
 * The caller must not use or free `str` after this call.
 */
struct text text_from_string(struct string *str, int nlines){
  struct text retval;
  char *s = str->p;
  retval.p = malloc(nlines*sizeof(struct string));
  retval.len = nlines;

  for(int i=0; i<nlines; i++){
    char *end = strchr(s, '\n');
    end = end ? end : s + strlen(s);
    retval.p[i].p = malloc(end-s+1);
    memcpy(retval.p[i].p, s, end-s);
    retval.p[i].p[end-s] = '\0'; // null terminated string
    retval.p[i].cap = retval.p[i].len = end-s;
    s = end+1;
  }

  string_destroy(str);
  return retval;
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

void change_stack_destroy(struct change_stack *cs){
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

static inline struct string *load_file(const char *filename, int *len){
  if (filename == NULL) return 0;
  FILE *fp = fopen(filename, "r");
  assert(fp);
  int res = fseek(fp, 0, SEEK_END);
  assert(res == 0);
  long fsize = ftell(fp);
  assert(fsize >= 0);
  if(fsize==0){
    *len = 1;
    struct string *retval = malloc(sizeof(struct string));
    retval[0].cap = retval[0].len = 0;
    retval[0].p = calloc(1,1);
    return retval;
  }
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
    buf[i].p = calloc(linelen+1, sizeof(char));
    if(linelen>0)
      memcpy(buf[i].p , &fcontent[j], linelen);
    buf[i].len = buf[i].cap = linelen;

    j += linelen+1;
  }
  free(fcontent);
  // calle should free buf
  return buf;
}

static inline void save_file(const struct text *txt, const char *filename){
  char *tmp = malloc(strlen(filename)+strlen(".bee.bak")+1);
  sprintf(tmp, "%s.bee.bak", filename);
  assert(0==rename(filename, tmp));
  FILE *f = fopen(filename, "a");
  assert(f);
  for(int i=0; i<txt->len; i++)
    fprintf(f, "%s\n", txt->p[i].p);
  assert(0==fclose(f));
  assert(0==remove(tmp));
  free(tmp);
}

static inline void print_tb(int x, int y, char* c){
  // print nothing, it doesn't matter
  if(*c == '\t')
    return;
  if(bytelen(c)==1)
    tb_set_cell(x, y, *c, FG_COLOR, BG_COLOR);
  else {
      wchar_t wc;
      mbstowcs(&wc, c, 1);
      tb_set_cell(x, y, wc, FG_COLOR, BG_COLOR);
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
  for(int x=0; x<tb_width(); x++)
    tb_print(x, tb_height()-1, MARGIN_FG, MARGIN_BG, " ");

  char *mode = mode_label[bee->mode];
  int buf_len = bee->mode == INSERT ?
    bee->buf.len + bee->y - bee->ins_y : bee->buf.len;
  tb_printf(0, tb_height() - 1, MARGIN_FG, MARGIN_BG, footer_format,
            mode, bee->filename, buf_len, bee->y, bee->vx);
}

static inline void print_insert_buffer(const struct bee *bee, int *x, int *y){
  char *c = bee->ins_buf.p;
  while(*c){
    if(*c == '\n'){
      (*y)++;
      *x = 0;
      // TODO: skip chars at the left of bee->leftcol 
    } else {
      print_tb(*x+MARGIN_LEN, *y, c);
      (*x) += columnlen(c, *x);
    }
    c += bytelen(c);
  }
}

static inline void print_margin(const struct bee *bee){
  for(int i=0; i<screen_height; i++)
    if(i+bee->toprow < bee->buf.len)
      tb_printf(0, i, MARGIN_FG, MARGIN_BG, "%-3d ", ABS(i+bee->toprow-bee->y));
}

static inline void print_screen(const struct bee *bee){
  tb_clear();
  int remainder;
  char *s;
  if(bee->mode == INSERT){
    // before insert buffer
    for(int j=0; bee->toprow+j<bee->buf.len && bee->toprow+j<bee->ins_y; j++){
      s = skip_n_col(bee->buf.p[bee->toprow+j].p, bee->leftcol, &remainder);
      println(remainder+MARGIN_LEN, j, s, screen_width);
    }

    // insert buffer
    s = skip_n_col(bee->buf.p[bee->ins_y].p, bee->leftcol, &remainder);
    println(remainder+MARGIN_LEN, bee->ins_y - bee->toprow, bee->buf.p[bee->ins_y].p, bee->ins_vx+MARGIN_LEN);
    int xx = bee->ins_vx - bee->leftcol; // relative to the screen
    int yy = bee->ins_y - bee->toprow; // relative to the screen
    print_insert_buffer(bee, &xx, &yy);
    println(xx+MARGIN_LEN, yy, bee->buf.p[bee->ins_y].p + bee->ins_bx, screen_width);
    tb_set_cursor(xx+MARGIN_LEN, yy);

    // after insert buffer
    //int num_lines_inserted = bee->y - bee->ins_y;
    //for(int j = yy+1; j<screen_height && bee->toprow+j < bee->buf.len; j++){
    //  s = skip_n_col(
    //      bee->buf.p[bee->toprow+j-num_lines_inserted].p, bee->leftcol, &remainder);
    //  println(remainder, j, s, screen_width);
    //}
    for(int j=0; j+yy+1<screen_height && bee->ins_y+j < bee->buf.len-1; j++){
      s = skip_n_col(
	  bee->buf.p[bee->ins_y+j+1].p, bee->leftcol, &remainder);
      println(remainder+MARGIN_LEN, yy+1+j, s, screen_width);
    }
  }
  else { // mode != INSERT
    for(int j=0; j < screen_height && j+bee->toprow < bee->buf.len; j++){
      s = skip_n_col(bee->buf.p[bee->toprow+j].p, bee->leftcol, &remainder);
      //s = bee->buf.p[bee->toprow+j].p; remainder = 0;
      if(s)
      println(remainder+MARGIN_LEN, j, s, screen_width);
    }
    if(bee->bx < bee->buf.p[YY].len)
      tb_set_cursor(bee->vx+MARGIN_LEN - bee->leftcol, bee->y - bee->toprow);
    else {
      tb_hide_cursor();
      tb_set_cell(bee->vx+MARGIN_LEN - bee->leftcol, bee->y - bee->toprow, ' ', FG_COLOR, TB_CYAN);
    }

  }
  
  print_footer(bee);
  print_margin(bee);
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
  if(bee->vx + columnlen(&bee->buf.p[YY].p[XX], bee->vx) > screen_width + bee->leftcol - MARGIN_LEN){
    int bx_leftcol, vx_leftcol;
    vx_to_bx(bee->buf.p[bee->y].p, bee->leftcol, &bx_leftcol, &vx_leftcol);
    while(bee->vx + columnlen(&bee->buf.p[YY].p[XX], bee->vx) > screen_width + bee->leftcol - MARGIN_LEN){
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
  if(bee->y - bee->toprow >= screen_height)
    bee->toprow = bee->y-screen_height+1;
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
  //if(bee->bx + bytelen(&bee->buf.p[YY].p[XX]) < bee->buf.p[YY].len){
  // allow go till the linebreak character
  if(bee->bx + bytelen(&bee->buf.p[YY].p[XX]) <= bee->buf.p[YY].len){
    bee->vx += columnlen(&bee->buf.p[YY].p[XX], bee->vx);
    bee->bx += bytelen(&bee->buf.p[YY].p[XX]);
    autoscroll_x(bee);
  }
  bee->vxgoal = bee->vx;
}

static inline void n_l_pastend(struct bee *bee){
  // you can go past the end of the line so you can append at the end of the line
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

static inline void resize(const struct bee *bee){
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
  case TB_KEY_CTRL_W:
    save_file(&bee->buf, bee->filename);
  }
  return 1;
}

static inline void i_esc(struct bee *bee){
  if(bee->ins_buf.len == 0) return;
  change_stack_destroy(bee->redo_stack);
  bee->redo_stack = NULL;
  struct change_stack *old_undo_stack = bee->undo_stack;

  int num_lines_inserted = bee->y - bee->ins_y;
  int num_lines_ins_buf = num_lines_inserted +1;
  struct text inserted_lines = text_from_string(&bee->ins_buf, num_lines_ins_buf);

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
  bee->buf.len = 0;
  bee->y = 0;
  bee->leftcol = bee->toprow = 0;
  bee->bx = bee->vx = bee->vxgoal = 0;
  bee->ins_buf.p = NULL;
  bee->undo_stack = bee->redo_stack = NULL;
}

static inline void bee_destroy(struct bee *bee){
  for(int i=0; i<bee->buf.len; i++)
    string_destroy(&bee->buf.p[i]);
  free(bee->buf.p);
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

  bee.buf.p = load_file(bee.filename, &bee.buf.len);

  tb_init();
  tb_set_clear_attrs(FG_COLOR, BG_COLOR);
  //tb_set_input_mode(TB_INPUT_ESC); // default
  //tb_set_input_mode(TB_INPUT_ALT);

  do {
    print_screen(&bee);
  } while(read_key(&bee));

  tb_shutdown();
  bee_destroy(&bee);

  return 0;
}
