/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <time.h>
#include <math.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 4
#define KILO_QUIT_TIMES 3
#define KILO_LINE_NUM_SEP ": "

#define CTRL_KEY(k) ((k) & 0x1f)

#define HL_HIGHLIGHT_NUMBERS      (1<<0)
#define HL_HIGHLIGHT_STRINGS      (1<<1)
#define HL_HIGHLIGHT_FUNCTIONS    (2<<1)

enum editorKey {
  CTRL_ENTER = 30,
  CTRL_BACKSPACE,
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN,
  CTRL_SHIFT_ENTER,
  CTRL_SHIFT_D,
  CTRL_ARROW_LEFT,
  CTRL_ARROW_RIGHT,
  CTRL_ARROW_UP,
  CTRL_ARROW_DOWN,
  CTRL_SHIFT_ARROW_LEFT,
  CTRL_SHIFT_ARROW_RIGHT,
  CTRL_SHIFT_ARROW_UP,
  CTRL_SHIFT_ARROW_DOWN,
  CTRL_DELETE
};

enum editorHighlight {
  HL_NORMAL = 0,
  HL_NUMBER,
  HL_STRING,
  HL_MATCH,
  HL_COMMENT,
  HL_MLCOMMENT,
  HL_KEYWORD1,
  HL_KEYWORD2,
  HL_FUNCTION,
};

/*** data ***/

struct editorSyntax {
  char* filetype;
  char** filematch;
  char** keywords;
  char* single_line_comment_start;
  char* multi_line_comment_start;
  char* multi_line_comment_end;
  int flags;
};

typedef struct erow {
  int idx;
  int size;
  int rsize;
  char *chars;
  char *render;
  unsigned char *hl;
  int hl_open_comment;
} erow;

struct editorConfig {
  int cx, cy;
  int rx;
  int rowoff;
  int coloff;
  int screenrows;
  int screencols;
  int numrows;
  int rowborder_width;
  erow *row;
  int dirty;
  char* filename;
  char statusmsg[80];
  time_t statusmsg_time;
  struct editorSyntax *syntax;
  struct termios orig_termios;
};

struct editorConfig E;

/*** filetypes ***/

char *C_HL_extensions[] = {".c", ".h", ".cpp", NULL};
char *C_HL_keywords[] = {
  "switch", "if", "while", "for", "break", "continue", "return", "else",
  "struct", "union", "typedef", "static", "enum", "class", "case", "include", "define",

  "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
  "void|", NULL
};

struct editorSyntax HLDB[] =
{
  {
    "C",
    C_HL_extensions,
    C_HL_keywords,
    "//", "/*", "*/",
    HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_FUNCTIONS
  },
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/*** prototypes ***/
void editorSetStatusMessage(const char *fmt, ...);
void editorMoveCursor(int key);
void editorRefreshScreen();
char* editorPrompt(char *prompt, void (*callback)(char *, int));

/*** terminal ***/

void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

int editorReadKey() {
  int nread;
  unsigned char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }

  if (c == '\x1b') {
    char seq[5];

    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
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
        } else if (seq[2] == ';') {
          if (read(STDIN_FILENO, &seq[3], 1) != 1) return '\x1b';
          if (read(STDIN_FILENO, &seq[4], 1) != 1) return '\x1b';
          if (seq[1] == '1') {
            if (seq[3] == '5') {
              switch(seq[4]) {
                case 'D': return CTRL_ARROW_LEFT;
                case 'C': return CTRL_ARROW_RIGHT;
                case 'A': return CTRL_ARROW_UP;
                case 'B': return CTRL_ARROW_DOWN;
              }
            } else if (seq[3] == '6') {
              switch (seq[4]) {
                case 'D': return CTRL_SHIFT_ARROW_LEFT;
                case 'C': return CTRL_SHIFT_ARROW_RIGHT;
                case 'A': return CTRL_SHIFT_ARROW_UP;
                case 'B': return CTRL_SHIFT_ARROW_DOWN;  
              }
            }
          } else if (seq[1] == '3') {
            if (seq[3] == '5' && seq[4] == '~') return CTRL_DELETE;
            return '\x1b';
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
  } else if (c == 194) {
    if (read(STDIN_FILENO, &c, 1) != 1) return '\x1b';
    switch (c) {
      case (158): return CTRL_SHIFT_ENTER;
      case (132): return CTRL_SHIFT_D;
      default: return '\x1b';
    }
  } else {
    return (char)c;
  }
}

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** syntax highlighting ***/

int is_separator(char c) {
  return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];#", c) != NULL;
}

