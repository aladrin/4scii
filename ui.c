#include "4scii.h"

static const char *color_names[8] = {
    "Black", "Red", "Green", "Yellow",
    "Blue", "Magenta", "Cyan", "White"
};

static const char brush_presets[] = "*#@%+.:=~^/\\|-_OX$!&?";

short ui_pair_id(short fg, short bg) {
    short safe_fg = (fg >= 0 && fg < 8) ? fg : COLOR_WHITE;
    short safe_bg = (bg >= 0 && bg < 8) ? bg : COLOR_BLACK;
    return (short)(safe_fg * 8 + safe_bg + 1);
}

void ui_init_colors(void) {
    if (has_colors()) {
        start_color();
        for (short fg = 0; fg < 8; fg++) {
            for (short bg = 0; bg < 8; bg++) {
                init_pair(ui_pair_id(fg, bg), fg, bg);
            }
        }
    }
}

void ui_set_status(Editor *ed, const char *fmt, ...) {
    if (ed == NULL) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ed->status_msg, sizeof(ed->status_msg), fmt, ap);
    va_end(ap);
}

static const char *mode_short_label(DrawMode mode) {
    switch (mode) {
        case MODE_DRAG:  return "[Draw]";
        case MODE_ERASE: return "[Eras]";
        default:         return "[Draw]";
    }
}

void ui_render_status(Editor *ed) {
    if (ed == NULL) {
        return;
    }
    int y = ed->term_rows - 1;
    if (y < 0) {
        return;
    }

    attr_t bar_attr = COLOR_PAIR(ui_pair_id(COLOR_BLACK, COLOR_WHITE));
    attron(bar_attr);
    mvhline(y, 0, ' ', ed->term_cols);

    /* Fixed-offset interactive toolbar buttons */
    mvprintw(y, 0, "[Menu]");
    attroff(bar_attr);

    /* Brush swatch in active FG/BG/Bold */
    attr_t swatch_attr = COLOR_PAIR(ui_pair_id(ed->fg, ed->bg)) | (ed->bold ? A_BOLD : A_NORMAL);
    attron(swatch_attr);
    mvprintw(y, 6, "[B:%c]", isprint((unsigned char)ed->brush) ? ed->brush : '*');
    attroff(swatch_attr);

    attron(bar_attr);
    mvprintw(y, 11, "%s", mode_short_label(ed->draw_mode));
    mvprintw(y, 17, "[<]");
    mvprintw(y, 20, "%3d/%-3d", ed->frame_index + 1, ed->frame_count);
    mvprintw(y, 27, "[>]");
    mvprintw(y, 30, "[+F]");
    mvprintw(y, 34, "[Cp]");
    mvprintw(y, 38, "[-F]");
    mvprintw(y, 42, "[Play]");
    mvprintw(y, 48, "[Save]");
    mvprintw(y, 54, "[Cast]");

    if (ed->term_cols > 61) {
        int rem = ed->term_cols - 61;
        mvprintw(y, 60, " %-*.*s", rem - 1, rem - 1, ed->status_msg);
    }
    attroff(bar_attr);
}

static void render_frame_contents(Editor *ed, Frame *f, bool allow_onion) {
    Frame *prev_f = (allow_onion && ed->onion_skin && f != NULL) ? f->prev : NULL;
    short default_pair = ui_pair_id(COLOR_WHITE, COLOR_BLACK);
    short onion_pair = ui_pair_id(COLOR_BLUE, COLOR_BLACK);

    for (int y = 0; y < ed->canvas_rows; y++) {
        for (int x = 0; x < ed->canvas_cols; x++) {
            Cell *c = frame_cell(ed, f, y, x);
            if (c != NULL && isprint((unsigned char)c->ch)) {
                attr_t a = COLOR_PAIR(ui_pair_id(c->fg, c->bg)) | (c->bold ? A_BOLD : A_NORMAL);
                attron(a);
                mvaddch(y, x, (unsigned char)c->ch);
                attroff(a);
            } else if (prev_f != NULL) {
                Cell *pc = frame_cell(ed, prev_f, y, x);
                if (pc != NULL && isprint((unsigned char)pc->ch)) {
                    attr_t a = COLOR_PAIR(onion_pair) | A_DIM;
                    attron(a);
                    mvaddch(y, x, (unsigned char)pc->ch);
                    attroff(a);
                } else {
                    attron(COLOR_PAIR(default_pair));
                    mvaddch(y, x, ' ');
                    attroff(COLOR_PAIR(default_pair));
                }
            } else {
                attron(COLOR_PAIR(default_pair));
                mvaddch(y, x, ' ');
                attroff(COLOR_PAIR(default_pair));
            }
        }
    }
}

