#include "line.h"

#include <stdlib.h>
#include <memory.h>
#include <stdio.h>

void line_print(const Line *line)
{
    printf("%d : ", line->size);
    for(int i = 0; i < line->size+1; ++i)
    {
        if (line->letters[i] == '\0') printf("\\0");
        else putchar(line->letters[i]);
    }
    putchar('\n'); fflush(stdout);
}

void line_init(Line *line)
{
    line->size = 0;
    line->letters = NULL;
}
void line_delete(Line *line)
{
    free(line->letters);
}

void line_insert_letter_at(Line *line, int at, Letter l)
{
    if (at > line->size) at = line->size;

    line->letters = realloc(line->letters, sizeof(Letter) * (line->size + 2));

    memmove(line->letters + at + 1, line->letters + at, line->size - at + 1);
    line->letters[at] = l;
    line->size++;
}
void line_delete_letter_at(Line *line, int at)
{
    if (at > line->size - 1) at = line->size - 1;
    memmove(line->letters + at, line->letters + at + 1, line->size - at);
    line->size--;
}

void line_append_str(Line *line, const char* buf, int buflen)
{
    line->letters = realloc(line->letters, sizeof(Letter) * (line->size + buflen + 1));

    memcpy(line->letters + line->size, buf, buflen);
    line->size += buflen;
    line->letters[line->size] = '\0';
}
