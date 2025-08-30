#define TB_IMPL
#include "termbox2.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
** keymaps **

h - move left
j - move down
k - move up
l - move right

C-h - scroll left
C-j - scroll down
C-k - scroll up
C-l - scroll right

A-h - go to beginning of line
A-j - go to last line
A-k - go to first line
A-l - go to end of line
*/

enum mode {
  NORMAL, INSERT, COMMAND
};

char *mode_label[] = {"N", "I", "C"};

struct string {
  char *data;
  int len, cap;
};

/*
  invariant:
  y:
  0 <= toprow <= by < buf_len
  by - toprow < screen_height // the distance between by and toprow must fit into the screen
    where screen_height = tb_height - 1 (because of the footer)
  x:
  0 <= leftcol <= bx < line_len(by)
  bx - leftcol < screen_width // the distance between bx and leftcol must fit into the screen
  leftcol + screen_width +1 <= max(line_len(0..buf_len-1)) // in practice give a little more room

  after moving / updating by:
  if by < 0 => by := 0 (after move up)
  if by >= buf_len => by := buf_len -1 (after move down)
  if by < toprow => toprow := by (after move up)
  if by >= toprow + screen_height => toprow := by - screen_height +1 (after move down)

  after scrolling / updating toprow:
  if toprow < 0 => toprow := 0 (after scroll up)
  if toprow > buf_len => toprow := buf_len -1 (after scroll down)
  if toprow > by => by := toprow (after scroll down)
  if toprow <= by - screen_height => by := toprow + screen_height -1 (after scroll up)
*/
struct bee {
  enum mode mode;
  struct string *buf; // buffer content
  int buf_len; // buffer length
  int bx, by; // cursor position in the file
  int sx, sy; // cursor position in the screen
  int sxx; // preferred column / goal column
  int leftcol, toprow; // scroll offset, toprow is the first line of the file we print
};

int main(int argc, char **argv){
  // assert argument count
  if(argc < 2){
    printf("missing file name\naborting\n");
    return 1;
  }

  struct bee bee;
  bee.mode = NORMAL;
  bee.bx = bee.by = 0;
  bee.sx = bee.sxx = bee.sy = 0;
  bee.leftcol = bee.toprow = 0;

  /* read file */
  {
    FILE *fp = fopen(argv[1], "r");
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
    bee.buf_len = nlines;

    // copy all lines from fcontent into buf
    bee.buf = malloc(nlines * sizeof(struct string));
    int linelen;
    for(int i = 0, j = 0; i<nlines && j<fsize; i++){
      // count line length
      for(linelen = 0; j+linelen<fsize-1 && fcontent[j+linelen]!='\n'; linelen++);
      // copy line in buffer
      bee.buf[i].data = calloc(linelen, sizeof(char));
      if(linelen>0)
        memcpy(bee.buf[i].data, &fcontent[j], linelen);
      bee.buf[i].len = bee.buf[i].cap = linelen;

      j += linelen+1;
    }
    free(fcontent);
  }

  tb_init();

  struct tb_event ev;
  const char *footer_format = "<%s> \"%s\"  [=%d] L%d C%d";
  int screen_height, screen_width;
  while(1){
    tb_clear();

    screen_height = tb_height() - 1;
    screen_width = tb_width();

    // print file
    for(int i=bee.toprow; i< bee.toprow+screen_height && i<bee.buf_len; i++){
      for(int j=0; j<screen_width && j<bee.buf[i].len; j++){
        char c = *(bee.buf[i].data+j+bee.leftcol);
        tb_set_cell(j, i - bee.toprow, c, TB_WHITE, TB_BLACK);
      }
    }
    // print footer
    tb_printf(0, tb_height() - 1, TB_BLACK, TB_WHITE,
              footer_format, mode_label[bee.mode], argv[1], bee.buf_len, bee.by, bee.bx);

    // print cursor
    tb_set_cursor(bee.bx - bee.leftcol, bee.by - bee.toprow);

    tb_present();

    tb_poll_event(&ev);
    if(ev.ch == 'q')
      break;

    switch(ev.ch){
    case 'h':
      bee.bx--;
      if(bee.bx<0) bee.bx = 0;
      if(bee.bx < bee.leftcol) bee.leftcol = bee.bx;
      break;
    case 'j':
      bee.by++;
      if(bee.by>=bee.buf_len) bee.by = bee.buf_len > 0 ? bee.buf_len -1 : 0;
      if(bee.by-bee.toprow >= screen_height) bee.toprow = bee.by - screen_height + 1;
      break;
    case 'k':
      bee.by--;
      if(bee.by < 0) bee.by = 0;
      if(bee.by < bee.toprow) bee.toprow = bee.by;
      break;
    case 'l':
      bee.bx++;
      int line_len = bee.buf[bee.by].len;
      if(bee.bx >= line_len) bee.bx = line_len > 0 ? line_len -1 : 0;
      if(bee.bx - bee.leftcol >= screen_width) bee.leftcol = bee.bx - screen_width +1;
      break;
    case 'x':
      ;
      struct string *s = bee.buf + bee.by;
      if(s->len==0)
        break;
      if(bee.bx < s->len-1)
        memmove(s->data+bee.bx,   // dest
                s->data+bee.bx+1, // src
                s->len-bee.bx-1); // size 
      else
        bee.bx--;
      s->len--;
      break;
    }
  }
  tb_shutdown();

  return 0;
}
