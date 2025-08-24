#define TB_IMPL
#include "termbox2.h"

int main(void){
  int y = 0;
  struct tb_event ev;

  tb_init();

  tb_printf(0, y++, TB_GREEN, TB_BLACK, "hello from termbox");
  tb_printf(0, y++, TB_WHITE, TB_BLACK, "hello from termbox");
  tb_printf(0, y++, TB_RED, TB_BLACK, "hello from termbox");
  tb_printf(0, y++, TB_YELLOW, TB_BLACK, "hello from termbox");
  tb_printf(0, y++, TB_BLUE, TB_BLACK, "hello from termbox");
  tb_printf(0, y++, TB_MAGENTA, TB_BLACK, "hello from termbox");
  tb_printf(0, y++, TB_BLACK, TB_WHITE, "hello from termbox");
  tb_printf(0, y++, TB_BLACK, TB_BLACK, "");
  tb_printf(0, y++, TB_CYAN, TB_BLACK, "(press any key to quit)");
  tb_present();
  tb_poll_event(&ev);

  tb_shutdown();


  return 0;
}
