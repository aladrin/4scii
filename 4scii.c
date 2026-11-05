#include "4scii.h"

static const char *fg_bg_names[8] = {
    "Black", "Red", "Green", "Yellow",
    "Blue", "Magenta", "Cyan", "White"
};

static void apply_brush_at(Editor *ed, int y, int x) {
    if (ed == NULL || y < 0 || y >= ed->canvas_rows || x < 0 || x >= ed->canvas_cols) {
        return;
    }
    ed->cursor_y = y;
    ed->cursor_x = x;
    if (ed->draw_mode == MODE_ERASE) {
        canvas_erase_cell(ed, y, x);
    } else {
        canvas_put_cell(ed, y, x);
    }
}

int main(int argc, char *argv[]) {
    const char *term = getenv("TERM");
    if (term == NULL || term[0] == '\0') {
        setenv("TERM", "xterm", 1);
    }

    initscr();
    raw();
    noecho();
    curs_set(0);
    nonl();
    keypad(stdscr, TRUE);
    timeout(-1);

    ui_init_colors();

    mouseinterval(0);
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    printf("\033[?1002h");
    fflush(stdout);

    int term_rows = 0, term_cols = 0;
    getmaxyx(stdscr, term_rows, term_cols);

    Editor ed;
    editor_init(&ed, term_rows, term_cols);

    if (argc > 1 && argv[1] != NULL && argv[1][0] != '\0') {
        fileio_load_auto(&ed, argv[1]);
    }

    ui_render(&ed);

    while (ed.running) {
        int input = wgetch(stdscr);
        if (input == ERR) {
            continue;
        }

        if (input == CTRL('Q')) {
            ed.running = false;
            break;
        } else if (input == KEY_F(2) || input == CTRL('G') || input == '\t') {
            ed.mouse_down = false;
            ed.stroke_active = false;
            ui_open_menu(&ed);
        } else if (input == CTRL('S')) {
            ed.mouse_down = false;
            ed.stroke_active = false;
            fileio_save_txt_quick(&ed);
        } else if (input == CTRL('A')) {
            ed.mouse_down = false;
            ed.stroke_active = false;
            fileio_prompt_save_as(&ed);
        } else if (input == CTRL('O') || input == CTRL('L')) {
            ed.mouse_down = false;
            ed.stroke_active = false;
            fileio_prompt_load_txt(&ed);
        } else if (input == CTRL('R')) {
            ed.mouse_down = false;
            ed.stroke_active = false;
            fileio_prompt_load_cast(&ed);
        } else if (input == CTRL('E')) {
            ed.mouse_down = false;
            ed.stroke_active = false;
            fileio_prompt_export_cast(&ed);
        } else if (input == CTRL('P')) {
            ed.mouse_down = false;
            ed.stroke_active = false;
            ui_play_animation(&ed);
        } else if (input == CTRL('Z')) {
            if (canvas_undo(&ed)) {
                ui_set_status(&ed, "Undo (depth: %d)", ed.current_frame->undo_depth);
            } else {
                ui_set_status(&ed, "Nothing to undo.");
            }
            ui_render(&ed);
        } else if (input == CTRL('Y')) {
            if (canvas_redo(&ed)) {
                ui_set_status(&ed, "Redo (depth: %d)", ed.current_frame->redo_depth);
            } else {
                ui_set_status(&ed, "Nothing to redo.");
            }
            ui_render(&ed);
        } else if (input == CTRL('N')) {
            frame_new_after(&ed, false);
            ui_set_status(&ed, "Added blank frame (%d/%d)", ed.frame_index + 1, ed.frame_count);
            ui_render(&ed);
        } else if (input == CTRL('D')) {
            frame_new_after(&ed, true);
            ui_set_status(&ed, "Duplicated frame (%d/%d)", ed.frame_index + 1, ed.frame_count);
            ui_render(&ed);
        } else if (input == CTRL('X') || input == KEY_DC) {
            frame_delete_current(&ed);
            ui_set_status(&ed, "Deleted frame (now %d/%d)", ed.frame_index + 1, ed.frame_count);
            ui_render(&ed);
        } else if (input == CTRL('K')) {
            canvas_push_undo(&ed);
            frame_clear(&ed, ed.current_frame);
            ui_set_status(&ed, "Cleared frame %d/%d", ed.frame_index + 1, ed.frame_count);
            ui_render(&ed);
        } else if (input == KEY_LEFT) {
            ed.fg = (short)((ed.fg + 7) % 8);
            ui_set_status(&ed, "FG Color: %s | BG Color: %s",
                          fg_bg_names[ed.fg], fg_bg_names[ed.bg]);
            ui_render_status(&ed);
            refresh();
        } else if (input == KEY_RIGHT) {
            ed.fg = (short)((ed.fg + 1) % 8);
            ui_set_status(&ed, "FG Color: %s | BG Color: %s",
                          fg_bg_names[ed.fg], fg_bg_names[ed.bg]);
            ui_render_status(&ed);
            refresh();
        } else if (input == KEY_UP) {
            ed.bg = (short)((ed.bg + 1) % 8);
            ui_set_status(&ed, "FG Color: %s | BG Color: %s",
                          fg_bg_names[ed.fg], fg_bg_names[ed.bg]);
            ui_render_status(&ed);
            refresh();
        } else if (input == KEY_DOWN) {
            ed.bg = (short)((ed.bg + 7) % 8);
            ui_set_status(&ed, "FG Color: %s | BG Color: %s",
                          fg_bg_names[ed.fg], fg_bg_names[ed.bg]);
            ui_render_status(&ed);
            refresh();
        } else if (input == KEY_PPAGE) {
            frame_prev(&ed);
            ui_set_status(&ed, "Frame %d/%d (%dms)",
                          ed.frame_index + 1, ed.frame_count, ed.current_frame->delay_ms);
            ui_render(&ed);
        } else if (input == KEY_NPAGE) {
            frame_next(&ed);
            ui_set_status(&ed, "Frame %d/%d (%dms)",
                          ed.frame_index + 1, ed.frame_count, ed.current_frame->delay_ms);
            ui_render(&ed);
        } else if (input == KEY_MOUSE) {
            MEVENT mev;
            if (getmouse(&mev) != OK) {
                continue;
            }

            int my = mev.y;
            int mx = mev.x;

            /* Click on the bottom status/toolbar line */
            if (my == ed.term_rows - 1) {
                if (mev.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED)) {
                    ed.mouse_down = false;
                    ed.stroke_active = false;
                    ui_handle_status_click(&ed, mx);
                }
                continue;
            }

            /* Right-click opens the interactive ncurses menu */
            if (mev.bstate & (BUTTON3_PRESSED | BUTTON3_CLICKED)) {
                ed.mouse_down = false;
                ed.stroke_active = false;
                ui_open_menu(&ed);
                continue;
            }

            /* Middle-click toggles Draw/Erase mode */
            if (mev.bstate & (BUTTON2_PRESSED | BUTTON2_CLICKED)) {
                ed.draw_mode = (ed.draw_mode == MODE_DRAG) ? MODE_ERASE : MODE_DRAG;
                ed.stroke_active = false;
                ui_set_status(&ed, "Draw mode: %s",
                              ed.draw_mode == MODE_DRAG ? "Draw" : "Erase");
                ui_render_status(&ed);
                refresh();
                continue;
            }

            /* Canvas area mouse handling */
            if (my >= 0 && my < ed.canvas_rows && mx >= 0 && mx < ed.canvas_cols) {
                if (mev.bstate & BUTTON1_PRESSED) {
                    ed.mouse_down = true;
                    if (!ed.stroke_active) {
                        canvas_push_undo(&ed);
                        ed.stroke_active = true;
                    }
                    apply_brush_at(&ed, my, mx);
                    ui_set_status(&ed, "Pos:(%d,%d) Brush:'%c' Frame:%d/%d",
                                  mx, my, ed.brush, ed.frame_index + 1, ed.frame_count);
                    ui_render(&ed);
                } else if (mev.bstate & BUTTON1_RELEASED) {
                    ed.mouse_down = false;
                    ed.stroke_active = false;
                } else if (mev.bstate & BUTTON1_CLICKED) {
                    canvas_push_undo(&ed);
                    apply_brush_at(&ed, my, mx);
                    ed.mouse_down = false;
                    ed.stroke_active = false;
                    ui_set_status(&ed, "Pos:(%d,%d) Brush:'%c' Frame:%d/%d",
                                  mx, my, ed.brush, ed.frame_index + 1, ed.frame_count);
                    ui_render(&ed);
                } else if (ed.mouse_down) {
                    if (!ed.stroke_active) {
                        canvas_push_undo(&ed);
                        ed.stroke_active = true;
                    }
                    apply_brush_at(&ed, my, mx);
                    ui_set_status(&ed, "Pos:(%d,%d) Brush:'%c' Frame:%d/%d",
                                  mx, my, ed.brush, ed.frame_index + 1, ed.frame_count);
                    ui_render(&ed);
                }
            }
        } else if (isprint(input)) {
            ed.brush = (char)input;
            if (ed.draw_mode == MODE_ERASE) {
                ed.draw_mode = MODE_DRAG;
            }
            ui_set_status(&ed, "Selected brush character: '%c'", ed.brush);
            ui_render_status(&ed);
            refresh();
        }
    }

    printf("\033[?1002l");
    fflush(stdout);
    endwin();
    editor_free(&ed);
    return 0;
}
