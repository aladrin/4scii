#include "4scii.h"

#define PROMPT_HIST_MAX 32
#define MAX_COMP_MATCHES 128

static const char *color_names[8] = {
    "Black", "Red", "Green", "Yellow",
    "Blue", "Magenta", "Cyan", "White"
};

static const char brush_presets[] = "*#@%+.:=~^/\\|-_OX$!&?";

static char prompt_history[PROMPT_HIST_MAX][MAX_FILENAME];
static int prompt_history_count = 0;
static char prompt_yank_buf[MAX_FILENAME];

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
    mvprintw(y, 54, "[SvAs]");
    mvprintw(y, 60, "[Cast]");

    if (ed->term_cols > 67) {
        int rem = ed->term_cols - 67;
        mvprintw(y, 66, " %-*.*s", rem - 1, rem - 1, ed->status_msg);
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

static int cmp_str_ptr(const void *a, const void *b) {
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}

static void prompt_history_add(const char *entry) {
    if (entry == NULL || entry[0] == '\0') {
        return;
    }
    if (prompt_history_count > 0 &&
        strcmp(prompt_history[prompt_history_count - 1], entry) == 0) {
        return;
    }
    if (prompt_history_count < PROMPT_HIST_MAX) {
        snprintf(prompt_history[prompt_history_count++], MAX_FILENAME, "%s", entry);
    } else {
        memmove(prompt_history[0], prompt_history[1],
                (size_t)(PROMPT_HIST_MAX - 1) * MAX_FILENAME);
        snprintf(prompt_history[PROMPT_HIST_MAX - 1], MAX_FILENAME, "%s", entry);
    }
}

static void prompt_tab_complete(char *buf, size_t *len, size_t *pos, size_t max_cap,
                                char *hint_buf, size_t hint_cap) {
    hint_buf[0] = '\0';

    char prefix[MAX_FILENAME];
    if (*pos >= sizeof(prefix)) {
        return;
    }
    memcpy(prefix, buf, *pos);
    prefix[*pos] = '\0';

    char dir_path[MAX_FILENAME];
    char file_part[MAX_FILENAME];
    size_t slash_offset = 0;

    char *last_slash = strrchr(prefix, '/');
    if (last_slash == NULL) {
        snprintf(dir_path, sizeof(dir_path), ".");
        snprintf(file_part, sizeof(file_part), "%s", prefix);
        slash_offset = 0;
    } else {
        slash_offset = (size_t)(last_slash - prefix) + 1;
        if (slash_offset == 1) {
            snprintf(dir_path, sizeof(dir_path), "/");
        } else {
            size_t dlen = slash_offset - 1;
            if (dlen >= sizeof(dir_path)) {
                dlen = sizeof(dir_path) - 1;
            }
            memcpy(dir_path, prefix, dlen);
            dir_path[dlen] = '\0';
        }
        snprintf(file_part, sizeof(file_part), "%s", last_slash + 1);
    }

    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        return;
    }

    char *matches[MAX_COMP_MATCHES];
    int match_count = 0;
    size_t flen = strlen(file_part);

    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }
        if (file_part[0] != '.' && de->d_name[0] == '.') {
            continue;
        }
        if (strncmp(de->d_name, file_part, flen) == 0) {
            if (match_count < MAX_COMP_MATCHES) {
                char full_candidate[MAX_FILENAME * 2];
                if (strcmp(dir_path, "/") == 0) {
                    snprintf(full_candidate, sizeof(full_candidate), "/%s", de->d_name);
                } else {
                    snprintf(full_candidate, sizeof(full_candidate), "%s/%s", dir_path, de->d_name);
                }

                struct stat st;
                bool is_dir = (stat(full_candidate, &st) == 0 && S_ISDIR(st.st_mode));
                size_t nlen = strlen(de->d_name);
                char *item = malloc(nlen + (is_dir ? 2 : 1));
                if (item == NULL) {
                    err(1, "malloc");
                }
                memcpy(item, de->d_name, nlen);
                if (is_dir) {
                    item[nlen++] = '/';
                }
                item[nlen] = '\0';
                matches[match_count++] = item;
            }
        }
    }
    closedir(dir);

    if (match_count == 0) {
        snprintf(hint_buf, hint_cap, "[no match]");
        return;
    }

    qsort(matches, (size_t)match_count, sizeof(char *), cmp_str_ptr);

    /* Compute longest common prefix across all matches */
    size_t lcp = strlen(matches[0]);
    for (int i = 1; i < match_count; i++) {
        size_t j = 0;
        while (j < lcp && matches[0][j] != '\0' && matches[0][j] == matches[i][j]) {
            j++;
        }
        lcp = j;
    }

    if (lcp > flen) {
        size_t add_len = lcp - flen;
        if (*len + add_len < max_cap) {
            memmove(buf + *pos + add_len, buf + *pos, *len - *pos + 1);
            memcpy(buf + slash_offset, matches[0], lcp);
            *len += add_len;
            *pos += add_len;
        }
    }

    if (match_count > 1) {
        size_t used = (size_t)snprintf(hint_buf, hint_cap, "[");
        for (int i = 0; i < match_count && used + 4 < hint_cap; i++) {
            int n = snprintf(hint_buf + used, hint_cap - used, "%s%s",
                             (i > 0) ? " " : "", matches[i]);
            if (n < 0 || (size_t)n >= hint_cap - used) {
                used = hint_cap - 1;
                break;
            }
            used += (size_t)n;
        }
        if (used + 2 < hint_cap) {
            hint_buf[used++] = ']';
            hint_buf[used] = '\0';
        }
    }

    for (int i = 0; i < match_count; i++) {
        free(matches[i]);
    }
}

