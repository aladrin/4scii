#ifndef FOURSCII_H
#define FOURSCII_H

#include <sys/types.h>
#include <ctype.h>
#if defined(HAVE_NCURSESW_CURSES_H)
#include <ncursesw/curses.h>
#elif defined(HAVE_NCURSES_CURSES_H)
#include <ncurses/curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#else
#include <curses.h>
#endif
#include <err.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#ifndef CTRL
#define CTRL(x) ((x) & 0x1f)
#endif

#define MAX_UNDO 64
#define MAX_FILENAME 256
#define STATUS_LEN 256
#define DEFAULT_FRAME_DELAY_MS 100
#define MIN_FRAME_DELAY_MS 20
#define MAX_FRAME_DELAY_MS 5000

typedef enum {
    MODE_DRAG = 0,
    MODE_ERASE = 1
} DrawMode;

typedef struct Cell {
    char ch;
    short fg;
    short bg;
    bool bold;
} Cell;

typedef struct UndoNode {
    Cell *cells;
    struct UndoNode *next;
} UndoNode;

typedef struct Frame {
    Cell *cells;
    int delay_ms;
    UndoNode *undo_stack;
    int undo_depth;
    UndoNode *redo_stack;
    int redo_depth;
    struct Frame *prev;
    struct Frame *next;
} Frame;

typedef struct Editor {
    int term_rows;
    int term_cols;
    int canvas_rows;
    int canvas_cols;
    int cursor_y;
    int cursor_x;
    char brush;
    short fg;
    short bg;
    bool bold;
    DrawMode draw_mode;
    bool mouse_down;
    bool stroke_active;
    bool onion_skin;
    bool running;
    char current_txt_file[MAX_FILENAME];
    char current_cast_file[MAX_FILENAME];
    char status_msg[STATUS_LEN];
    Frame *first_frame;
    Frame *current_frame;
    int frame_index;
    int frame_count;
} Editor;

/* canvas.c */
void editor_init(Editor *ed, int term_rows, int term_cols);
void editor_free(Editor *ed);
Frame *frame_create(int rows, int cols, int delay_ms);
void frame_free(Frame *f);
void frame_clear(Editor *ed, Frame *f);
Cell *frame_cell(Editor *ed, Frame *f, int y, int x);
void canvas_push_undo(Editor *ed);
bool canvas_undo(Editor *ed);
bool canvas_redo(Editor *ed);
void canvas_put_cell(Editor *ed, int y, int x);
void canvas_erase_cell(Editor *ed, int y, int x);
void frame_new_after(Editor *ed, bool clone_current);
void frame_delete_current(Editor *ed);
void frame_next(Editor *ed);
void frame_prev(Editor *ed);
bool frame_get_bounds(Editor *ed, Frame *f, int *ymin, int *ymax, int *xmin, int *xmax);

/* ui.c */
void ui_init_colors(void);
short ui_pair_id(short fg, short bg);
void ui_set_status(Editor *ed, const char *fmt, ...);
void ui_render(Editor *ed);
void ui_render_status(Editor *ed);
void ui_open_menu(Editor *ed);
bool ui_prompt_string(Editor *ed, const char *prompt, char *out_buf, size_t out_len, const char *default_val);
void ui_handle_status_click(Editor *ed, int x);
void ui_play_animation(Editor *ed);

/* fileio.c */
void fileio_ensure_ext(char *filename, size_t max_len, const char *ext);
bool fileio_save_txt(Editor *ed, const char *raw_filename);
bool fileio_load_txt(Editor *ed, const char *raw_filename);
bool fileio_export_cast(Editor *ed, const char *raw_filename);
void fileio_prompt_save_txt(Editor *ed);
void fileio_prompt_load_txt(Editor *ed);
void fileio_prompt_export_cast(Editor *ed);

#endif /* FOURSCII_H */
