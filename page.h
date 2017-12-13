#pragma once

#include "line.h"

typedef struct {
	int max_rows;
    int row_off;
    int col;
    int row;
    Line *lines;
    int num_lines;
} Page;

int page_init(Page *page, int max_rows);
void page_delete(Page *page);

void page_insert_line_at(Page *page, int at);
void page_append_line(Page *page, const char *buf, int len);

void page_delete_line_at(Page *page, int at);

void page_insert_letter(Page *page, char c);
void page_delete_letter(Page *page);

void page_move_cursor_left(Page *page);
void page_move_cursor_right(Page *page);


void page_scroll(Page *page);
void page_move_cursor_up(Page *page);
void page_move_cursor_down(Page *page);

void page_move_cursor_end(Page *page);
void page_move_cursor_home(Page *page);
