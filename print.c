#include "bee.h"
#include "termbox2.h"
#include "text_util.h"
#include <stdbool.h>

static inline void print_tb(int x, int y, char *c) {
  // print nothing, it doesn't matter
  if (*c == '\t')
    return;
  if (bytelen(c) == 1)
    tb_set_cell(x, y, *c, FG_COLOR, BG_COLOR);
  else {
    wchar_t wc;
    mbstowcs(&wc, c, 1);
    tb_set_cell(x, y, wc, FG_COLOR, BG_COLOR);
  }
}

static inline char* println_max(int xoff, int y, char *s, int maxx) {
  if (s == NULL)
    return NULL;
  int x = 0;
  while (x < maxx && *s != '\0') {
    print_tb(x + xoff, y, s);
    x += columnlen(s, x);
    s += bytelen(s);
  }
  return s;
}

static inline void println(int xoff, int y, char *s) {
  if (s == NULL)
    return;
  int x = 0;
  while (*s) {
    print_tb(x + xoff, y, s);
    x += columnlen(s, x);
    s += bytelen(s);
  }
}

const char *FOOTER_FORMAT = "<%s>  \"%s\"  [=%d]  L%d C%d";

static inline void print_footer(const struct bee *bee) {
  for (int x = 0; x < tb_width(); x++)
    tb_print(x, tb_height() - 1, FOOTER_FG, FOOTER_BG, " ");

  const char *mode = mode_label[bee->mode];

  if (bee->mode == COMMAND) {
    tb_printf(0, tb_height() - 1, FOOTER_FG, FOOTER_BG, "<C>  %s",
              bee->cmd_buf.p);
  } else {
    int buf_len =
        bee->mode == INSERT ? bee->buf.len + bee->y - bee->ins_y : bee->buf.len;
    tb_printf(0, tb_height() - 1, FOOTER_FG, FOOTER_BG, FOOTER_FORMAT, mode,
              bee->filename, buf_len, bee->y, bee->vx);
  }
}

// static inline void print_insert_buffer(const struct bee *bee, int *x, int
// *y){
//   char *c = bee->ins_buf.p;
//   while(*c){
//     if(*c == '\n'){
//       (*y)++;
//       *x = 0;
//       // TODO: skip chars at the left of bee->leftcol
//     } else {
//       print_tb(*x+MARGIN_LEN, *y, c);
//       (*x) += columnlen(c, *x);
//     }
//     c += bytelen(c);
//   }
// }

// static inline void print_margin(const struct bee *bee){
//   int nlines_ins_buf = 0;
//   if(bee->mode == INSERT){
//     for(int i=0; i<bee->ins_buf.len; i++)
//       if(bee->ins_buf.p[i] == '\n')
//	nlines_ins_buf++;
//   }
//   for(int i=0; i<SCREEN_HEIGHT; i++)
//     if(i+bee->toprow < bee->buf.len + nlines_ins_buf)
//       tb_printf(0, i, MARGIN_FG, MARGIN_BG, "%-3d ",
//       ABS(i+bee->toprow-bee->y));
// }

// static inline void print_cursor(const struct bee *bee){
//   if(bee->mode == COMMAND){
//     int x = 0;
//     for(int i=0; i<bee->cmd_buf.len; i++)
//       x += bytelen(&bee->cmd_buf.p[i]);
//     tb_set_cursor( 5 + x, tb_height()-1);
//   }
//   else if(bee->bx == bee->buf.p[YY].len && bee->mode != INSERT){
//     tb_hide_cursor();
//     tb_set_cell(bee->vx+MARGIN_LEN - bee->leftcol, bee->y - bee->toprow, ' ',
//     FG_COLOR, TB_CYAN);
//   } else {
//     tb_set_cursor(bee->vx+MARGIN_LEN - bee->leftcol, bee->y - bee->toprow);
//   }
// }

struct text string_to_virtual_lines(const struct string s, int line_len) {
  struct text t;
  text_init(&t);
  char *c = s.p;
  while (c) {
    char *cc = skip_n_col(c, line_len);
    struct string ss;
    string_init(&ss);
    if (cc) {
      char *cs = malloc(cc - c + 1);
      cs[cc - c] = '\0'; // null terminated
      strncpy(cs, c, cc - c);
      ss.p = cs;
      ss.len = ss.cap = cc - c;
    } else {
      ss.len = ss.cap = strlen(c);
      ss.p = malloc(ss.len + 1);
      strcpy(ss.p, c);
    }
    text_append(&t, ss);
    c = cc;
  }
  return t;
}

