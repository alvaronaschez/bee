#define TB_IMPL
#include "termbox2.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum mode {
  NORMAL, INSERT, COMMAND
};

char *mode_label[] = {"N", "I", "C"};

struct string {
  char *data;
  int len, cap;
};

struct bee {
  enum mode mode;
  struct string *buf;
  int buf_len;
  int x, y;
  int bx, vx;
  int xoff, yoff;
};

int main(int argc, char **argv){
  // assert argument count
  if(argc < 2){
    printf("missing file name\naborting\n");
    return 1;
  }

  struct bee bee;
  bee.mode = NORMAL;
  bee.x = bee.y = 0;
  bee.xoff = bee.yoff = 0;
  bee.bx = bee.vx = 0;

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
  const int tablen = 8;
  const	int footerheight = 1;
#define screen_height (tb_height() - footerheight)
#define screen_width (tb_width())
  while(1){
    tb_clear();

    // print file
    for(int j=bee.yoff; j< bee.yoff+screen_height && j<bee.buf_len; j++){
      // i points the buffer, si points the screen (including the non-visible part)
      for(int i=0, vi=0; i<bee.buf[j].len && vi<bee.xoff+screen_width; i++){
        char c = *(bee.buf[j].data+i+bee.xoff);
        // sync x, vx, bx
        if(j-bee.yoff == bee.y && i == bee.x) bee.vx = vi;

        // print char
        if(vi >= bee.xoff){
          if(c=='\t')
            tb_print(vi, j - bee.yoff, TB_WHITE, TB_BLACK, "        ");
          else
            tb_set_cell(vi, j - bee.yoff, c, TB_WHITE, TB_BLACK);
        }

        // advance screen pointer
        vi += (c=='\t' ? tablen : 1);
      }
    }
    // print footer
    tb_printf(0, tb_height() - 1, TB_BLACK, TB_WHITE,
              footer_format, mode_label[bee.mode], argv[1],
              bee.buf_len, bee.yoff + bee.y, bee.xoff + bee.x);

    // print cursor
    tb_set_cursor(bee.vx, bee.y);

    tb_present();

    tb_poll_event(&ev);
    if(ev.ch == 'q')
      break;

    switch(ev.ch){
    case 'h':
      if(bee.x > 0)
        bee.x--;
      else if(bee.xoff > 0)
        bee.xoff--;
      break;
    case 'j':
      if(bee.y+1<screen_height)
        bee.y++;
      else if(bee.yoff + bee.y +1 < bee.buf_len)
        bee.yoff++;
      break;
    case 'k':
      if(bee.y>0)
        bee.y--;
      else {
        if(bee.yoff>0)
          bee.yoff--;
      }
      break;
    case 'l':
      if(bee.vx < screen_width && bee.x+1<bee.buf[bee.yoff+bee.y].len)
        bee.x++;
      break;
    }
  }
  tb_shutdown();

  return 0;
}