void editorUpdateSyntax(erow *row) {
  row->hl = realloc(row->hl, row->rsize);
  memset(row->hl, HL_NORMAL, row->rsize);

  if (E.syntax == NULL) return;

  char** keywords = E.syntax->keywords;

  char *scs = E.syntax->single_line_comment_start;
  char *mcs = E.syntax->multi_line_comment_start;
  char *mce = E.syntax->multi_line_comment_end;

  int scs_len = scs ? strlen(scs) : 0;
  int mcs_len = mcs ? strlen(mcs) : 0;
  int mce_len = mce ? strlen(mce) : 0;

  int prev_sep = 1;
  int in_str = 0;
  int in_comment = (row->idx > 0 && E.row[row->idx-1].hl_open_comment);
  int i = 0;
  while (i < row->rsize) {
    char c = row->render[i];
    unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

    if (scs_len && !in_str && !in_comment) {
      if (!strncmp(scs, row->render + i, scs_len)) {
        memset(row->hl + i, HL_COMMENT, row->rsize - i);
        break;
      }
    }

    if (mcs_len && mce_len && !in_str) {
      if (in_comment) {
        row->hl[i] = HL_MLCOMMENT;
        if (!strncmp(row->render + i, mce, mce_len)) {
          memset(row->hl+i, HL_MLCOMMENT, mce_len);
          i += mce_len;
          in_comment = 0;
          prev_sep = 1;
          continue;
        } else {
          i++;
          continue;
        }
      } else if (!strncmp(row->render + i, mcs, mcs_len)) {
          memset(row->hl+i, HL_MLCOMMENT, mcs_len);
          i += mcs_len;
          in_comment = 1;
          continue;
      }
    }

    if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
      if (in_str) {
        row->hl[i] = HL_STRING;
        if (c == '\\' && i+1 < row->rsize) {
          row->hl[i+1] = HL_STRING;
          i += 2;
          continue;
        }
        if (c == in_str) in_str = 0;
        i++;
        prev_sep = 1;
        continue;
      } else {
        if (c == '"' || c == '\'') {
          in_str = c;
          row->hl[i] = HL_STRING;
          i++;
          continue;
        } 
      }
    }

    if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
      if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
        (c == '.' && prev_hl == HL_NUMBER)) {
        row->hl[i] = HL_NUMBER;
        i++;
        prev_sep = 0;
        continue;
      }
    }

    if (prev_sep) {
      char **keyword;
      for(keyword = keywords; *keyword; keyword++) {
        int klen = strlen(*keyword);
        int kw2 = (*keyword)[klen-1] == '|';
        if (kw2) klen--;

        if (!strncmp(row->render + i, *keyword, klen) &&
          is_separator(row->render[i+klen])) {
          memset(row->hl + i, kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
          i += klen;
          break;
        }
      }
      if (*keyword != NULL) {
        prev_sep = 0;
        continue;
      } else if (E.syntax->flags & HL_HIGHLIGHT_FUNCTIONS &&
                  i+1 < row->rsize) {
        char* next_space = strchr(row->render + i + 1, ' ');
        char* next_paren = strchr(row->render + i + 1, '(');

        if (next_paren && (!next_space || next_space - next_paren > 0)) {
          memset(row->hl + i, HL_FUNCTION, next_paren - row->render - i);
          i += next_paren - row->render - i;
          prev_sep = 0;
          continue;
        }
      }
    }

    prev_sep = is_separator(c);
    ++i;
  }

  int changed = (row->hl_open_comment != in_comment);
  row->hl_open_comment = in_comment;
  if (changed && row->idx+1 < E.numrows)
    editorUpdateSyntax(E.row + row->idx + 1);

}

