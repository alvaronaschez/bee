#include "termbox2.h"
#include "text_util.h"

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

static inline void println(int x, int y, char* s, int maxx){
  if(s == NULL) return;
  while(x<maxx && *s != '\0'){
    print_tb(x, y, s);
    x += columnlen(s, x);
    s += bytelen(s);
  }
}

const char *FOOTER_FORMAT = "<%s>  \"%s\"  [=%d]  L%d C%d";

static inline void print_footer(const struct bee *bee){
  for(int x=0; x<tb_width(); x++)
    tb_print(x, tb_height()-1, FOOTER_FG, FOOTER_BG, " ");
  
  const char *mode = mode_label[bee->mode];

  if(bee->mode == COMMAND){
    tb_printf(0, tb_height() - 1, FOOTER_FG, FOOTER_BG, "<C>  %s", bee->cmd_buf.p);
  } else {
    int buf_len = bee->mode == INSERT ?
      bee->buf.len + bee->y - bee->ins_y : bee->buf.len;
    tb_printf(0, tb_height() - 1, FOOTER_FG, FOOTER_BG, FOOTER_FORMAT,
             mode, bee->filename, buf_len, bee->y, bee->vx);
  }
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
  int nlines_ins_buf = 0;
  if(bee->mode == INSERT){
    for(int i=0; i<bee->ins_buf.len; i++)
      if(bee->ins_buf.p[i] == '\n')
	nlines_ins_buf++;
  }
  for(int i=0; i<SCREEN_HEIGHT; i++)
    if(i+bee->toprow < bee->buf.len + nlines_ins_buf)
      tb_printf(0, i, MARGIN_FG, MARGIN_BG, "%-3d ", ABS(i+bee->toprow-bee->y));
}

static inline void print_cursor(const struct bee *bee){
  if(bee->mode == COMMAND){
    int x = 0;
    for(int i=0; i<bee->cmd_buf.len; i++)
      x += bytelen(&bee->cmd_buf.p[i]);
    tb_set_cursor( 5 + x, tb_height()-1);
  }
  else if(bee->bx == bee->buf.p[YY].len && bee->mode != INSERT){
    tb_hide_cursor();
    tb_set_cell(bee->vx+MARGIN_LEN - bee->leftcol, bee->y - bee->toprow, ' ', FG_COLOR, TB_CYAN);
  } else {
    tb_set_cursor(bee->vx+MARGIN_LEN - bee->leftcol, bee->y - bee->toprow);
  }
}

void print_screen(const struct bee *bee){
  tb_clear();
  int remainder;
  char *s;
  if(bee->mode == INSERT){
    // before insert buffer
    for(int j=0; bee->toprow+j<bee->buf.len && bee->toprow+j<bee->ins_y; j++){
      s = skip_n_col(bee->buf.p[bee->toprow+j].p, bee->leftcol, &remainder);
      println(remainder+MARGIN_LEN, j, s, SCREEN_WIDTH);
    }

    // insert buffer
    s = skip_n_col(bee->buf.p[bee->ins_y].p, bee->leftcol, &remainder);
    println(remainder+MARGIN_LEN, bee->ins_y - bee->toprow, bee->buf.p[bee->ins_y].p, bee->ins_vx+MARGIN_LEN);
    int xx = bee->ins_vx - bee->leftcol; // relative to the screen
    int yy = bee->ins_y - bee->toprow; // relative to the screen
    print_insert_buffer(bee, &xx, &yy);
    println(xx+MARGIN_LEN, yy, bee->buf.p[bee->ins_y].p + bee->ins_bx, SCREEN_WIDTH);

    // after insert buffer
    //int num_lines_inserted = bee->y - bee->ins_y;
    //for(int j = yy+1; j<SCREEN_HEIGHT && bee->toprow+j < bee->buf.len; j++){
    //  s = skip_n_col(
    //      bee->buf.p[bee->toprow+j-num_lines_inserted].p, bee->leftcol, &remainder);
    //  println(remainder, j, s, SCREEN_WIDTH);
    //}
    for(int j=0; j+yy+1<SCREEN_HEIGHT && bee->ins_y+j < bee->buf.len-1; j++){
      s = skip_n_col(
	  bee->buf.p[bee->ins_y+j+1].p, bee->leftcol, &remainder);
      println(remainder+MARGIN_LEN, yy+1+j, s, SCREEN_WIDTH);
    }
  }
  else { // mode != INSERT
    for(int j=0; j < SCREEN_HEIGHT && j+bee->toprow < bee->buf.len; j++){
      s = skip_n_col(bee->buf.p[bee->toprow+j].p, bee->leftcol, &remainder);
      //s = bee->buf.p[bee->toprow+j].p; remainder = 0;
      if(s)
      println(remainder+MARGIN_LEN, j, s, SCREEN_WIDTH);
    }
  }
  
  print_footer(bee);
  print_margin(bee);
  print_cursor(bee);
  tb_present();
}