bool ui_prompt_string(Editor *ed, const char *prompt, char *out_buf, size_t out_len, const char *default_val) {
    if (ed == NULL || out_buf == NULL || out_len == 0) {
        return false;
    }

    size_t max_cap = (out_len < MAX_FILENAME) ? out_len : MAX_FILENAME;
    char buf[MAX_FILENAME];
    char saved_edit[MAX_FILENAME];
    char hint_buf[STATUS_LEN];
    hint_buf[0] = '\0';

    if (default_val != NULL) {
        snprintf(buf, max_cap, "%s", default_val);
    } else {
        buf[0] = '\0';
    }
    snprintf(saved_edit, sizeof(saved_edit), "%s", buf);

    size_t len = strlen(buf);
    size_t pos = len;
    int hist_idx = prompt_history_count;

    int y = ed->term_rows - 1;
    size_t prompt_len = (prompt != NULL) ? strlen(prompt) : 0;

    curs_set(1);
    timeout(-1);

    bool accepted = false;
    bool done = false;

    while (!done) {
        attr_t bar_attr = COLOR_PAIR(ui_pair_id(COLOR_BLACK, COLOR_CYAN)) | A_BOLD;
        attron(bar_attr);
        mvhline(y, 0, ' ', ed->term_cols);
        mvprintw(y, 0, "%s%s", prompt ? prompt : "", buf);
        if (hint_buf[0] != '\0') {
            int hint_col = (int)(prompt_len + len + 2);
            if (hint_col < ed->term_cols - 4) {
                int rem = ed->term_cols - hint_col - 1;
                mvprintw(y, hint_col, "%.*s", rem, hint_buf);
            }
        }
        attroff(bar_attr);

        int cursor_col = (int)(prompt_len + pos);
        if (cursor_col >= ed->term_cols) {
            cursor_col = ed->term_cols - 1;
        }
        if (cursor_col < 0) {
            cursor_col = 0;
        }
        move(y, cursor_col);
        refresh();

        int ch = wgetch(stdscr);
        if (ch != '\t') {
            hint_buf[0] = '\0';
        }

        if (ch == 27 || ch == CTRL('G')) {
            accepted = false;
            done = true;
        } else if (ch == '\r' || ch == '\n' || ch == KEY_ENTER) {
            if (len > 0) {
                snprintf(out_buf, out_len, "%s", buf);
                prompt_history_add(buf);
                accepted = true;
            }
            done = true;
        } else if (ch == KEY_LEFT || ch == CTRL('B')) {
            if (pos > 0) {
                pos--;
            }
        } else if (ch == KEY_RIGHT || ch == CTRL('F')) {
            if (pos < len) {
                pos++;
            }
        } else if (ch == KEY_HOME || ch == CTRL('A')) {
            pos = 0;
        } else if (ch == KEY_END || ch == CTRL('E')) {
            pos = len;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b' || ch == CTRL('H')) {
            if (pos > 0) {
                memmove(buf + pos - 1, buf + pos, len - pos + 1);
                pos--;
                len--;
            }
        } else if (ch == KEY_DC || ch == CTRL('D')) {
            if (pos < len) {
                memmove(buf + pos, buf + pos + 1, len - pos);
                len--;
            }
        } else if (ch == CTRL('K')) {
            if (pos < len) {
                snprintf(prompt_yank_buf, sizeof(prompt_yank_buf), "%s", buf + pos);
                buf[pos] = '\0';
                len = pos;
            }
        } else if (ch == CTRL('U')) {
            if (pos > 0) {
                memcpy(prompt_yank_buf, buf, pos);
                prompt_yank_buf[pos] = '\0';
                memmove(buf, buf + pos, len - pos + 1);
                len -= pos;
                pos = 0;
            }
        } else if (ch == CTRL('W')) {
            if (pos > 0) {
                size_t start = pos;
                while (start > 0 && (buf[start - 1] == ' ' || buf[start - 1] == '/')) {
                    start--;
                }
                while (start > 0 && buf[start - 1] != ' ' && buf[start - 1] != '/') {
                    start--;
                }
                size_t klen = pos - start;
                memcpy(prompt_yank_buf, buf + start, klen);
                prompt_yank_buf[klen] = '\0';
                memmove(buf + start, buf + pos, len - pos + 1);
                len -= klen;
                pos = start;
            }
        } else if (ch == CTRL('Y')) {
            size_t ylen = strlen(prompt_yank_buf);
            if (ylen > 0 && len < max_cap - 1) {
                if (len + ylen >= max_cap) {
                    ylen = (max_cap - 1) - len;
                }
                memmove(buf + pos + ylen, buf + pos, len - pos + 1);
                memcpy(buf + pos, prompt_yank_buf, ylen);
                len += ylen;
                pos += ylen;
            }
        } else if (ch == CTRL('T')) {
            if (len >= 2 && pos > 0) {
                size_t swap_idx = (pos == len) ? (pos - 1) : pos;
                char tmp = buf[swap_idx - 1];
                buf[swap_idx - 1] = buf[swap_idx];
                buf[swap_idx] = tmp;
                if (pos < len) {
                    pos++;
                }
            }
        } else if (ch == KEY_UP || ch == CTRL('P')) {
            if (hist_idx > 0) {
                if (hist_idx == prompt_history_count) {
                    snprintf(saved_edit, sizeof(saved_edit), "%s", buf);
                }
                hist_idx--;
                snprintf(buf, max_cap, "%s", prompt_history[hist_idx]);
                len = strlen(buf);
                pos = len;
            }
        } else if (ch == KEY_DOWN || ch == CTRL('N')) {
            if (hist_idx < prompt_history_count) {
                hist_idx++;
                if (hist_idx == prompt_history_count) {
                    snprintf(buf, max_cap, "%s", saved_edit);
                } else {
                    snprintf(buf, max_cap, "%s", prompt_history[hist_idx]);
                }
                len = strlen(buf);
                pos = len;
            }
        } else if (ch == '\t') {
            prompt_tab_complete(buf, &len, &pos, max_cap, hint_buf, sizeof(hint_buf));
        } else if (isprint(ch) && len + 1 < max_cap) {
            memmove(buf + pos + 1, buf + pos, len - pos + 1);
            buf[pos] = (char)ch;
            pos++;
            len++;
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

        /* Row 15: Edit & Save Operations */
        mvwprintw(win, 15, 2, "Edit & Save   :");
        mvwprintw(win, 15, 18, "[Undo(Z)]");
        mvwprintw(win, 15, 28, "[Redo(Y)]");
        mvwprintw(win, 15, 38, "[Clear(K)]");
        mvwprintw(win, 15, 49, "[Save(S)]");
        mvwprintw(win, 15, 59, "[Save As(A)]");

        /* Row 17: Load & Export & Close */
        mvwprintw(win, 17, 2, "Load & Cast   :");
        mvwprintw(win, 17, 18, "[Load(L)]");
        mvwprintw(win, 17, 28, "[Load .cast(I)]");
        mvwprintw(win, 17, 44, "[Export .cast(E)]");
        wattron(win, A_BOLD);
        mvwprintw(win, 17, 62, "[Close(Q)]");
        wattroff(win, A_BOLD);

        mvwprintw(win, 19, 2, "Active Files  : TXT='%s'%s | CAST='%s'",
                  ed->current_txt_file,
                  ed->txt_file_set ? "" : " (unsaved)",
                  ed->current_cast_file);

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
            fileio_save_txt_quick(ed);
            return;
        } else if (ch == 'a' || ch == 'A') {
            in_menu = false;
            delwin(win);
            touchwin(stdscr);
            fileio_prompt_save_as(ed);
            return;
        } else if (ch == 'l' || ch == 'L') {
            in_menu = false;
            delwin(win);
            touchwin(stdscr);
            fileio_prompt_load_txt(ed);
            return;
        } else if (ch == 'i' || ch == 'I') {
            in_menu = false;
            delwin(win);
            touchwin(stdscr);
            fileio_prompt_load_cast(ed);
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
                    } else if (rx >= 49 && rx <= 57) {
                        in_menu = false;
                        delwin(win);
                        touchwin(stdscr);
                        fileio_save_txt_quick(ed);
                        return;
                    } else if (rx >= 59 && rx <= 70) {
                        in_menu = false;
                        delwin(win);
                        touchwin(stdscr);
                        fileio_prompt_save_as(ed);
                        return;
                    }
                } else if (ry == 17) {
                    if (rx >= 18 && rx <= 26) {
                        in_menu = false;
                        delwin(win);
                        touchwin(stdscr);
                        fileio_prompt_load_txt(ed);
                        return;
                    } else if (rx >= 28 && rx <= 42) {
                        in_menu = false;
                        delwin(win);
                        touchwin(stdscr);
                        fileio_prompt_load_cast(ed);
                        return;
                    } else if (rx >= 44 && rx <= 60) {
                        in_menu = false;
                        delwin(win);
                        touchwin(stdscr);
                        fileio_prompt_export_cast(ed);
                        return;
                    } else if (rx >= 62 && rx <= 71) {
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
        fileio_save_txt_quick(ed);
    } else if (x >= 54 && x <= 59) {
        fileio_prompt_save_as(ed);
    } else if (x >= 60 && x <= 65) {
        fileio_prompt_export_cast(ed);
    } else {
        ui_open_menu(ed);
    }
}
