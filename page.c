#include "page.h"

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

int page_init(Page *page, int max_rows)
{
    page->col = page->row = 0;
    page->num_lines = 0;
    page->row_off = 0;
    page->max_rows = max_rows;
    page->lines = NULL;
    /*
    page->num_lines = 1;
    page->lines = calloc(1, sizeof(Line));
    line_init(page->lines);
    */
    return 0;
}
void page_delete(Page *page)
{
    for(int i = 0; i < page->num_lines; ++i)
        line_delete(page->lines + i);
    free(page->lines);
}

void page_delete_line_at(Page *page, int at)
{
    if (at > page->num_lines - 1) at = page->num_lines - 1;

    memmove(page->lines + at, page->lines + at + 1, page->num_lines - at - 1);
    page->num_lines--;
}

void page_insert_line_at(Page *page, int at)
{
    if (at > page->num_lines) at = page->num_lines;

    page->lines = realloc(page->lines, sizeof(Line) * (page->num_lines + 1));
    memmove(page->lines + at + 1, page->lines + at, page->num_lines - at);
    page->num_lines++;

    line_init(page->lines + at);
}
void page_append_line(Page *page, const char *buf, int len)
{
    page_insert_line_at(page, page->num_lines);
    line_append_str(page->lines + page->num_lines - 1, buf, len);
}

void page_insert_letter(Page *page, Letter l)
{
    line_insert_letter_at(page->lines + page->row, page->col++, l);
}
void page_delete_letter(Page *page)
{
    if (page->col == 0) return;
    line_delete_letter_at(page->lines + page->row, --page->col);
}

void page_move_cursor_left(Page *page)
{
    if (page->col == 0 && page->row == 0) return;
    else
    {
        if (page->col == 0) page->col = page->lines[--page->row].size;
        else --page->col;
    }
}
void page_move_cursor_right(Page *page)
{
    int size = page->lines[page->row].size;
    if (page->col == size && page->row == page->num_lines-1) return;
    else
    {
        if (page->col == size)
        {
            page->col = 0;
            ++page->row;
        }
        else ++page->col;
    }
}

void page_move_cursor_up(Page *page)
{
    if (page->row == 0) page->col = 0;
    else
    {
        int size = page->lines[--page->row].size;
        if (page->col > size) page->col = size;
    }
}
void page_move_cursor_down(Page *page)
{
    if (page->row == page->num_lines-1)
        page->col = page->lines[page->num_lines-1].size;
    else
    {
        int size = page->lines[++page->row].size;
        if (page->col > size) page->col = size;
    }
}

void page_move_cursor_end(Page *page)
{
    page->col = page->lines[page->row].size;
}
void page_move_cursor_home(Page *page)
{
    page->col = 0;
}

void page_scroll(Page *page) {
    if (page->row < page->row_off)
        page->row_off = page->row;
    if (page->row >= page->row_off + page->max_rows)
        page->row_off = page->row - page->max_rows + 1;
}
