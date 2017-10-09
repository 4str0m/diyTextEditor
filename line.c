#include "line.h"

#include <stdlib.h>
#include <memory.h>
#include <stdio.h>

void line_print(const Line *line)
{
    printf("%zu : ", line->size);
    for(size_t i = 0; i < line->size+1; ++i)
    {
        if (line->letters[i] == '\0') printf("\\0");
        else putchar(line->letters[i]);
    }
    putchar('\n'); fflush(stdout);
}

void line_init(Line *line)
{
    line->size = 0;
    line->letters = calloc(1, sizeof(Letter));
}
void line_delete(Line *line)
{
    free(line->letters);
}

void line_insert_letter_at(Line *line, size_t at, Letter l)
{
    if (at > line->size) at = line->size;

    line->letters = realloc(line->letters, sizeof(Letter) * (line->size + 2));

    memmove(line->letters + at + 1, line->letters + at, line->size - at + 1);
    line->letters[at] = l;
    line->size++;
}
void line_delete_letter_at(Line *line, size_t at)
{
    if (at > line->size - 1) at = line->size - 1;
    memmove(line->letters + at, line->letters + at + 1, line->size - at);
    line->size--;
}

void line_append_str(Line *line, const char* buf, size_t buflen)
{
    line->letters = realloc(line->letters, sizeof(Letter) * (line->size + buflen + 1));

    memcpy(line->letters + line->size, buf, buflen);
    line->size += buflen;
    line->letters[line->size] = '\0';
}
