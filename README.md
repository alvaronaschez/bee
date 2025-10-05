# Bee

A Vi-like text editor

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
- Ctrl+w - save file
- Ctrl+q - exit Bee
- u - undo
- Ctrl+r - redo

### INSERT MODE

- Esc - to normal mode

