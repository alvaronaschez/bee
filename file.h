#ifndef FILE_H
#define FILE_H

#include "text.h"

char **load_file(const char *filename, int *len);
void save_file(const struct text *txt, const char *filename);

#endif