int editorSyntaxToColor(int hl) {
  switch(hl) {
    case HL_NUMBER: return 35;
    case HL_MATCH: return 34;
    case HL_STRING: return 33;
    case HL_MLCOMMENT:
    case HL_COMMENT: return 35;
    case HL_KEYWORD1: return 31;
    case HL_KEYWORD2: return 36;
    case HL_FUNCTION: return 32;
    default: return 37;
  }
}

void editorSelectSyntaxHighlight() {
  E.syntax = NULL;
  if (E.filename == NULL) return;

  char *ext = strrchr(E.filename, '.');

  for (unsigned int i = 0; i < HLDB_ENTRIES; ++i) {
    char** filematch = HLDB[i].filematch;
    while (*filematch) {
      int is_ext = (*filematch)[0] == '.';
      if ((is_ext && !strcmp(ext, *filematch)) ||
        (!is_ext && strstr(E.filename, *filematch))) {
        E.syntax = HLDB + i;

        for (int i = 0; i < E.numrows; ++i) {
          editorUpdateSyntax(E.row + i);
        }
        return;
      }
      filematch++;
    }
  }
}

/*** row operations ***/

int editorRowCxToRx(erow *row, int cx) {
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    if (row->chars[j] == '\t')
      rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
    rx++;
  }
  return rx;
}
int editorRowRxToCx(erow *row, int rx) {
  int cx = 0;
  int j = 0;
  int i;
  for (i = 0, j = 0; i < rx; i++, j++) {
    if (row->chars[j] == '\t') {
      i += (KILO_TAB_STOP - 1) - (i % KILO_TAB_STOP);
    }
    cx++;
  }
  return cx;
}

void editorUpdateRow(erow *row) {
  int tabs = 0;
  int j;
  for (j = 0; j < row->size; j++)
    if (row->chars[j] == '\t') tabs++;

  free(row->render);
  row->render = malloc(row->size + tabs*(KILO_TAB_STOP - 1) + 1);

  int idx = 0;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % KILO_TAB_STOP != 0) row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;

  editorUpdateSyntax(row);
}

int editorInsertRow(int at, char *s, size_t len, int auto_indent) {
  if (at < 0 || at > E.numrows) return 0;


  int indentlen = 0;
  int indentsize = at > 0 ? E.row[at-1].size+1 : 0;
  char indent_buf[indentsize];
  if (auto_indent && indentsize) {
    while (indentlen < indentsize && isspace(E.row[at-1].chars[indentlen])) {
      indent_buf[indentlen] = E.row[at-1].chars[indentlen];
      indentlen++;
    }
    indent_buf[indentlen] = '\0';
  }

  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  memmove(E.row + at + 1, E.row + at, sizeof(erow) * (E.numrows - at));
  for (int i = at + 1; i <= E.numrows; ++i) E.row[i].idx++;
  E.row[at].idx = at;

  E.row[at].size = len + indentlen;
  E.row[at].chars = malloc(len + indentlen + 1);
  if (auto_indent)
    memcpy(E.row[at].chars, indent_buf, indentlen);
  memcpy(E.row[at].chars + indentlen, s, len);
  E.row[at].chars[len + indentlen] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  E.row[at].hl = NULL;
  E.row[at].hl_open_comment = 0;
  editorUpdateRow(&E.row[at]);

  E.numrows++;
  E.dirty++;
  return indentlen;
}

void editorFreeRow(erow *row) {
  free(row->render);
  free(row->chars);
  free(row->hl);
}

