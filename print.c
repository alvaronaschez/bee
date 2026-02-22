#include "print.h"

#include "bee.h"
#include "termbox2.h"
#include "text_util.h"
#include "util.h"
#include "string.h"
#include <stdbool.h>

struct vs_line {
  int y_file, bx_file, vx_file;
  char *p;
};


const char *FOOTER_FORMAT = "<%s>  \"%s\"  [=%d]  L%d C%d";

static inline void print_footer(const struct bee *bee) {
  for (int x = 0; x < tb_width(); x++)
    tb_print(x, tb_height() - 1, FOOTER_FG, FOOTER_BG, " ");

  const char *mode = mode_label[bee->mode];
  switch(bee->mode){
    case COMMAND:
      tb_printf(0, tb_height() - 1, FOOTER_FG, FOOTER_BG, "<%s>  %s",
              mode ,bee->cmd_buf);
      break;
    case INSERT:
      ;
      int len = bee->buf.len + bee->ins_buf.len -1;
      int y = bee->y + bee->ins_buf.len -1;
      int cursor_col;
      if(bee->ins_buf.len == 1){
        char *aux = malloc(bee->bx + strlen(bee->ins_buf.p[0]) +1);
        memcpy(aux, bee->buf.p[bee->y], bee->bx);
        aux[bee->bx] = '\0';
        strcat(aux, bee->ins_buf.p[0]);
        cursor_col = bx_to_vx(strlen(aux), 0, aux);
        free(aux);
      } else {
        char *last_insert_line = bee->ins_buf.p[bee->ins_buf.len-1];
        cursor_col = bx_to_vx(strlen(last_insert_line), 0, last_insert_line);
      }
      tb_printf(0, tb_height() - 1, FOOTER_FG, FOOTER_BG, FOOTER_FORMAT, mode,
              bee->filename, len, y, cursor_col);
      break;
    default:
      tb_printf(0, tb_height() - 1, FOOTER_FG, FOOTER_BG, FOOTER_FORMAT, mode,
              bee->filename, bee->buf.len, bee->y, bee->vx);
  }
}

void print_to_vscreen(const char *s, struct vs_line vs[], int y_len, int x_len, int y_start, int y_file){
  int bx, vx, i;
  bx = vx = i = 0;
  while(1){
    int j = vx / x_len;
    if(j+y_start >= y_len) return;

    if(i==0 && j+y_start >= 0){
      vs[j+y_start].bx_file = bx;
      vs[j+y_start].vx_file = vx;
      vs[j+y_start].y_file = y_file;
    }
    
    int vn = columnlen(s+bx, vx);
    int bn = bytelen(s+bx);

    switch(s[bx]){
      case '\t':
        if(j+y_start >= 0)
          vs[j+y_start].p[i] = ' ';
        bn = vn == 1 ? 1 : 0;
        vn = 1;
        break;
      default:
        if(j+y_start >= 0)
          for(int k=0; k<bn; k++)
            vs[j+y_start].p[i+k] = s[bx+k];
    }

    if(s[bx] == '\0') return;

    bool is_endline = (vx+1) % x_len == 0;
    if(is_endline){
      if(j+y_start >= 0)
        vs[j+y_start].p[i+bn] = '\0';
      i = 0;
    } else if(s[bx] == '\t')
      i++;
    else
      i += bn;

    vx += vn;
    bx += bn;
  }
}