void ui_render(Editor *ed) {
    if (ed == NULL) {
        return;
    }
    render_frame_contents(ed, ed->current_frame, true);
    ui_render_status(ed);
    refresh();
}

bool ui_prompt_string(Editor *ed, const char *prompt, char *out_buf, size_t out_len, const char *default_val) {
    if (ed == NULL || out_buf == NULL || out_len == 0) {
        return false;
    }

    char buf[MAX_FILENAME];
    if (default_val != NULL) {
        snprintf(buf, sizeof(buf), "%s", default_val);
    } else {
        buf[0] = '\0';
    }
    size_t len = strlen(buf);

    int y = ed->term_rows - 1;
    curs_set(1);
    timeout(-1);

    bool accepted = false;
    bool done = false;

    while (!done) {
        attr_t bar_attr = COLOR_PAIR(ui_pair_id(COLOR_BLACK, COLOR_CYAN)) | A_BOLD;
        attron(bar_attr);
        mvhline(y, 0, ' ', ed->term_cols);
        mvprintw(y, 0, "%s%s", prompt, buf);
        attroff(bar_attr);
        refresh();

        int ch = wgetch(stdscr);
        if (ch == 27 || ch == CTRL('G')) {
            accepted = false;
            done = true;
        } else if (ch == '\r' || ch == '\n' || ch == KEY_ENTER) {
            if (len > 0) {
                snprintf(out_buf, out_len, "%s", buf);
                accepted = true;
            }
            done = true;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b' || ch == CTRL('H')) {
            if (len > 0) {
                buf[--len] = '\0';
            }
        } else if (ch == CTRL('U')) {
            len = 0;
            buf[0] = '\0';
        } else if (isprint(ch) && len + 1 < sizeof(buf) && len + 1 < out_len) {
            buf[len++] = (char)ch;
            buf[len] = '\0';
        }
    }

    curs_set(0);
    ui_render(ed);
    return accepted;
}

void ui_play_animation(Editor *ed) {
    if (ed == NULL || ed->first_frame == NULL) {
        return;
    }

    Frame *saved_frame = ed->current_frame;
    int saved_index = ed->frame_index;

    Frame *f = ed->first_frame;
    int idx = 0;
    bool playing = true;

    while (playing) {
        render_frame_contents(ed, f, false);

        int y = ed->term_rows - 1;
        attr_t bar_attr = COLOR_PAIR(ui_pair_id(COLOR_BLACK, COLOR_GREEN)) | A_BOLD;
        attron(bar_attr);
        mvhline(y, 0, ' ', ed->term_cols);
        mvprintw(y, 0, " [PLAYING ANIMATION] Frame %d/%d (%dms) - Press any key or click to stop ",
                 idx + 1, ed->frame_count, f->delay_ms);
        attroff(bar_attr);
        refresh();

        int wait_ms = (f->delay_ms >= MIN_FRAME_DELAY_MS) ? f->delay_ms : MIN_FRAME_DELAY_MS;
        timeout(wait_ms);
        int ch = wgetch(stdscr);
        if (ch != ERR) {
            if (ch == KEY_MOUSE) {
                MEVENT mev;
                getmouse(&mev);
                if (mev.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED | BUTTON2_PRESSED | BUTTON3_PRESSED)) {
                    playing = false;
                }
            } else {
                playing = false;
            }
        }

        if (playing) {
            if (f->next != NULL) {
                f = f->next;
                idx++;
            } else {
                f = ed->first_frame;
                idx = 0;
            }
        }
    }

    timeout(-1);
    ed->current_frame = saved_frame;
    ed->frame_index = saved_index;
    ui_set_status(ed, "Animation playback stopped.");
    ui_render(ed);
}