void editorDelRow(int at) {
  if (at < 0 || at >= E.numrows) return;
  editorFreeRow(E.row + at);
  memmove(E.row + at, E.row + at + 1, sizeof(erow) * (E.numrows - at - 1));
  for (int i = at; i < E.numrows - 1; ++i) E.row[i].idx--;
  E.numrows--;
  E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size) at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(row->chars + at + 1, row->chars + at, row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowAppendString(erow *row, char* s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(row->chars + row->size, s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowDelChar(erow *row, int at) {
  if (at < 0 || at >= row->size) return;
  memmove(row->chars + at, row->chars + at + 1, row->size - at);
  row->size--;
  editorUpdateRow(row);
  E.dirty++;
}

void editorMoveRowUp(int at) {
  if (at <= 0 || at >= E.numrows) return;
  E.row[at].idx--;
  E.row[at-1].idx++;
  erow temp = E.row[at-1];
  E.row[at-1] = E.row[at];
  E.row[at] = temp;
  E.dirty++;
}

void editorMoveRowDown(int at) {
  if (at < 0 || at >= E.numrows-1) return;
  E.row[at].idx++;
  E.row[at+1].idx--;
  erow temp = E.row[at+1];
  E.row[at+1] = E.row[at];
  E.row[at] = temp;
  E.dirty++;
}


/*** editor operation ***/

void editorInsertChar(int c) {
  if (E.cy == E.numrows) {
    editorInsertRow(E.numrows, "", 0, 0);
  }
  editorRowInsertChar(E.row + E.cy, E.cx, c);
  E.cx++;
}

void editorInsertNewLine() {
  if(E.cx == 0) {
    editorInsertRow(E.cy, "", 0, 1);
    E.cy++;
  } else {
    erow *row = E.row+E.cy;
    int prev_indented = editorInsertRow(E.cy+1, row->chars + E.cx, row->size - E.cx, 1);
    row = E.row+E.cy;
    row->size = E.cx;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.cy++;
    E.cx = 0;
    if (prev_indented)
      editorMoveCursor(CTRL_ARROW_RIGHT);
  }
}

void editorDelChar() {
  if (E.cy == E.numrows) {
    editorMoveCursor(ARROW_LEFT);
    return;
  }
  if (E.cx == 0 && E.cy == 0) return;

  erow *row = E.row + E.cy;
  if (E.cx > 0) {
    editorRowDelChar(row, E.cx - 1);
    E.cx--;
  } else {
    E.cx = E.row[E.cy - 1].size;
    editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
    editorDelRow(E.cy);
    E.cy--;
  }
}

void editorDelWord() {
  if (E.cx == 0 || E.cy == E.numrows) {
    editorDelChar();
  }
  else {
    int curr_cx = E.cx;
    editorMoveCursor(CTRL_ARROW_LEFT);
    int target_cx = E.cx;
    E.cx = curr_cx;
    for (int i = 0; i < curr_cx - target_cx; ++i)
    {
      editorDelChar();
    }
  }
}

void editorMoveLineUp() {
  editorMoveRowUp(E.cy);
  editorMoveCursor(ARROW_UP);
}

void editorMoveLineDown() {
  editorMoveRowDown(E.cy);
  editorMoveCursor(ARROW_DOWN);
}

void editorDelLine() {
  if (E.cy == E.numrows) return;
  editorMoveCursor(ARROW_DOWN);
  editorDelRow(E.cy-1);
  editorMoveCursor(ARROW_UP);
}

void editorDuplicateLine() {
  if (E.cy == E.numrows) return;
  int curr_cx = E.cx;
  E.cx = 0;
  editorInsertNewLine();
  E.cx = curr_cx;
  editorRowAppendString(E.row + E.cy - 1, E.row[E.cy].chars, E.row[E.cy].size);
}

/*** file i/o ***/

char* editorRowsToString(int *buflen) {
  int totlen = 0;
  for (int i = 0; i < E.numrows; ++i)
    totlen += E.row[i].size + 1; // waring: \n char
  *buflen = totlen;

  char *buf = malloc(totlen);
  char *p = buf;
  for (int i = 0; i < E.numrows; ++i)
  {
    memcpy(p, E.row[i].chars, E.row[i].size);
    p += E.row[i].size;
    *p = '\n';
    p++;
  }
  return buf;
}

void editorOpen(char *filename) {
  free(E.filename);
  E.filename = strdup(filename);

  editorSelectSyntaxHighlight();

  FILE *fp = fopen(filename, "r");
  if (!fp) die("fopen");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' ||
                           line[linelen - 1] == '\r'))
      linelen--;
    editorInsertRow(E.numrows, line, linelen, 0);
  }
  free(line);
  fclose(fp);
  E.dirty = 0;
}