struct text get_ins_buf_cpy(const struct bee *bee) {
  int yy = bee->ins_y;
  int xx = bee->ins_bx;
  struct text ins_buf_cpy;
  text_clone(&ins_buf_cpy, &bee->ins_buf);
  char *s1 = malloc(xx + 1);
  strncpy(s1, bee->buf.p[yy].p, xx);
  s1[xx] = '\0';
  int l = bee->buf.p[yy].len - xx + 1;
  char *s2 = malloc(l);
  strcpy(s2, &bee->buf.p[yy].p[xx]);
  string_prepend(&ins_buf_cpy.p[0], s1);
  string_append(&ins_buf_cpy.p[ins_buf_cpy.len - 1], s2);
  return ins_buf_cpy;
}

int get_ncol(const char *c){
  int ncol = 0;
  while(*c){
    ncol+=columnlen(c, ncol);
    c+=bytelen(c);
  }
  return ncol;
}
// skip_n_col
bool print_ncol(int x, int y, int n, const char *c);
bool print_ncol_up(int x, int y, int n, const char *c);


void print_screen_old(const struct bee *bee) {
  tb_clear();

  if (SCREEN_HEIGHT < 4 || SCREEN_WIDTH < 8) // print nothing
    return;

  switch (bee->mode) {
  case INSERT: {
    // TODO
    struct text ib = get_ins_buf_cpy(bee);

    //struct {int x, y;} vm[SCREEN_HEIGHT];
    char *vm[SCREEN_HEIGHT];

    int m = SCREEN_HEIGHT - SCREEN_HEIGHT/2 -1;

    // map
    for(int i=0; i<SCREEN_HEIGHT; i++)
      vm[i] = NULL;

    // map bottom half of the screen
    int vj = m;
    int j = YY;
    char *c = &ib.p[ib.len-1].p[(XX/SCREEN_WIDTH)*SCREEN_WIDTH]; 
    while(vj<SCREEN_HEIGHT && j<bee->buf.len){
      do {
	vm[vj] = c;
	c = skip_n_col(c, SCREEN_WIDTH);
	vj++;
      } while(c && vj<SCREEN_HEIGHT && j<bee->buf.len);
      j++;
      c = bee->buf.p[j].p;
    }

    // map top half of the screen
    j = YY;
    vj = m-1;
    // TODO: review if next line is okay
    c = ib.len>1 || ib.p[0].len>SCREEN_WIDTH ? ib.p[ib.len-1].p : 
      (YY > 0 ? bee->buf.p[YY-1].p : NULL);
    while(vj>=0 && j>=0){
    // TODO
      vj--;
      j--;
    }

    // print
    for(int i=0; i<SCREEN_HEIGHT; i++){
      if(vm[i]==NULL)
	continue;
      //struct text b = vm[i].y<bee->ins_y || vm[i].y>=bee->ins_y+bee->ins_buf.len ? bee->buf : ib;
      //c = &b.p[vm[i].y].p[vm[i].x];
      println_max(MARGIN_LEN, i, vm[i], SCREEN_WIDTH); 
    }
  } break;
  default: {
    // pointers to lines in file
    int fi, fj;
    // pointers to lines on screen
    int si = SCREEN_HEIGHT - SCREEN_HEIGHT / 2 - 1;
    int sj = si - 1;

    int cursor_in_t = bee->vx / SCREEN_WIDTH;
    struct text t;
    // print current line
    fi = fj = YY;
    t = string_to_virtual_lines(bee->buf.p[YY], SCREEN_WIDTH);
    for (int i = cursor_in_t; i < t.len && si < SCREEN_HEIGHT; i++) {
      if (i == 0)
        tb_printf(0, si, MARGIN_FG, MARGIN_BG, "%-3d ", fi);
      println(MARGIN_LEN, si, t.p[i].p);
      si++;
    }
    fi++;
    for (int j = cursor_in_t - 1; j >= 0 && sj >= 0; j--) {
      if (j == 0)
        tb_printf(0, sj, MARGIN_FG, MARGIN_BG, "%-3d ", fj);
      println(MARGIN_LEN, sj, t.p[j].p);
      sj--;
    }
    fj--;
    text_deinit(&t);
    // end print current line
    // print down
    while (fi < bee->buf.len && si < SCREEN_HEIGHT) {
      t = string_to_virtual_lines(bee->buf.p[fi], SCREEN_WIDTH);
      for (int i = 0; i < t.len && si < SCREEN_HEIGHT; i++) {
        if (i == 0)
          tb_printf(0, si, MARGIN_FG, MARGIN_BG, "%-3d ", fi);
        println(MARGIN_LEN, si, t.p[i].p);
        si++;
      }
      text_deinit(&t);
      fi++;
    }
    // print up
    while (fj >= 0 && sj >= 0) {
      t = string_to_virtual_lines(bee->buf.p[fj], SCREEN_WIDTH);
      for (int j = t.len - 1; j >= 0 && sj >= 0; j--) {
        if (j == 0)
          tb_printf(0, sj, MARGIN_FG, MARGIN_BG, "%-3d ", fj);
        println(MARGIN_LEN, sj, t.p[j].p);
        sj--;
      }
      text_deinit(&t);
      fj--;
    }
    tb_set_cursor((bee->vx % SCREEN_WIDTH) + MARGIN_LEN,
                  SCREEN_HEIGHT - SCREEN_HEIGHT / 2 - 1);
  } break;
  }

  print_footer(bee);
  // print_margin(bee);
  // print_cursor(bee);
  tb_present();
}

