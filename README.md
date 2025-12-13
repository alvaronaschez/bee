# Bee

A Vi-like text editor

## Features

- Minimal dependencies (termbox2)

- Around 800 lines of code (not counting termbox2)

- Typewriter scrolling

- Soft wrapping

- Undo/Redo

## How to install Bee?

### Requirements

You need a POSIX system and gcc. Other C compilers should work but haven't been tested.

Run:
``` sh
git clone https://www.github.com/alvaronaschez/bee
cd bee
./do build
```

That should create a `bee` executable.

Try to run `./bee bee.c` to use `bee` to edit bee itself.

## Key bindings

### NORMAL MODE

- h, j, k, l - move left, down, up, right
- x - delete the character under the cursor
- i - insert
- a - append
- u - undo
- Ctrl+r - redo
- : - command mode
- Ctrl+d - move half screen down
- Ctrl+u - move half screen up
- 0 - move to the beginning of the line
- $ - move to end of line

### INSERT MODE

- Esc - to normal mode

### COMMAND MODE

- w - save file
- q - quit
- wq - save file then quit