void editorSave() {
  if (E.filename == NULL) {
    E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
    if (E.filename == NULL) {
      editorSetStatusMessage("Save aborted");
      return;
    }
    editorSelectSyntaxHighlight();
  }

  int len;
  char *buf = editorRowsToString(&len);

  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        E.dirty = 0;
        editorSetStatusMessage("%d bytes written to disk", len);
        return;
      }
    }
    close(fd);
  }

  free(buf);
  editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

/*** find ***/

void editorFindCallback(char* query, int key) {
  static int last_match = -1;
  static int direction = 1;

  static int saved_hl_line;
  static unsigned char *saved_hl = NULL;
  if (saved_hl) {
    memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
    free(saved_hl);
    saved_hl = NULL;
  }

  if (key == '\r' || key == '\x1b') {
    last_match = -1;
    direction = 1;
    return;
  } else if (key == ARROW_DOWN || key == ARROW_RIGHT) {
    direction = 1;
  } else if (key == ARROW_UP || key == ARROW_LEFT) {
    direction = -1;
  } else {
    last_match = -1;
    direction = 1;
  }

  if (last_match == -1) direction = 1;
  int current = last_match;
  int i;
  for (i = 0; i < E.numrows; i++) {
    current += direction;
    if (current == -1) current = E.numrows-1;
    else if (current == E.numrows) current = 0;

    erow *row = &E.row[current];
    char *match = strstr(row->render, query);
    if (match) {
      last_match = current;
      E.cy = current;
      E.cx = editorRowRxToCx(row, match - row->render);
      E.rowoff = E.numrows;

      saved_hl_line = current;
      saved_hl = malloc(row->rsize);
      memcpy(saved_hl, row->hl, row->rsize);
      memset(row->hl + (match - row->render), HL_MATCH, strlen(query));
      break;
    }
  }
}

void editorFind() {
  int saved_cx = E.cx;
  int saved_cy = E.cy;
  int saved_coloff = E.coloff;
  int saved_rowoff = E.rowoff;

  char *query = editorPrompt("Search: %s (Use ESC/ARROW/ENTER)", editorFindCallback);

  if (query) {
    free(query);
  } else {
    E.cx = saved_cx;
    E.cy = saved_cy;
    E.coloff = saved_coloff;
    E.rowoff = saved_rowoff;
  }
}

/*** append buffer ***/

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}

/*** output ***/

void editorScroll() {
  E.rx = 0;
  if (E.cy < E.numrows) {
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
  }
  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows) {
    E.rowoff = E.cy - E.screenrows + 1;
  }
  if (E.rx < E.coloff) {
    E.coloff = E.rx;
  }
  if (E.rx >= E.coloff + E.screencols) {
    E.coloff = E.rx - E.screencols + 1;
  }
}

