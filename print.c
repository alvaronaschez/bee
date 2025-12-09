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

// TODO: replace by regular tb_print
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
              bee->cmd_buf);
  } else {
    int buf_len =
        bee->mode == INSERT ? bee->buf.len + bee->y - bee->y : bee->buf.len;
    tb_printf(0, tb_height() - 1, FOOTER_FG, FOOTER_BG, FOOTER_FORMAT, mode,
              bee->filename, buf_len, bee->y, bee->vx);
  }
}

void print_screen_old(const struct bee *bee) {
  tb_clear();
  char vs[SCREEN_HEIGHT][SCREEN_WIDTH+1]; // virtual screen
  int lidx[SCREEN_HEIGHT];

  for(int j=0; j<SCREEN_HEIGHT; j++){
    vs[j][0] = '\0';
    lidx[j] = -1;
  }

  int m = SCREEN_HEIGHT - SCREEN_HEIGHT/2 -1;
  bool is_insert_mode = bee->mode == INSERT;
  //is_insert_mode = false;

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
    len = strlen(bee->buf.p[bee->y]);
    //len = columnlen(bee->buf.p[bee->y].p, 0);
    line = bee->buf.p[bee->y];
  } else if(bee->ins_buf.len==1){
    len = strlen(bee->ins_buf.p[0]) + strlen(bee->buf.p[bee->y]);
    line = malloc(len+1);
    memcpy(line, bee->buf.p[bee->y], bee->bx);
    memcpy(&line[bee->bx], bee->ins_buf.p[0], strlen(bee->ins_buf.p[0]));
    memcpy(&line[strlen(bee->bx+bee->ins_buf.p[0])], 
      &bee->buf.p[bee->y][bee->bx], 
      strlen(bee->buf.p[bee->y]) - bee->bx);
    line[len]='\0';
  } else { // (is_insert_mode && strlen(bee->ins_buf)>1)
    int len1 = strlen(bee->ins_buf.p[bee->ins_buf.len-1]);
    int len2 = strlen(bee->buf.p[bee->y]) - bee->bx;
    len = len1 + len2;
    line = malloc(len+1);
    memcpy(line, bee->ins_buf.p[bee->ins_buf.len-1], len1);
    memcpy(&line[len1], &bee->buf.p[bee->y][bee->bx], len2);
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
    int first_line_len = strlen(bee->bx + bee->ins_buf.p[0]);
    char* first_line = malloc(first_line_len+1);
    memcpy(first_line, bee->buf.p[bee->y], bee->bx);
    memcpy(first_line, bee->ins_buf.p[0], strlen(bee->ins_buf.p[0]));
    first_line[first_line_len] = '\0';

    char *curr_line;
    for(int j = m-1-k1, jj=bee->ins_buf.len-2; j>=0 && jj >=0; jj--){
      curr_line = jj == 0 ? first_line : bee->ins_buf.p[jj];
      int len = jj == 0 ? first_line_len : (int)strlen(bee->ins_buf.p[jj]);
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

    for(int n = 0; n <= (int)strlen(bee->buf.p[jj]) && j<SCREEN_HEIGHT; n+=SCREEN_WIDTH) {
      strlcpy(vs[j], &bee->buf.p[jj][n], SCREEN_WIDTH+1);
      j++;
    }
  }

  // map top half of the screen
  for(int j = m-1-k1, jj=bee_y-1; j>=0 && jj >=0; jj--){
    // number of screen lines -1 in file line
    int n = strlen(bee->buf.p[jj]) / SCREEN_WIDTH; 
    for(; n>=0 && j>=0; n--){
      strlcpy(vs[j], &bee->buf.p[jj][n*SCREEN_WIDTH], SCREEN_WIDTH+1);
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
    x = strlen(bee->ins_buf.p[bee->ins_buf.len-1]) + bee->vx;
  } else {
    x = strlen(bee->ins_buf.p[bee->ins_buf.len-1]);
  }
  tb_set_cursor(( x % SCREEN_WIDTH) + MARGIN_LEN, m);
  tb_present();
}

int vlen(char *s){
  int vx = 0;
  while(*s != '\0'){
    vx += columnlen(s, vx);
    s += bytelen(s);
  }
  return vx;
}

int bx_to_vx(int bx, char* s){
  int vx = 0;
  while(bx > 0){
    vx += columnlen(s, vx);
    int bn = bytelen(s);
    bx -= bn;
    s += bn;
  }
  return vx;
}

void print_to_vscreen(const char *s, char **vscreen, int y_max, int x_max, int y_start){
  int vx = 0;
  for(int j = y_start; j < y_max && *s; j++){
    int i=0;
    for(int vi = 0; vi < x_max && *s;){
      int bn = bytelen(s);
      int vn = columnlen(s, vx);
      if(*s != '\t'){
        if(vn+vi > x_max)
          break;
        vx += vn;
        vi += vn;
        if(j>=0) // print
          for(int k=0; k<bn; k++){
          vscreen[j][i] = *s;
          s++;
        }
        i += bn;
      } else { // (*s == '\t')
        // replace tab with spaces
        if(j>=0)
          vscreen[j][i] = ' ';
        i++;
        vi++;
        vx++;
        if(vn == 1)
          s++;
      }
    }
    vscreen[j][i] = '\0';
  }
}

void print_screen(const struct bee *bee) {
  tb_clear();

  char* vs[SCREEN_HEIGHT]; // init virtual screen
  int lidx[SCREEN_HEIGHT];
  for(int j=0; j<SCREEN_HEIGHT; j++){
    vs[j] = malloc(2*SCREEN_WIDTH+1);
    vs[j][0] = '\0';
    lidx[j] = -1;
  }

  int m = SCREEN_HEIGHT - SCREEN_HEIGHT/2 -1;
  //bool is_insert_mode = bee->mode == INSERT;

  // map cursor line
  char *cursor_line = bee->buf.p[bee->y];
  int cursor_vx = bx_to_vx(bee->bx, cursor_line);
  int cursor_vlen = vlen(cursor_line);
  int y = m - cursor_vx/SCREEN_WIDTH;
  print_to_vscreen(cursor_line, vs, SCREEN_HEIGHT, SCREEN_WIDTH, y);
  if(m >= cursor_vx/SCREEN_WIDTH)
    lidx[m - cursor_vx/SCREEN_WIDTH] = 0;

  // map above cursor
  for(int j = y, jj=bee->y-1; j>0 && jj >=0; jj--){
    j -= 1 + vlen(bee->buf.p[jj])/SCREEN_WIDTH;
    if(j>=0)
      lidx[j] = (bee->y-1) - jj + 1;
    print_to_vscreen(bee->buf.p[jj], vs, SCREEN_HEIGHT, SCREEN_WIDTH, j);
  }
  
  // map below cursor
  for(int j = y+cursor_vlen/SCREEN_WIDTH+1, jj=bee->y+1; j<SCREEN_HEIGHT && jj<bee->buf.len; jj++){
    print_to_vscreen(bee->buf.p[jj], vs, SCREEN_HEIGHT, SCREEN_WIDTH, j);
    lidx[j] = jj - bee->y;
    j += 1 + vlen(bee->buf.p[jj])/SCREEN_WIDTH;
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
  int cursor_col = bx_to_vx(bee->bx, bee->buf.p[bee->y]) % SCREEN_WIDTH;
  tb_set_cursor( cursor_col + MARGIN_LEN, m);

  tb_present();
  
  // cleanup virtual screen
  for(int j=0; j<SCREEN_HEIGHT; j++){
    free(vs[j]);
  }
}
