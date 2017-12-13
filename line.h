#pragma once

#include <stdlib.h>

typedef char Letter;

typedef struct {
    int size;
    Letter* letters; // null terminated (not counted in size) **************************
} Line;

void line_print(const Line *line);
void line_init(Line *line);
void line_delete(Line *line);

void line_insert_letter_at(Line *line, int at, Letter l);
void line_delete_letter_at(Line *line, int at);

void line_append_str(Line *line, const char* buf, int buflen);