void editorDrawRows(struct abuf *ab) {
  int y;
  // add line number
  int digitnum = (int)ceil(log10(E.numrows));
  if (digitnum == log10(E.numrows)) digitnum++;
  char line_num_format_buf[32];
  snprintf(line_num_format_buf, 32, "%%0%dd" KILO_LINE_NUM_SEP, digitnum);
  E.rowborder_width = digitnum + strlen(KILO_LINE_NUM_SEP);

  for (y = 0; y < E.screenrows; y++) {
    int filerow = y + E.rowoff;
    if (filerow >= E.numrows) {
      if (E.numrows == 0 && y == E.screenrows / 3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
          "Kilo editor -- version %s", KILO_VERSION);
        if (welcomelen > E.screencols) welcomelen = E.screencols;
        int padding = (E.screencols - welcomelen) / 2;
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--) abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      char line_num_buf[E.rowborder_width+1];
      snprintf(line_num_buf, E.rowborder_width+1, line_num_format_buf, E.row[filerow].idx);

      abAppend(ab, "\x1b[94m", 5);
      abAppend(ab, line_num_buf, E.rowborder_width+1);
      abAppend(ab, "\x1b[m", 3);
      
      int len = E.row[filerow].rsize - E.coloff;
      if (len < 0) len = 0;
      if (len > E.screencols - E.rowborder_width) len = E.screencols - E.rowborder_width;
      char *c = &E.row[filerow].render[E.coloff];
      unsigned char *hl = &E.row[filerow].hl[E.coloff];
      int current_color = -1;
      for (int j = 0; j < len; ++j)
      {
        if (iscntrl(c[j])) {
          char sym = (c[j] <= 26) ? '@' + c[j] : '?';
          abAppend(ab, "\x1b[7m", 4);
          abAppend(ab, &sym, 1);
          abAppend(ab, "\x1b[m", 3);
          if (current_color != -1) {
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
            abAppend(ab, buf, clen);
          }
        } else if (hl[j] == HL_NORMAL) {
          if (current_color != -1) {
            abAppend(ab, "\x1b[39m", 5);
            current_color = -1;
          }
          abAppend(ab, &c[j], 1);
        } else {
          int color = editorSyntaxToColor(hl[j]);
          if (current_color != color) {
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
            abAppend(ab, buf, clen);
            current_color = color;
          }
          abAppend(ab, &c[j], 1);
        }
      }
      abAppend(ab, "\x1b[39m", 5);
    }

    abAppend(ab, "\x1b[K", 3);
    abAppend(ab, "\r\n", 2);
  }
}

void editorDrawStatusBar(struct abuf *ab) {
  abAppend(ab, "\x1b[7m", 4);
  char status[80], rstatus[80];
  
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
    E.filename ? E.filename : "[No Name]", E.numrows,
    E.dirty ? "(modified)" : "");

  int rlen = snprintf(rstatus, sizeof(rstatus), "%s %d/%d",
    E.syntax ? E.syntax->filetype : "no ft", E.cy + 1, E.numrows);

  if (len > E.screencols) len = E.screencols;
  abAppend(ab, status, len);
  while (len < E.screencols) {
    if (E.screencols - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      len++;
    }
  }
  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols) msglen = E.screencols;
  if (msglen && time(NULL) - E.statusmsg_time < 5)
    abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen() {
  editorScroll();

  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
                                            (E.rx - E.coloff) + E.rowborder_width + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
}

/*** input ***/

char* editorPrompt(char *prompt, void (*callback)(char *, int)) {
  size_t bufsize = 128;
  char *buf = malloc(bufsize);

  size_t buflen = 0;
  buf[0] = '\0';

  while (1) {
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();

    int c = editorReadKey();

    if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
      if (buflen != 0) buf[--buflen] = '\0';
    } else if (c == '\x1b') {
      editorSetStatusMessage("");
      if (callback) callback(buf, c);
      free(buf);
      return NULL;
    } else if (c == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage("");
        if (callback) callback(buf, c);
        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }
    if (callback) callback(buf, c);
  }
}

void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

  switch (key) {
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx--;
      } else if (E.cy > 0) {
        E.cy--;
        E.cx = E.row[E.cy].size;
      }
      break;
    case ARROW_RIGHT:
      if (row && E.cx < row->size) {
        E.cx++;
      } else if (row && E.cx == row->size) {
        E.cy++;
        E.cx = 0;
      }
      break;
    case ARROW_UP:
      if (E.cy != 0) {
        E.cy--;
        E.cx = editorRowRxToCx(&E.row[E.cy], E.rx);
      }
      break;
    case ARROW_DOWN:
      if (E.cy < E.numrows) {
        E.cy++;
        if (E.cy < E.numrows)
          E.cx = editorRowRxToCx(&E.row[E.cy], E.rx);
      }
      break;
    case CTRL_ARROW_LEFT:
      if (E.cx == 0) {
        editorMoveCursor(ARROW_LEFT);
      } else if (row) {
        int isCurrAlnum = isalnum(row->chars[E.cx-1]) ? 1 : 0;
        while(E.cx > 0 && (isalnum(row->chars[E.cx-1]) ? 1 : 0) == isCurrAlnum) {
          E.cx--;
        }
      }
      break;
    case CTRL_ARROW_RIGHT:
      if (row) {
        if (E.cx == row->size) {
          editorMoveCursor(ARROW_RIGHT);
        } else {
          int isCurrAlnum = isalnum(row->chars[E.cx]) ? 1 : 0;
          while(E.cx < row->size && (isalnum(row->chars[E.cx]) ? 1 : 0) == isCurrAlnum) {
            E.cx++;
          }
        }
      }
      break;
  }

  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen) {
    E.cx = rowlen;
  }
}

