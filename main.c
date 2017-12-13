#include "editor.h"

#include "page.h"
#define P page_print(page);

int main(int argc, char const *argv[])
{
    enable_raw_mode();

    init_editor();
    if (argc >= 2) {
        editor_open(argv[1]);
    }
    while(1)
    {
        editor_refresh_screen();
        editor_process_keypress();
    }
    close_editor();

    return 0;
}
