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

struct bee {
  struct line *buf;
  size_t buf_len;
  unsigned char cursor_x, cursor_y;
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

  tb_init();

  // print file
  for(int i=0; i<tb_height() -1; i++){
    tb_print(0, i, TB_WHITE, TB_BLACK, bee.buf[i].data);
  }
  // footer
  char *footer_format = "\"%s\"  [=%d] L%d C%d";
  tb_printf(0, tb_height() - 1, TB_BLACK, TB_WHITE, footer_format, argv[1], nlines, 0, 0);
  tb_present();

  struct tb_event ev;
  tb_poll_event(&ev);

  tb_shutdown();

  return 0;
}