void editorProcessKeypress(int c) {
  static int quit_times = KILO_QUIT_TIMES;

  switch (c) {
    case '\r':
      editorInsertNewLine();
      break;
    case CTRL_ENTER:
      if (E.cy < E.numrows)
        E.cx = E.row[E.cy].size;
      editorInsertNewLine();
      break;
    case CTRL_SHIFT_ENTER:
      E.cx = 0;
      editorInsertNewLine();
      E.cy--;
      break;

    case CTRL_KEY('q'):
      if (E.dirty && quit_times > 0) {
        editorSetStatusMessage("Warning! File not saved. "
          "Press Ctrl-Q %d  more times to quit.", quit_times);
        quit_times--;
        return;
      }
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
    case CTRL_KEY('s'):
      editorSave();
      break;
    case CTRL_KEY('f'):
      editorFind();
      break;

    case CTRL_KEY('x'):
      editorDelLine();
      break;

    case HOME_KEY:
      E.cx = 0;
      break;

    case END_KEY:
      if (E.cy < E.numrows)
        E.cx = E.row[E.cy].size;
      break;

    case BACKSPACE:
    case CTRL_KEY('h'):
    case DEL_KEY:
      if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
      editorDelChar();
      break;

    case PAGE_UP:
    case PAGE_DOWN:
      {
        if (c == PAGE_UP) {
          E.cy = E.rowoff;
        } else if (c == PAGE_DOWN) {
          E.cy = E.rowoff + E.screenrows - 1;
          if (E.cy > E.numrows) E.cy = E.numrows;
        }

        int times = E.screenrows;
        while (times--)
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;

    case CTRL_ARROW_UP:
      if (E.rowoff > 0) {
        E.rowoff--;
        if (E.cy > E.rowoff + E.screenrows)
          editorMoveCursor(ARROW_UP);
      }
      break;
    case CTRL_ARROW_DOWN:
      if (E.rowoff < E.numrows) {
        E.rowoff++;
        if (E.cy < E.rowoff)
          editorMoveCursor(ARROW_DOWN);
      }
      break;

    case CTRL_BACKSPACE:
      editorDelWord();
      break;

    case CTRL_DELETE:
      if (E.cy != E.numrows) {
        if (E.cx == E.row[E.cy].size) {
          editorProcessKeypress(DEL_KEY);
        } else {
          editorInsertChar(isalnum(E.row[E.cy].chars[E.cx]) ? ' ' : 'a');
          editorMoveCursor(CTRL_ARROW_RIGHT);
          editorDelWord();
          editorDelChar();
        }
      }
      break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
    case CTRL_ARROW_RIGHT:
    case CTRL_ARROW_LEFT:
      editorMoveCursor(c);
      break;

    case CTRL_SHIFT_ARROW_DOWN:
      editorMoveLineDown();
      break;
    case CTRL_SHIFT_ARROW_UP:
      editorMoveLineUp();
      break;

    case CTRL_SHIFT_D:
      editorDuplicateLine();
      break;

    case CTRL_KEY('l'):
    case '\x1b':
      break; 

    default:
      editorInsertChar(c);
      break;
  }
  quit_times = KILO_QUIT_TIMES;
}

/*** init ***/

void initEditor() {
  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.rowborder_width = 0;
  E.row = NULL;
  E.dirty = 0;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;
  E.syntax = NULL;

  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
  E.screenrows -= 2;
}

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  editorSetStatusMessage(
    "HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

  while (1) {
    editorRefreshScreen();
    int c = editorReadKey();
    editorProcessKeypress(c);
  }

  return 0;
}
