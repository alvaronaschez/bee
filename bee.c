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
  0 <= sy <= cy < buf_len
  cy - xy < screen_height // the distance between cy and xy must fit into the screen
    where screen_height = tb_height - 1 (because of the footer)
  x:
  0 <= sx <= cx < line_len(cy)
  cx - sx < screen_width // the distance between cx and sx must fit into the screen
  sx + screen_width +1 <= max(line_len(0..buf_len-1)) // in practice give a little more room

  after moving / updating cy:
  if cy < 0 => cy := 0 (after move up)
  if cy >= buf_len => cy := buf_len -1 (after move down)
  if cy < sy => sy := cy (after move up)
  if cy >= sy + screen_height => sy := cy - screen_height +1 (after move down)

  after scrolling / updating sy:
  if sy < 0 => sy := 0 (after scroll up)
  if sy > buf_len => sy := buf_len -1 (after scroll down)
  if sy > cy => cy := sy (after scroll down)
  if sy <= cy - screen_height => cy := sy + screen_height -1 (after scroll up)
*/
struct bee {
  enum mode mode;
  struct string *buf; // buffer content
  int buf_len; // buffer length
  int cx, cy; // cursor position in the file
  int cxx; // preferred column / goal column
  int sx, sy; // screen position, sy is the first line of the file we print
};

int main(int argc, char **argv){
  // assert argument count
  if(argc < 2){
    printf("missing file name\naborting\n");
    return 1;
  }

  struct bee bee;
  bee.mode = NORMAL;
  bee.cx = bee.cy = bee.sx = bee.sy = 0;

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
    for(int i=bee.sy; i< bee.sy+screen_height && i<bee.buf_len; i++){
      for(int j=0; j<screen_width && j<bee.buf[i].len; j++)
        tb_set_cell(j, i - bee.sy, *(bee.buf[i].data+j+bee.sx), TB_WHITE, TB_BLACK);
    }
    // print footer
    tb_printf(0, tb_height() - 1, TB_BLACK, TB_WHITE,
              footer_format, mode_label[bee.mode], argv[1], bee.buf_len, bee.cy, bee.cx);

    // print cursor
    tb_set_cursor(bee.cx - bee.sx, bee.cy - bee.sy);

    tb_present();

    tb_poll_event(&ev);
    if(ev.ch == 'q')
      break;

    switch(ev.ch){
    case 'h':
      bee.cx--;
      if(bee.cx<0) bee.cx = 0;
      if(bee.cx < bee.sx) bee.sx = bee.cx;
      break;
    case 'j':
      bee.cy++;
      if(bee.cy>=bee.buf_len) bee.cy = bee.buf_len > 0 ? bee.buf_len -1 : 0;
      if(bee.cy-bee.sy >= screen_height) bee.sy = bee.cy - screen_height + 1;
      break;
    case 'k':
      bee.cy--;
      if(bee.cy < 0) bee.cy = 0;
      if(bee.cy < bee.sy) bee.sy = bee.cy;
      break;
    case 'l':
      bee.cx++;
      int line_len = bee.buf[bee.cy].len;
      if(bee.cx >= line_len) bee.cx = line_len > 0 ? line_len -1 : 0;
      if(bee.cx - bee.sx >= screen_width) bee.sx = bee.cx - screen_width +1;
      break;
    case 'x':
      ;
      struct string *s = bee.buf + bee.cy;
      if(s->len==0)
        break;
      if(bee.cx < s->len-1)
        memmove(s->data+bee.cx,   // dest
                s->data+bee.cx+1, // src
                s->len-bee.cx-1); // size 
      else
        bee.cx--;
      s->len--;
      break;
    }
  }
  tb_shutdown();

  return 0;
}
