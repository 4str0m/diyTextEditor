#pragma once

#include "line.h"

typedef struct {
    size_t size;
    size_t col;
    size_t row;
    Line *lines;
    size_t lines_num;
} Page;

Page* page_create();
void page_delete(Page *page);

void page_print(const Page *page);

void page_insert_line_at(Page *page, size_t at, const char *buf, size_t len);
void page_insert_line(Page *page);
void page_delete_line_at(Page *page, size_t at);

void page_insert_letter(Page *page, char c);
void page_delete_letter(Page *page);

void page_move_cursor_left(Page *page, size_t n);
void page_move_cursor_right(Page *page, size_t n);

void page_move_cursor_up(Page *page, size_t n);
void page_move_cursor_down(Page *page, size_t n);
