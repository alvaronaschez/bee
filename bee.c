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
  int bx, by; // relative to buffer
  int sx, sy; // relative to viewport
  int sxx; // goal/preferred/desired column
  int sxoff, syoff;
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
  bee.sxoff = bee.syoff = 0;

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
    int xoff;
    for(int i=bee.syoff; i< bee.syoff+screen_height && i<bee.buf_len; i++){
      xoff = 0;
      for(int j=0; j<screen_width && j<bee.buf[i].len; j++){
        char c = *(bee.buf[i].data+j+bee.sxoff);
        if(c=='\t'){
          tb_print(j + xoff, i - bee.syoff, TB_WHITE, TB_BLACK, "        ");
          xoff+=7;
        }
        else
          tb_set_cell(j + xoff, i - bee.syoff, c, TB_WHITE, TB_BLACK);
      }
    }
    // print footer
    tb_printf(0, tb_height() - 1, TB_BLACK, TB_WHITE,
              footer_format, mode_label[bee.mode], argv[1], bee.buf_len, bee.by, bee.bx);

    // print cursor
    tb_set_cursor(bee.sx - bee.sxoff, bee.by - bee.syoff);

    tb_present();

    tb_poll_event(&ev);
    if(ev.ch == 'q')
      break;

    switch(ev.ch){
    case 'h':
      bee.bx--;
      if(bee.bx<0) bee.bx = 0;
      if(bee.bx < bee.sxoff) bee.sxoff = bee.bx;
      if(bee.buf[bee.by].data[bee.bx] == '\t')
        bee.sx -= 8;
      else
        bee.sx --;
      break;
    case 'j':
      bee.by++;
      if(bee.by>=bee.buf_len) bee.by = bee.buf_len > 0 ? bee.buf_len -1 : 0;
      if(bee.by-bee.syoff >= screen_height) bee.syoff = bee.by - screen_height + 1;
      break;
    case 'k':
      bee.by--;
      if(bee.by < 0) bee.by = 0;
      if(bee.by < bee.syoff) bee.syoff = bee.by;
      break;
    case 'l':
      if(bee.sxoff+sx+1==slinelen) break;
      if(sx+1==swidth)bee.sxoff++;
      bee.sx++;
      break;
      //bee.bx++;
      //int line_len = bee.buf[bee.by].len;
      //if(bee.bx >= line_len) bee.bx = line_len > 0 ? line_len -1 : 0;
      //if(bee.bx - bee.sxoff >= screen_width) bee.sxoff = bee.bx - screen_width +1;
      //if(bee.bx > 0 && bee.buf[bee.by].data[bee.bx-1] == '\t')
        //bee.sx += 8;
      //else
        //bee.sx++;
      //break;
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
