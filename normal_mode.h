#ifndef NORMAL_MODE_H
#define NORMAL_MODE_H

#include "bee.h"

void normal_read_key(struct bee *bee);

void bee_move_cursor_up(struct bee *bee, int n);
void bee_move_cursor_down(struct bee *bee, int n);
void bee_move_cursor_left(struct bee *bee);
void bee_move_cursor_right(struct bee *bee);

#endif
