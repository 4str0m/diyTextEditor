#include "page.h"

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

Page* page_create()
{
    Page *new_p = calloc(1, sizeof(Page));

    new_p->lines_num = 1;
    new_p->lines = calloc(1, sizeof(Line));
    line_init(new_p->lines);
    return new_p;
}
void page_delete(Page *page)
{
    for(size_t i = 0; i < page->lines_num; ++i)
        line_delete(page->lines + i);
    free(page->lines);
    free(page);
}

void page_print(const Page *page)
{
    printf("%zu [%zu/%zu]\n", page->lines_num, page->col, page->row);
    for (size_t i = 0; i < page->lines_num; ++i)
        line_print(page->lines + i);
}

void page_insert_line_at(Page *page, size_t at, const char *buf, size_t len)
{
    if (at > page->lines_num) at = page->lines_num;

    page->lines = realloc(page->lines, sizeof(Line) * (page->lines_num + 1));
    memmove(page->lines + at + 1, page->lines + at, page->lines_num - at);
    page->lines_num++;

    line_init(page->lines + at);
    line_append_str(page->lines + at, buf, len);
}
void page_delete_line_at(Page *page, size_t at)
{
    if (at > page->lines_num - 1) at = page->lines_num - 1;

    memmove(page->lines + at, page->lines + at + 1, page->lines_num - at - 1);
    page->lines_num--;
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

void page_insert_line(Page *page)
{
    page_insert_line_at(page, page->row + 1, page->lines[page->row].letters + page->col, page->lines[page->row].size - page->col);
    //printf("%zu, %zu\n", page->lines[page->row].size, page->col);
    page->lines[page->row].letters[page->col] = '\0';
    page->lines[page->row].size = page->col;

    page->col = 0;
    ++page->row;
}

void page_move_cursor_left(Page *page, size_t n)
{
    if (n == 0 || (page->col == 0 && page->row == 0)) return;
    else
    {
        if (page->col == 0) page->col = page->lines[--page->row].size;
        else --page->col;
        page_move_cursor_left(page, n - 1);
    }
}
void page_move_cursor_right(Page *page, size_t n)
{
    size_t size = page->lines[page->row].size;
    if (n == 0 || (page->col == size && page->row == page->lines_num-1)) return;
    else
    {
        if (page->col == size)
        {
            page->col = 0;
            ++page->row;
        }
        else ++page->col;
        page_move_cursor_right(page, n - 1);
    }
}

void page_move_cursor_up(Page *page, size_t n)
{
    if (n == 0) return;
    if (page->row == 0) page->col = 0;
    else
    {
        size_t size = page->lines[--page->row].size;
        if (page->col > size) page->col = size;
        page_move_cursor_up(page, n - 1);
    }
}
void page_move_cursor_down(Page *page, size_t n)
{
    if (n == 0) return;
    if (page->row == page->lines_num-1) page->col = page->lines[page->lines_num-1].size;
    else
    {
        size_t size = page->lines[++page->row].size;
        if (page->col > size) page->col = size;
        page_move_cursor_down(page, n - 1);
    }
}
