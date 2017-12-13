#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "page.h"

#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,

    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN,
};

struct editorConfig {
    int screenrows;
    int screencols;

    Page page;
    struct termios orig_termios;
};
struct editorConfig E;

/*** terminal ***/
void die(const char *s)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    
    perror(s);
    exit(1);
}

void disable_raw_mode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

void enable_raw_mode()
{
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");
    atexit(disable_raw_mode);

    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag &= ~(CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

int editor_read_key() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1)
                    return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
        return '\x1b';
    } else {
       return c;
    }
}
int get_cursor_position(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;
    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    return 0;
}

int get_window_size(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;
        return get_cursor_position(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** file i/o ***/
void editor_open(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) die("fopen");
    char *line = NULL;
    size_t linecap = 0;
    int linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' ||
                               line[linelen - 1] == '\r'))
            linelen--;
        page_append_line(&E.page, line, linelen);
    }
    free(line);
    fclose(fp);
}

/*** append buffer ***/
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void ab_append(struct abuf *ab, const char *s, int len)
{
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL)
        return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}
void ab_free(struct abuf *ab) {
    free(ab->b);
}

/*** input ***/
void editor_move_cursor(int key) {
  switch (key) {
    case ARROW_LEFT:
        page_move_cursor_left(&E.page);
        break;
    case ARROW_RIGHT:
        page_move_cursor_right(&E.page);
        break;
    case ARROW_UP:
        page_move_cursor_up(&E.page);
      break;
    case ARROW_DOWN:
        page_move_cursor_down(&E.page);
      break;
  }
}
void editor_process_keypress() {
    int c = editor_read_key();
    switch (c) {
    case ARROW_DOWN: case ARROW_UP: case ARROW_RIGHT: case ARROW_LEFT:
        editor_move_cursor(c);
        break;

    case PAGE_UP: case PAGE_DOWN:
        {
            int times = E.screenrows;
            while (times--)
                editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
        }
        break;

    case HOME_KEY:
        page_move_cursor_home(&E.page);
        break;
    case END_KEY:
        page_move_cursor_end(&E.page);
        break;
    
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    }
}

/*** output ***/
void editor_draw_rows(struct abuf *ab)
{
    for(int y = 0; y < E.screenrows; ++y)
    {
        int file_row = y + E.page.row_off;
        if (file_row >= E.page.num_lines) {
            if (E.page.num_lines == 0 && y == E.screenrows / 3) {
	            char welcome[80];
	            int welcomelen = snprintf(welcome, sizeof(welcome),
	            "Kilo editor -- version %s", KILO_VERSION);
	            if (welcomelen > (int)E.screencols)
	                welcomelen = E.screencols;
	            int padding = (E.screencols - welcomelen) / 2;
	            if (padding) {
	                ab_append(ab, "~", 1);
	                padding--;
	            }
	            while (padding--)
	                ab_append(ab, " ", 1);
	            ab_append(ab, welcome, welcomelen);
	        } else {
	            ab_append(ab, "~", 1);
	        } 
	    } else {
	    	int len = E.page.lines[file_row].size;
	    	if (len > E.screencols) len = E.screencols;
	    	ab_append(ab, E.page.lines[file_row].letters, len);
		}
        ab_append(ab, "\x1b[K", 3);
        if (y < E.screenrows - 1)
        {
            ab_append(ab, "\r\n", 2);
        }
    }
}

void editor_refresh_screen() {
    page_scroll(&E.page);
    struct abuf ab = ABUF_INIT;
    
    ab_append(&ab, "\x1b[?25l", 6);
    ab_append(&ab, "\x1b[H", 3);
    
    editor_draw_rows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.page.row - E.page.row_off + 1, E.page.col + 1);
    ab_append(&ab, buf, strlen(buf));
    
    ab_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    ab_free(&ab);
}

/*** init ***/
void init_editor() {
    E.page.col = E.page.row = 0;
    E.page.num_lines = 0;
    if (get_window_size(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
    page_init(&E.page, E.screenrows);
}

void close_editor() {
    page_delete(&E.page);
}
