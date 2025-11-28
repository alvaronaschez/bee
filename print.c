#include "print.h"

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
        bee->mode == INSERT ? bee->buf.len + bee->y - bee->y : bee->buf.len;
    tb_printf(0, tb_height() - 1, FOOTER_FG, FOOTER_BG, FOOTER_FORMAT, mode,
              bee->filename, buf_len, bee->y, bee->vx);
  }
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
  // we distinguish 3 cases:
  //  1. Not insert mode
  //  2. Insert mode and only one line in the insert buffer
  //  3. Insert mode with multiple lines in the insert buffer
  int k1, k2;
  k1 = k2 = 0;

  char* line = NULL;
  int len;
  if(!is_insert_mode){
    len = bee->buf.p[bee->y].len;
    line = bee->buf.p[bee->y].p;
  } else if(bee->ins_buf.len==1){
    len = bee->ins_buf.p[0].len + bee->buf.p[bee->y].len;
    line = malloc(len+1);
    memcpy(line, bee->buf.p[bee->y].p, bee->bx);
    memcpy(&line[bee->bx], bee->ins_buf.p[0].p, bee->ins_buf.p[0].len);
    memcpy(&line[bee->bx+bee->ins_buf.p[0].len], 
      &bee->buf.p[bee->y].p[bee->bx], 
      bee->buf.p[bee->y].len - bee->bx);
    line[len]='\0';
  } else { // (is_insert_mode && bee->ins_buf.len>1)
    int len1 = bee->ins_buf.p[bee->ins_buf.len-1].len;
    int len2 = bee->buf.p[bee->y].len - bee->bx;
    len = len1 + len2;
    line = malloc(len+1);
    memcpy(line, bee->ins_buf.p[bee->ins_buf.len-1].p, len1);
    memcpy(&line[len1], &bee->buf.p[bee->y].p[bee->bx], len2);
    line[len]='\0';
  }
  int nnn = len/SCREEN_WIDTH;
  int nn=bee->bx/SCREEN_WIDTH;
  //k1+=nn;
  k2+=nnn-nn;
  for(int n=nn-1, j=m-1; n>=0 && j >= 0; n--){
    if(n==0) lidx[j] = 0;
    strlcpy(vs[j], &line[n*SCREEN_WIDTH], SCREEN_WIDTH+1);
    j--;
    k1++;
  }
  for(int n = nn*SCREEN_WIDTH, j=m; n <= len && j<SCREEN_HEIGHT; n+=SCREEN_WIDTH) {
    if(n==0) lidx[j] = 0;
    strlcpy(vs[j], &line[n], SCREEN_WIDTH+1);
    j++;
  }
  if(is_insert_mode && bee->ins_buf.len > 1){
    int first_line_len = bee->bx + bee->ins_buf.p[0].len;
    char* first_line = malloc(first_line_len+1);
    memcpy(first_line, bee->buf.p[bee->y].p, bee->bx);
    memcpy(first_line, bee->ins_buf.p[0].p, bee->ins_buf.p[0].len);
    first_line[first_line_len] = '\0';

    char *curr_line;
    for(int j = m-1-k1, jj=bee->ins_buf.len-2; j>=0 && jj >=0; jj--){
      curr_line = jj == 0 ? first_line : bee->ins_buf.p[jj].p;
      int len = jj == 0 ? first_line_len : bee->ins_buf.p[jj].len;
      for(int n=len/SCREEN_WIDTH; n>=0 && j>=0; n--){
        strlcpy(vs[j], &curr_line[n*SCREEN_WIDTH], SCREEN_WIDTH+1);
        j--;
        k1++;
      }
      if(j+1>=0) lidx[j+1] = bee->y-jj;
    }
    free(first_line);
  }
  if(is_insert_mode){
    free(line);
  }

  // map bottom half of the screen
  int bee_y = is_insert_mode ? bee->y : bee->y; // TODO: we do not need bee->ins_y
  for(int j=m+1+k2, jj=bee_y+1; j<SCREEN_HEIGHT && jj<bee->buf.len; jj++){
    lidx[j] = jj-bee_y;

    for(int n = 0; n <= bee->buf.p[jj].len && j<SCREEN_HEIGHT; n+=SCREEN_WIDTH) {
      strlcpy(vs[j], &bee->buf.p[jj].p[n], SCREEN_WIDTH+1);
      j++;
    }
  }

  // map top half of the screen
  for(int j = m-1-k1, jj=bee_y-1; j>=0 && jj >=0; jj--){
    // number of screen lines -1 in file line
    int n = bee->buf.p[jj].len / SCREEN_WIDTH; 
    for(; n>=0 && j>=0; n--){
      strlcpy(vs[j], &bee->buf.p[jj].p[n*SCREEN_WIDTH], SCREEN_WIDTH+1);
      j--;
    }
    if(j+1>=0) lidx[j+1] = bee_y-jj;
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
  int x;
  // TODO: should be all visual
  if(!is_insert_mode){
    x = bee->vx;
  } else if (bee->ins_buf.len == 1){
    x = bee->ins_buf.p[bee->ins_buf.len-1].len + bee->vx;
  } else {
    x = bee->ins_buf.p[bee->ins_buf.len-1].len;
  }
  tb_set_cursor(( x % SCREEN_WIDTH) + MARGIN_LEN, m);
  tb_present();
}