void ui_open_menu(Editor *ed) {
    if (ed == NULL) {
        return;
    }

    int mh = 21;
    int mw = 74;
    int sy = (ed->term_rows > mh) ? (ed->term_rows - mh) / 2 : 0;
    int sx = (ed->term_cols > mw) ? (ed->term_cols - mw) / 2 : 0;
    if (sy + mh > ed->term_rows) mh = ed->term_rows;
    if (sx + mw > ed->term_cols) mw = ed->term_cols;

    WINDOW *win = newwin(mh, mw, sy, sx);
    keypad(win, TRUE);

    bool in_menu = true;
    int preset_len = (int)strlen(brush_presets);

    while (in_menu) {
        werase(win);
        wbkgd(win, COLOR_PAIR(ui_pair_id(COLOR_WHITE, COLOR_BLACK)));
        box(win, 0, 0);

        wattron(win, A_BOLD | COLOR_PAIR(ui_pair_id(COLOR_CYAN, COLOR_BLACK)));
        mvwprintw(win, 1, 2, "4SCII CONTROL & COLOR MENU (Mouse Click or Hotkeys | ESC/F2 to Close)");
        wattroff(win, A_BOLD | COLOR_PAIR(ui_pair_id(COLOR_CYAN, COLOR_BLACK)));

        /* Row 3: Foreground Color Palette */
        mvwprintw(win, 3, 2, "FG Color (L/R):");
        for (short c = 0; c < 8; c++) {
            int cx = 18 + c * 6;
            short label_fg = (c == COLOR_BLACK) ? COLOR_WHITE : COLOR_BLACK;
            wattron(win, COLOR_PAIR(ui_pair_id(label_fg, c)) | (ed->fg == c ? A_BOLD : A_NORMAL));
            mvwprintw(win, 3, cx, "%c%-3.3s%c",
                      (ed->fg == c) ? '[' : ' ',
                      color_names[c],
                      (ed->fg == c) ? ']' : ' ');
            wattroff(win, COLOR_PAIR(ui_pair_id(label_fg, c)) | (ed->fg == c ? A_BOLD : A_NORMAL));
        }

        /* Row 5: Background Color Palette */
        mvwprintw(win, 5, 2, "BG Color (U/D):");
        for (short c = 0; c < 8; c++) {
            int cx = 18 + c * 6;
            short label_fg = (c == COLOR_BLACK) ? COLOR_WHITE : COLOR_BLACK;
            wattron(win, COLOR_PAIR(ui_pair_id(label_fg, c)) | (ed->bg == c ? A_BOLD : A_NORMAL));
            mvwprintw(win, 5, cx, "%c%-3.3s%c",
                      (ed->bg == c) ? '[' : ' ',
                      color_names[c],
                      (ed->bg == c) ? ']' : ' ');
            wattroff(win, COLOR_PAIR(ui_pair_id(label_fg, c)) | (ed->bg == c ? A_BOLD : A_NORMAL));
        }

        /* Row 7: Bold & Brush Preview & Mode */
        mvwprintw(win, 7, 2, "Style & Mode  :");
        wattron(win, ed->bold ? (A_REVERSE | A_BOLD) : A_NORMAL);
        mvwprintw(win, 7, 18, "[ (B)old: %-3s ]", ed->bold ? "ON" : "OFF");
        wattroff(win, ed->bold ? (A_REVERSE | A_BOLD) : A_NORMAL);

        mvwprintw(win, 7, 35, "Sample:");
        attr_t sample_attr = COLOR_PAIR(ui_pair_id(ed->fg, ed->bg)) | (ed->bold ? A_BOLD : A_NORMAL);
        wattron(win, sample_attr);
        mvwprintw(win, 7, 43, " %c%c%c ", ed->brush, ed->brush, ed->brush);
        wattroff(win, sample_attr);

        mvwprintw(win, 7, 50, "[ (M)ode: %-5s ]",
                  ed->draw_mode == MODE_DRAG ? "Draw" : "Erase");

        /* Row 9: Brush Presets */
        mvwprintw(win, 9, 2, "Brush (C=Type):");
        for (int i = 0; i < preset_len; i++) {
            int bx = 18 + i * 2;
            bool sel = (ed->brush == brush_presets[i]);
            if (sel) wattron(win, A_REVERSE | A_BOLD);
            mvwprintw(win, 9, bx, "%c", brush_presets[i]);
            if (sel) wattroff(win, A_REVERSE | A_BOLD);
        }
        mvwprintw(win, 9, 18 + preset_len * 2 + 2, "[Type(C)]");

        /* Row 11: Frame Navigation & Creation */
        mvwprintw(win, 11, 2, "Frames (%2d/%-2d):", ed->frame_index + 1, ed->frame_count);
        mvwprintw(win, 11, 18, "[< Prev(,)]");
        mvwprintw(win, 11, 30, "[Next(.) >]");
        mvwprintw(win, 11, 42, "[+Blank(N)]");
        mvwprintw(win, 11, 54, "[Dup(D)]");
        mvwprintw(win, 11, 63, "[Del(X)]");

        /* Row 13: Frame Timing & Playback */
        mvwprintw(win, 13, 2, "Timing (%4dms):", ed->current_frame->delay_ms);
        mvwprintw(win, 13, 18, "[-20ms(-)]");
        mvwprintw(win, 13, 29, "[+20ms(+)]");
        mvwprintw(win, 13, 40, "[Onion(O):%-3s]", ed->onion_skin ? "ON" : "OFF");
        mvwprintw(win, 13, 55, "[Play(P)]");

        /* Row 15: Edit & File Operations */
        mvwprintw(win, 15, 2, "Edit & Files  :");
        mvwprintw(win, 15, 18, "[Undo(Z)]");
        mvwprintw(win, 15, 28, "[Redo(Y)]");
        mvwprintw(win, 15, 38, "[Clear(K)]");
        mvwprintw(win, 15, 49, "[Save .txt(S)]");

        /* Row 17: Load & Export & Close */
        mvwprintw(win, 17, 18, "[Load .txt(L)]");
        mvwprintw(win, 17, 33, "[Export .cast(E)]");
        wattron(win, A_BOLD);
        mvwprintw(win, 17, 51, "[Close Menu(Q)]");
        wattroff(win, A_BOLD);

        mvwprintw(win, 19, 2, "Active Files  : TXT='%s' | CAST='%s'",
                  ed->current_txt_file, ed->current_cast_file);

        wrefresh(win);

        int ch = wgetch(win);
        if (ch == 27 || ch == KEY_F(2) || ch == CTRL('G') || ch == 'q' || ch == 'Q' || ch == '\r' || ch == '\n') {
            in_menu = false;
        } else if (ch >= '1' && ch <= '8') {
            ed->fg = (short)(ch - '1');
        } else if (ch == KEY_LEFT) {
            ed->fg = (short)((ed->fg + 7) % 8);
        } else if (ch == KEY_RIGHT) {
            ed->fg = (short)((ed->fg + 1) % 8);
        } else if (ch == 'f' || ch == 'F' || ch == KEY_DOWN) {
            ed->bg = (short)((ed->bg + 7) % 8);
        } else if (ch == 'g' || ch == 'G' || ch == KEY_UP) {
            ed->bg = (short)((ed->bg + 1) % 8);
        } else if (ch == 'b' || ch == 'B') {
            ed->bold = !ed->bold;
        } else if (ch == 'm' || ch == 'M') {
            ed->draw_mode = (ed->draw_mode == MODE_DRAG) ? MODE_ERASE : MODE_DRAG;
        } else if (ch == 'c' || ch == 'C') {
            mvwprintw(win, 9, 18, "Press any printable key for brush...                  ");
            wrefresh(win);
            int bch = wgetch(win);
            if (isprint(bch)) {
                ed->brush = (char)bch;
                if (ed->draw_mode == MODE_ERASE) {
                    ed->draw_mode = MODE_DRAG;
                }
            }
        } else if (ch == ',' || ch == '<' || ch == KEY_PPAGE) {
            frame_prev(ed);
        } else if (ch == '.' || ch == '>' || ch == KEY_NPAGE) {
            frame_next(ed);
        } else if (ch == 'n' || ch == 'N') {
            frame_new_after(ed, false);
            ui_set_status(ed, "Created blank frame %d/%d", ed->frame_index + 1, ed->frame_count);
        } else if (ch == 'd' || ch == 'D') {
            frame_new_after(ed, true);
            ui_set_status(ed, "Duplicated to frame %d/%d", ed->frame_index + 1, ed->frame_count);
        } else if (ch == 'x' || ch == 'X' || ch == KEY_DC) {
            frame_delete_current(ed);
            ui_set_status(ed, "Deleted frame (now %d/%d)", ed->frame_index + 1, ed->frame_count);
        } else if (ch == '-' || ch == '_') {
            if (ed->current_frame->delay_ms > MIN_FRAME_DELAY_MS) {
                ed->current_frame->delay_ms -= 20;
            }
        } else if (ch == '+' || ch == '=') {
            if (ed->current_frame->delay_ms < MAX_FRAME_DELAY_MS) {
                ed->current_frame->delay_ms += 20;
            }
        } else if (ch == 'o' || ch == 'O') {
            ed->onion_skin = !ed->onion_skin;
        } else if (ch == 'p' || ch == 'P') {
            in_menu = false;
            delwin(win);
            touchwin(stdscr);
            ui_play_animation(ed);
            return;
        } else if (ch == 'z' || ch == 'Z') {
            canvas_undo(ed);
        } else if (ch == 'y' || ch == 'Y') {
            canvas_redo(ed);
        } else if (ch == 'k' || ch == 'K') {
            canvas_push_undo(ed);
            frame_clear(ed, ed->current_frame);
        } else if (ch == 's' || ch == 'S') {
            in_menu = false;
            delwin(win);
            touchwin(stdscr);
            fileio_prompt_save_txt(ed);
            return;
        } else if (ch == 'l' || ch == 'L') {
            in_menu = false;
            delwin(win);
            touchwin(stdscr);
            fileio_prompt_load_txt(ed);
            return;
        } else if (ch == 'e' || ch == 'E') {
            in_menu = false;
            delwin(win);
            touchwin(stdscr);
            fileio_prompt_export_cast(ed);
            return;
        } else if (ch == KEY_MOUSE) {
            MEVENT mev;
            if (getmouse(&mev) == OK && (mev.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED))) {
                int ry = mev.y - sy;
                int rx = mev.x - sx;
                if (ry < 0 || ry >= mh || rx < 0 || rx >= mw) {
                    in_menu = false;
                } else if (ry == 3 && rx >= 18 && rx < 18 + 8 * 6) {
                    ed->fg = (short)((rx - 18) / 6);
                } else if (ry == 5 && rx >= 18 && rx < 18 + 8 * 6) {
                    ed->bg = (short)((rx - 18) / 6);
                } else if (ry == 7) {
                    if (rx >= 18 && rx <= 33) {
                        ed->bold = !ed->bold;
                    } else if (rx >= 50 && rx <= 66) {
                        ed->draw_mode = (ed->draw_mode == MODE_DRAG) ? MODE_ERASE : MODE_DRAG;
                    }
                } else if (ry == 9) {
                    if (rx >= 18 && rx < 18 + preset_len * 2) {
                        int idx = (rx - 18) / 2;
                        if (idx >= 0 && idx < preset_len) {
                            ed->brush = brush_presets[idx];
                            if (ed->draw_mode == MODE_ERASE) {
                                ed->draw_mode = MODE_DRAG;
                            }
                        }
                    } else if (rx >= 18 + preset_len * 2 + 2) {
                        mvwprintw(win, 9, 18, "Press any printable key for brush...                  ");
                        wrefresh(win);
                        int bch = wgetch(win);
                        if (isprint(bch)) {
                            ed->brush = (char)bch;
                            if (ed->draw_mode == MODE_ERASE) {
                                ed->draw_mode = MODE_DRAG;
                            }
                        }
                    }
                } else if (ry == 11) {
                    if (rx >= 18 && rx <= 28) frame_prev(ed);
                    else if (rx >= 30 && rx <= 40) frame_next(ed);
                    else if (rx >= 42 && rx <= 52) frame_new_after(ed, false);
                    else if (rx >= 54 && rx <= 61) frame_new_after(ed, true);
                    else if (rx >= 63 && rx <= 70) frame_delete_current(ed);
                } else if (ry == 13) {
                    if (rx >= 18 && rx <= 27 && ed->current_frame->delay_ms > MIN_FRAME_DELAY_MS) {
                        ed->current_frame->delay_ms -= 20;
                    } else if (rx >= 29 && rx <= 38 && ed->current_frame->delay_ms < MAX_FRAME_DELAY_MS) {
                        ed->current_frame->delay_ms += 20;
                    } else if (rx >= 40 && rx <= 53) {
                        ed->onion_skin = !ed->onion_skin;
                    } else if (rx >= 55 && rx <= 63) {
                        in_menu = false;
                        delwin(win);
                        touchwin(stdscr);
                        ui_play_animation(ed);
                        return;
                    }
                } else if (ry == 15) {
                    if (rx >= 18 && rx <= 26) canvas_undo(ed);
                    else if (rx >= 28 && rx <= 36) canvas_redo(ed);
                    else if (rx >= 38 && rx <= 47) {
                        canvas_push_undo(ed);
                        frame_clear(ed, ed->current_frame);
                    } else if (rx >= 49 && rx <= 63) {
                        in_menu = false;
                        delwin(win);
                        touchwin(stdscr);
                        fileio_prompt_save_txt(ed);
                        return;
                    }
                } else if (ry == 17) {
                    if (rx >= 18 && rx <= 31) {
                        in_menu = false;
                        delwin(win);
                        touchwin(stdscr);
                        fileio_prompt_load_txt(ed);
                        return;
                    } else if (rx >= 33 && rx <= 49) {
                        in_menu = false;
                        delwin(win);
                        touchwin(stdscr);
                        fileio_prompt_export_cast(ed);
                        return;
                    } else if (rx >= 51 && rx <= 65) {
                        in_menu = false;
                    }
                }
            }
        }
    }

    delwin(win);
    touchwin(stdscr);
    ui_render(ed);
}