void print_screen(const struct bee *bee) {
  tb_clear();
  char vs[SCREEN_HEIGHT][SCREEN_WIDTH+1]; // virtual screen
  int lidx[SCREEN_HEIGHT];

  for(int j=0; j<SCREEN_HEIGHT; j++){
    vs[j][0] = '\0';
    lidx[j] = -1;
  }

  int m = SCREEN_HEIGHT - SCREEN_HEIGHT/2 -1;
  bool is_insert_mode = bee->mode == INSERT;

  // map insert buffer (in insert mode) or current line (otherwise)
  // TODO
  int k1, k2;
  k1 = k2 = 0;
  if(!is_insert_mode){
    int nnn = bee->buf.p[bee->y].len / SCREEN_WIDTH;
    int nn=bee->bx/SCREEN_WIDTH;
    k1+=nn;
    k2+=nnn-nn;
    for(int n=nn-1, j=m-1; n>=0 && j >= 0; n--){
      if(n==0) lidx[j] = 0;
      strlcpy(vs[j], &bee->buf.p[bee->y].p[n*SCREEN_WIDTH], SCREEN_WIDTH+1);
      j--;
    }
    for(int n = nn*SCREEN_WIDTH, j=m; n <= bee->buf.p[bee->y].len && j<SCREEN_HEIGHT; n+=SCREEN_WIDTH) {
      if(n==0) lidx[j] = 0;
      strlcpy(vs[j], &bee->buf.p[bee->y].p[n], SCREEN_WIDTH+1);
      j++;
    }
  } else if (bee->ins_buf.len == 1){
    int len = bee->ins_buf.p[0].len+bee->buf.p[bee->y].len;
    char l[len+1];
    memcpy(l, bee->buf.p[bee->y].p, bee->ins_bx);
    memcpy(&l[bee->ins_bx], bee->ins_buf.p[0].p, bee->ins_buf.p[0].len);
    memcpy(&l[bee->ins_bx+bee->ins_buf.p[0].len], 
        &bee->buf.p[bee->y].p[bee->ins_bx], 
        bee->buf.p[bee->y].len - bee->ins_bx);
    l[len]='\0';

    int nnn = len / SCREEN_WIDTH;
    int nn=bee->bx/SCREEN_WIDTH;
    k1+=nn;
    k2+=nnn-nn;
    for(int n=nn-1, j=m-1; n>=0 && j >= 0; n--){
      if(n==0) lidx[j] = 0;
      strlcpy(vs[j], &l[n*SCREEN_WIDTH], SCREEN_WIDTH+1);
      j--;
    }
    for(int n = nn*SCREEN_WIDTH, j=m; n <= len && j<SCREEN_HEIGHT; n+=SCREEN_WIDTH) {
      if(n==0) lidx[j] = 0;
      strlcpy(vs[j], &l[n], SCREEN_WIDTH+1);
      j++;
    }
  } else {
    // TODO
  }

  // map bottom half of the screen
  for(int j=m+1+k2, jj=bee->y+1; j<SCREEN_HEIGHT && jj<bee->buf.len; jj++){
    lidx[j] = jj-bee->y;

    for(int n = 0; n <= bee->buf.p[jj].len && j<SCREEN_HEIGHT; n+=SCREEN_WIDTH) {
      strlcpy(vs[j], &bee->buf.p[jj].p[n], SCREEN_WIDTH+1);
      j++;
    }
  }

  // map top half of the screen
  for(int j = m-1-k1, jj=bee->y-1; j>=0 && jj >=0; jj--){
    // number of screen lines -1 in file line
    int n = bee->buf.p[jj].len / SCREEN_WIDTH; 
    for(; n>=0 && j>=0; n--){
      strlcpy(vs[j], &bee->buf.p[jj].p[n*SCREEN_WIDTH], SCREEN_WIDTH+1);
      j--;
    }
    if(j+1>=0) lidx[j+1] = bee->y-jj;
  }

  // print
  for(int j=0; j<SCREEN_HEIGHT; j++){
    println(MARGIN_LEN, j, vs[j]);
    if(lidx[j]>=0){
      if(lidx[j]==0)
        tb_print(0, j, MARGIN_FG, MARGIN_BG, " 0 ");
      else
        tb_printf(0, j, MARGIN_FG, MARGIN_BG, "%-3d ", lidx[j]);
    }
  }

  print_footer(bee);
  tb_set_cursor((bee->vx % SCREEN_WIDTH) + MARGIN_LEN, m);
  tb_present();
}