void print_screen(const struct bee *bee) {

  int m = SCREEN_HEIGHT - SCREEN_HEIGHT/2 -1;

  /* find the position (both in the file and screen) of the first line to be printed */
  int y_file = bee->y;
  int y_screen;
  if(bee->mode == INSERT){
    if(bee->ins_buf.len == 1){
      y_screen = m - bx_to_vx(strlen(bee->ins_buf.p[bee->ins_buf.len-1]), bee->vx, bee->ins_buf.p[bee->ins_buf.len-1])/SCREEN_WIDTH;
    } else {
      y_screen = m - bx_to_vx(strlen(bee->ins_buf.p[bee->ins_buf.len-1]), 0, bee->ins_buf.p[bee->ins_buf.len-1])/SCREEN_WIDTH;
      for(int i=bee->ins_buf.len-2; i>0; i--){
        y_screen -= vlen(bee->ins_buf.p[i])/SCREEN_WIDTH+1;
      }
      y_screen -= bx_to_vx(strlen(bee->ins_buf.p[0]), bee->vx, bee->ins_buf.p[bee->ins_buf.len-1])/SCREEN_WIDTH+1;
    }
  } else {
    y_screen = m - bx_to_vx(bee->bx, 0, bee->buf.p[bee->y])/SCREEN_WIDTH;
  }

  int yyy = y_file;

  /* find the first line of the file to be printed and its offset respect to the screen */
  while(y_screen>0 && y_file>0){
    y_file--;
    y_screen -= vlen(bee->buf.p[y_file])/SCREEN_WIDTH+1;
  }

  /* init virtual screen */
  //char* vs[SCREEN_HEIGHT]; // init virtual screen
  struct vs_line vs[SCREEN_HEIGHT];
  for(int j=0; j<SCREEN_HEIGHT; j++){
    vs[j].p = malloc(2*SCREEN_WIDTH+1);
    vs[j].p[0] = '\0';
    vs[j].y_file = -1;
  }

  /* map virtual screen to file */
  if(bee->mode == INSERT){
    // before cursor line
    while(y_screen < SCREEN_HEIGHT && y_file < bee->buf.len && y_file < bee->y){
      print_to_vscreen(bee->buf.p[y_file], vs, SCREEN_HEIGHT, SCREEN_WIDTH, y_screen, y_file);
      y_screen += vlen(bee->buf.p[y_file])/SCREEN_WIDTH+1;
      y_file ++;
    }

    // insert lines
    if(bee->ins_buf.len == 1){
      char *begin = str_copy_range(bee->buf.p[y_file], 0, bee->bx);
      char *aux = str_cat3(begin, bee->ins_buf.p[0], &bee->buf.p[y_file][bee->bx]);
      print_to_vscreen(aux, vs, SCREEN_HEIGHT, SCREEN_WIDTH, y_screen, y_file);
      y_screen += vlen(aux)/SCREEN_WIDTH+1;
      y_file ++;
      free(begin);
      free(aux);
    } else if(bee->ins_buf.len > 1) {
      char *begin = str_copy_range(bee->buf.p[y_file], 0, bee->bx);
      char *aux1 = str_cat(begin, bee->ins_buf.p[0]);
      char *aux2 = str_cat(bee->ins_buf.p[bee->ins_buf.len-1], &bee->buf.p[y_file][bee->bx]);
      for(int i=0; i<bee->ins_buf.len; i++){
        if(y_screen >= SCREEN_HEIGHT)
          break;
        char *aux = bee->ins_buf.p[i];
        if(i==0) aux = aux1;
        if(i==bee->ins_buf.len-1) aux = aux2;

        print_to_vscreen(aux, vs, SCREEN_HEIGHT, SCREEN_WIDTH, y_screen, y_file);
        y_screen += vlen(aux)/SCREEN_WIDTH+1;
      }
      y_file ++;
      free(begin);
      free(aux1);
      free(aux2);
    }

    // after insert lines
    while(y_screen < SCREEN_HEIGHT && y_file < bee->buf.len){
      print_to_vscreen(bee->buf.p[y_file], vs, SCREEN_HEIGHT, SCREEN_WIDTH, y_screen, y_file);
      y_screen += vlen(bee->buf.p[y_file])/SCREEN_WIDTH+1;
      y_file ++;
    }
  } else {
    while(y_screen < SCREEN_HEIGHT && y_file < bee->buf.len){
      print_to_vscreen(bee->buf.p[y_file], vs, SCREEN_HEIGHT, SCREEN_WIDTH, y_screen, y_file);
      y_screen += vlen(bee->buf.p[y_file])/SCREEN_WIDTH+1;
      y_file ++;
    }
  }

  /* print */
  tb_set_clear_attrs(FG_COLOR, BG_COLOR);
  tb_clear();


  for(int j=0; j<SCREEN_HEIGHT; j++){
    // print file content
    if(bee->mode == VISUAL){
      int y0, y1, bx0, bx1, vx0, vx1;
      y0 = bee->y0; y1 = bee->y; bx0 = bee->bx0; bx1 = bee->bx; vx0 = bee->vx0; vx1 = bee->vx;
      if(y0>y1 || (y0==y1 && bx0 > bx1)){
        SWAP_INT(y0,y1); SWAP_INT(bx0, bx1); SWAP_INT(vx0, vx1);
      }
      int y = vs[j].y_file;
      if(y0 <= y && y <= y1){
        // TODO
        if(y0 < y && y < y1)
          tb_print(MARGIN_LEN, j, BG_COLOR, FG_COLOR, vs[j].p);
        else if(y0 == y && y == y1){
          tb_print(MARGIN_LEN, j, FG_COLOR, BG_COLOR, vs[j].p);
          tb_print(MARGIN_LEN+bx0, j, BG_COLOR, FG_COLOR, vs[j].p+bx0);
          tb_print(MARGIN_LEN+bx1, j, FG_COLOR, BG_COLOR, vs[j].p+bx1);
        } else if(y0 == y){
          tb_print(MARGIN_LEN, j, FG_COLOR, BG_COLOR, vs[j].p);
          tb_print(MARGIN_LEN+bx0, j, BG_COLOR, FG_COLOR, vs[j].p+bx0);
        } else if(y1 == y){
          tb_print(MARGIN_LEN, j, BG_COLOR, FG_COLOR, vs[j].p);
          tb_print(MARGIN_LEN+bx1, j, FG_COLOR, BG_COLOR, vs[j].p+bx1);
        }
      }
      else
        tb_print(MARGIN_LEN, j, FG_COLOR, BG_COLOR, vs[j].p);
    } else {
      tb_print(MARGIN_LEN, j, FG_COLOR, BG_COLOR, vs[j].p);
    }
    // print margin
    if(vs[j].y_file == bee->y && vs[j].bx_file == 0)
        tb_print(0, j, MARGIN_FG, MARGIN_BG, " 0 ");
    else if(vs[j].y_file >= 0 && vs[j].bx_file==0)
        tb_printf(0, j, MARGIN_FG, MARGIN_BG, "%-3d", ABS(vs[j].y_file - yyy));
    else
      tb_print(0, j, MARGIN_FG, MARGIN_BG, "   ");
  }

  /* footer */
  print_footer(bee);

  /* cursor */
  int cursor_col;
  if(bee->mode != INSERT){
    cursor_col= bx_to_vx(bee->bx, 0, bee->buf.p[bee->y]);
  } else if(bee->ins_buf.len == 1) {
    char *aux = malloc(bee->bx + strlen(bee->ins_buf.p[0]) +1);
    memcpy(aux, bee->buf.p[bee->y], bee->bx);
    aux[bee->bx] = '\0';
    strcat(aux, bee->ins_buf.p[0]);
    cursor_col = bx_to_vx(strlen(aux), 0, aux);
    free(aux);
  } else {
    char *last_insert_line = bee->ins_buf.p[bee->ins_buf.len-1];
    cursor_col = bx_to_vx(strlen(last_insert_line), 0, last_insert_line);
  }
  tb_set_cursor( cursor_col % SCREEN_WIDTH + MARGIN_LEN, m);

  tb_present();

  /* cleanup virtual screen */
  for(int j=0; j<SCREEN_HEIGHT; j++){
    free(vs[j].p);
  }

}

