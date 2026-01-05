#include "print.h"

#include "bee.h"
#include "termbox2.h"
#include "text_util.h"
#include <stdbool.h>

struct string_list {
  char *str;
  struct string_list *next;
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

int vlen(char *s){
  int vx = 0;
  while(*s != '\0'){
    vx += columnlen(s, vx);
    s += bytelen(s);
  }
  return vx;
}

void print_to_vscreen(const char *s, char **vs, int y_len, int x_len, int y_start){
  int bx, vx, i;
  bx = vx = i = 0;
  while(1){
    int j = vx / x_len;
    if(j+y_start >= y_len) return;

    int vn = columnlen(s+bx, vx);
    int bn = bytelen(s+bx);

    switch(s[bx]){
      case '\t':
        if(j+y_start >= 0)
          vs[j+y_start][i] = ' ';
        bn = vn == 1 ? 1 : 0;
        vn = 1;
        break;
      default:
        if(j+y_start >= 0)
          for(int k=0; k<bn; k++)
            vs[j+y_start][i+k] = s[bx+k];
    }

    if(s[bx] == '\0') return;

    bool is_endline = (vx+1) % x_len == 0;
    if(is_endline){
      if(j+y_start >= 0)
        vs[j+y_start][i+bn] = '\0';
      i = 0;
    } else if(s[bx] == '\t')
      i++;
    else
      i += bn;

    vx += vn;
    bx += bn;
  }
}

void print_screen2(const struct bee *bee) {
  tb_set_clear_attrs(FG_COLOR, BG_COLOR);
  tb_clear();

  char* vs[SCREEN_HEIGHT]; // init virtual screen
  struct {int y; int vx;} vs_to_file_map[SCREEN_HEIGHT];
  for(int j=0; j<SCREEN_HEIGHT; j++){
    vs[j] = malloc(2*SCREEN_WIDTH+1);
    vs[j][0] = '\0';
    vs_to_file_map[j].y = -1; // empty lines
  }

  int m = SCREEN_HEIGHT - SCREEN_HEIGHT/2 -1;
  bool is_insert_mode = bee->mode == INSERT;

  // prepare middle lines
  struct string_list *middle_lines;
  if(!is_insert_mode){
    middle_lines = malloc(sizeof(struct string_list));
    middle_lines->str = bee->buf.p[bee->y];
    middle_lines->next = NULL;
  } else {
    // we create the lines in reverse order
    // bottom upwards
    middle_lines = malloc(sizeof(struct string_list));
    middle_lines->next = NULL;
    middle_lines->str = malloc(
        strlen(bee->ins_buf.p[bee->ins_buf.len-1])
        + strlen(bee->buf.p[bee->y]) - bee->bx
        + 1);
    middle_lines->str[0] = '\0';
    strcat(middle_lines->str, bee->ins_buf.p[bee->ins_buf.len-1]);
    strcat(middle_lines->str, &bee->buf.p[bee->y][bee->bx]);

    struct string_list *tail = middle_lines;
    for(int i=bee->ins_buf.len-2; i>=0; i--){
      tail->next = malloc(sizeof(struct string_list));
      tail = tail->next;
      tail->next = NULL;
      tail->str = bee->ins_buf.p[i];
    }
    if(tail != middle_lines){
      int len = strlen(bee->ins_buf.p[0]);
      tail->str = malloc(len +1);
      tail->str[0] = '\0';
      strcat(tail->str, bee->ins_buf.p[0]);
    }
    tail->str = realloc(tail->str, strlen(tail->str) + bee->bx +1);
    memmove(tail->str + bee->bx, tail->str, strlen(tail->str) +1);
    memcpy(tail->str, bee->buf.p[bee->y], bee->bx);
  }

  // print middle lines
  int y = m - bx_to_vx(bee->bx, 0, bee->buf.p[bee->y])/SCREEN_WIDTH; // first line printed
  int yy = y + vlen(bee->buf.p[bee->y])/SCREEN_WIDTH; // last line printed
  int yyy = 0; // printed real lines, only for printing indexes
  y = yy + 1; // prepare to iterate
  for(struct string_list *line = middle_lines; line; line = line->next){
    char *s = line->str;
    y -= vlen(s)/SCREEN_WIDTH +1;
    print_to_vscreen(s, vs, SCREEN_HEIGHT, SCREEN_WIDTH, y);
    if(y >= 0){
      vs_to_file_map[y].y = yyy++;
    }
    else {
      vs_to_file_map[y].y = -2;
    }
  }

  // cleanup middle_lines
  if(is_insert_mode){
    struct string_list *head, *tail;
    tail = head = middle_lines;
    while(tail->next) // find tail
      tail = tail->next;
    // there are only two strings that we allocated, the ones in head and tail
    free(head->str);
    if(head!=tail)
      free(tail->str);
    while(head){
      struct string_list *aux = head->next;
      free(head);
      head = aux;
    }
  }

  // map above cursor
  for(int j = y, jj=bee->y-1; j>0 && jj >=0; jj--){
    j -= 1 + vlen(bee->buf.p[jj])/SCREEN_WIDTH;
    if(j>=0){
      vs_to_file_map[j].y = (bee->y-1) - jj + yyy;
    }
    print_to_vscreen(bee->buf.p[jj], vs, SCREEN_HEIGHT, SCREEN_WIDTH, j);
  }
  
  // map below cursor
  for(int j = yy+1, jj=bee->y+1; j<SCREEN_HEIGHT && jj<bee->buf.len; jj++){
    print_to_vscreen(bee->buf.p[jj], vs, SCREEN_HEIGHT, SCREEN_WIDTH, j);
    vs_to_file_map[j].y = jj - bee->y;
    j += 1 + vlen(bee->buf.p[jj])/SCREEN_WIDTH;
  }

  // print
  for(int j=0; j<SCREEN_HEIGHT; j++){
    // print empty line outside the document
    if(vs[j][0] == '\0' && vs_to_file_map[j].y == -1){
      for(int i=0; i<tb_width(); i++)
        tb_print(i, j, ALT_FG_COLOR, ALT_BG_COLOR, " ");
      continue;
    }
    // print file content
    tb_print(MARGIN_LEN, j, FG_COLOR, BG_COLOR, vs[j]);
    // print margin
    if(vs_to_file_map[j].y>=0){
      if(vs_to_file_map[j].y==0)
        tb_print(0, j, MARGIN_FG, MARGIN_BG, " 0 ");
      else
        tb_printf(0, j, MARGIN_FG, MARGIN_BG, "%-3d", vs_to_file_map[j].y);
    } else // vs_to_file_map[j].y == -2
      tb_print(0, j, MARGIN_FG, MARGIN_BG, "   ");
  }

  // footer
  print_footer(bee);

  // cursor
  int cursor_col;
  if(!is_insert_mode){
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
  
  // cleanup virtual screen
  for(int j=0; j<SCREEN_HEIGHT; j++){
    free(vs[j]);
  }
}

struct vs_line {
  int y_file, bx_file, vx_file;
  char *p;
};

void print_screen(const struct bee *bee) {

  int m = SCREEN_HEIGHT - SCREEN_HEIGHT/2 -1;

  // find the position (both in the file and screen) of the first line to be printed
  int y_screen, y_file;
  if(bee->mode == INSERT){
    if(bee->ins_buf.len == 1){
      y_screen = m - bx_to_vx(strlen(bee->ins_buf.p[bee->ins_buf.len-1]), bee->vx, bee->ins_buf.p[bee->ins_buf.len-1])/SCREEN_WIDTH;
    } else {
      y_screen = m - bx_to_vx(strlen(bee->ins_buf.p[bee->ins_buf.len-1]), 0, bee->ins_buf.p[bee->ins_buf.len-1])/SCREEN_WIDTH;
    }
    y_file = bee->y + bee->ins_buf.len - 1;
  } else {
    y_screen = m - bx_to_vx(bee->bx, 0, bee->buf.p[bee->y])/SCREEN_WIDTH;
    y_file = bee->y;
  }

  // find the first line of the file to be printed and its offset respect to the screen
  while(y_screen>0 && y_file>0){
    y_file--;
    y_screen -= vlen(bee->buf.p[y_file])/SCREEN_WIDTH+1;
  }

  /* init virtual screen */
  char* vs[SCREEN_HEIGHT]; // init virtual screen
  for(int j=0; j<SCREEN_HEIGHT; j++){
    vs[j] = malloc(2*SCREEN_WIDTH+1);
    vs[j][0] = '\0';
  }

  /* map virtual screen to file*/
  while(y_screen < SCREEN_HEIGHT && y_file < bee->buf.len){
    print_to_vscreen(bee->buf.p[y_file], vs, SCREEN_HEIGHT, SCREEN_WIDTH, y_screen);
    y_screen += vlen(bee->buf.p[y_file])/SCREEN_WIDTH+1;
    y_file ++;
  }

  /* print */
  tb_set_clear_attrs(FG_COLOR, BG_COLOR);
  tb_clear();


  for(int j=0; j<SCREEN_HEIGHT; j++){
    // print file content
    tb_print(MARGIN_LEN, j, FG_COLOR, BG_COLOR, vs[j]);
  }

  tb_set_cursor(0, m);

  tb_present();

  /* cleanup virtual screen */
  for(int j=0; j<SCREEN_HEIGHT; j++){
    free(vs[j]);
  }

}


