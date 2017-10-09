#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "page.h"

#define P page_print(page);

int main() {

    Page *page = page_create();

    const char* line = "Hello world! What's up ?";
    size_t len = strlen(line);

    for (size_t i = 0; i < len; ++i)
        page_insert_letter(page, line[i]);

    page_move_cursor_left(page, 5); P
    page_delete_letter(page); P
    page_insert_letter(page, 'd'); P
    page_insert_line(page); P
    page_insert_letter(page, 'd'); P
    page_move_cursor_down(page, 1);
    page_insert_letter(page, 'd'); P
    page_insert_letter(page, 'd'); P
    page_insert_letter(page, 'd'); P
    page_insert_line(page); P
    page_insert_line(page); P
    page_insert_line(page);
    page_insert_line(page);
    page_insert_line(page);
    page_insert_line(page);
    page_insert_line(page);
    page_insert_letter(page, 'd'); P

    page_delete(page);
    return 0;
}
