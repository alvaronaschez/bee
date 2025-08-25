#define TB_IMPL
#include "termbox2.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct line {
  char *data;
  size_t len;
};

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

struct bee {
  // struct mode mode // TODO
  struct line *buf; // buffer content
  size_t buf_len; // buffer length
  unsigned char cx, cy; // cursor position in the file
  unsigned char cxx; // preferred column / goal column
  unsigned char sx, sy; // screen position, sy is the first line we print
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
};

int main(int argc, char **argv){
  // assert argument count
  if(argc < 2){
    printf("missing file name\naborting\n");
    return 1;
  }

  struct bee bee;

  // read file
  FILE *fp = fopen(argv[1], "r");
  assert(fp);

  int res;
  res = fseek(fp, 0, SEEK_END);
  assert(res == 0);
  long fsize = ftell(fp);
  assert(fsize >= 0);
  rewind(fp);

  char *fcontent = (char*) malloc(fsize * sizeof(char));
  fread(fcontent, 1, fsize, fp);
  res = fclose(fp);
  assert(res == 0);

  // count lines in file
  size_t nlines= 0;
  for(size_t i = 0; i<(size_t)fsize; i++)
    if(fcontent[i] == '\n') nlines++;
  if(fcontent[fsize-1] != '\n')
    nlines++;

  // copy all lines from fcontent into buf
  bee.buf_len = nlines;
  bee.buf = malloc(nlines * sizeof(struct line));
  size_t linelen;
  for(size_t i = 0, j = 0; i<nlines && j<(size_t)fsize; i++){
    linelen = 0;
    while(j+linelen<(size_t)fsize && fcontent[j+linelen]!='\n')
      linelen++;
    // copy line in buffer
    bee.buf[i].data = calloc(j+linelen, sizeof(char));
    if(linelen>0)
      memcpy(bee.buf[i].data, &fcontent[j], linelen);
    bee.buf[i].len = linelen;

    j += linelen+1;
  }

  // start cursor
  bee.cx = bee.cy = 0;

  tb_init();

  struct tb_event ev;
  const char *footer_format = "\"%s\"  [=%d] L%d C%d";
  while(1){
    // print file
    for(int i=0; i<tb_height() -1; i++){
      tb_print(0, i, TB_WHITE, TB_BLACK, bee.buf[i].data);
    }
    // print footer
    tb_printf(0, tb_height() - 1, TB_BLACK, TB_WHITE, 
              footer_format, argv[1], nlines, bee.cy, bee.cx);

    // print cursor
    tb_set_cursor(bee.cx, bee.cy);

    tb_present();

    tb_poll_event(&ev);
    if(ev.ch == 'q')
      break;
    switch(ev.ch){
    case 'h':
      bee.cx--; break;
    case 'j':
      bee.cy++; break;
    case 'k':
      bee.cy--; break;
    case 'l':
      bee.cx++; break;
    }
  }
  tb_shutdown();

  return 0;
}