void ui_handle_status_click(Editor *ed, int x) {
    if (ed == NULL) {
        return;
    }
    if (x >= 0 && x <= 10) {
        ui_open_menu(ed);
    } else if (x >= 11 && x <= 16) {
        ed->draw_mode = (ed->draw_mode == MODE_DRAG) ? MODE_ERASE : MODE_DRAG;
        ui_set_status(ed, "Draw mode: %s",
                      ed->draw_mode == MODE_DRAG ? "Draw" : "Erase");
        ui_render(ed);
    } else if (x >= 17 && x <= 19) {
        frame_prev(ed);
        ui_set_status(ed, "Frame %d/%d", ed->frame_index + 1, ed->frame_count);
        ui_render(ed);
    } else if (x >= 27 && x <= 29) {
        frame_next(ed);
        ui_set_status(ed, "Frame %d/%d", ed->frame_index + 1, ed->frame_count);
        ui_render(ed);
    } else if (x >= 30 && x <= 33) {
        frame_new_after(ed, false);
        ui_set_status(ed, "New blank frame %d/%d", ed->frame_index + 1, ed->frame_count);
        ui_render(ed);
    } else if (x >= 34 && x <= 37) {
        frame_new_after(ed, true);
        ui_set_status(ed, "Cloned frame %d/%d", ed->frame_index + 1, ed->frame_count);
        ui_render(ed);
    } else if (x >= 38 && x <= 41) {
        frame_delete_current(ed);
        ui_set_status(ed, "Deleted frame (now %d/%d)", ed->frame_index + 1, ed->frame_count);
        ui_render(ed);
    } else if (x >= 42 && x <= 47) {
        ui_play_animation(ed);
    } else if (x >= 48 && x <= 53) {
        fileio_prompt_save_txt(ed);
    } else if (x >= 54 && x <= 59) {
        fileio_prompt_export_cast(ed);
    } else {
        ui_open_menu(ed);
    }
}
