#include "4scii.h"

static void trim_whitespace(char *str) {
    if (str == NULL) {
        return;
    }
    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[--len] = '\0';
    }
    size_t start = 0;
    while (str[start] != '\0' && isspace((unsigned char)str[start])) {
        start++;
    }
    if (start > 0) {
        memmove(str, str + start, len - start + 1);
    }
}

bool fileio_has_ext(const char *filename, const char *ext) {
    if (filename == NULL || ext == NULL) {
        return false;
    }
    size_t flen = strlen(filename);
    while (flen > 0 && isspace((unsigned char)filename[flen - 1])) {
        flen--;
    }
    size_t elen = strlen(ext);
    if (flen < elen) {
        return false;
    }
    const char *suffix = filename + (flen - elen);
    for (size_t i = 0; i < elen; i++) {
        if (tolower((unsigned char)suffix[i]) != tolower((unsigned char)ext[i])) {
            return false;
        }
    }
    return true;
}

void fileio_ensure_ext(char *filename, size_t max_len, const char *ext) {
    if (filename == NULL || ext == NULL || max_len == 0) {
        return;
    }

    trim_whitespace(filename);
    size_t len = strlen(filename);
    if (len == 0) {
        return;
    }

    if (fileio_has_ext(filename, ext)) {
        return;
    }

    size_t ext_len = strlen(ext);
    if (len + ext_len < max_len) {
        memcpy(filename + len, ext, ext_len + 1);
    }
}

static void apply_sgr_params(const char *params, short *fg, short *bg, bool *bold) {
    if (params == NULL || params[0] == '\0') {
        *fg = COLOR_WHITE;
        *bg = COLOR_BLACK;
        *bold = false;
        return;
    }

    const char *p = params;
    while (*p != '\0') {
        char *endp = NULL;
        long code = strtol(p, &endp, 10);
        if (endp == p) {
            code = 0;
        }

        if (code == 0) {
            *fg = COLOR_WHITE;
            *bg = COLOR_BLACK;
            *bold = false;
        } else if (code == 1) {
            *bold = true;
        } else if (code == 22) {
            *bold = false;
        } else if (code >= 30 && code <= 37) {
            *fg = (short)(code - 30);
        } else if (code == 39) {
            *fg = COLOR_WHITE;
        } else if (code >= 40 && code <= 47) {
            *bg = (short)(code - 40);
        } else if (code == 49) {
            *bg = COLOR_BLACK;
        } else if (code >= 90 && code <= 97) {
            *fg = (short)(code - 90);
            *bold = true;
        } else if (code >= 100 && code <= 107) {
            *bg = (short)(code - 100);
        }

        p = endp;
        if (*p == ';') {
            p++;
        } else if (*p != '\0') {
            break;
        }
    }
}

bool fileio_save_txt(Editor *ed, const char *raw_filename) {
    if (ed == NULL || ed->current_frame == NULL || raw_filename == NULL) {
        return false;
    }

    char path[MAX_FILENAME];
    snprintf(path, sizeof(path), "%s", raw_filename);
    fileio_ensure_ext(path, sizeof(path), ".txt");
    if (path[0] == '\0') {
        ui_set_status(ed, "Save cancelled: empty filename.");
        return false;
    }

    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        ui_set_status(ed, "Error saving '%s'", path);
        return false;
    }

    int ymin = 0, ymax = -1, xmin = 0, xmax = -1;
    if (frame_get_bounds(ed, ed->current_frame, &ymin, &ymax, &xmin, &xmax)) {
        for (int y = ymin; y <= ymax; y++) {
            int row_last_x = xmin - 1;
            for (int x = xmax; x >= xmin; x--) {
                Cell *c = frame_cell(ed, ed->current_frame, y, x);
                if (c != NULL && isprint((unsigned char)c->ch)) {
                    row_last_x = x;
                    break;
                }
            }

            short cur_fg = -1;
            short cur_bg = -1;
            int cur_bold = -1;
            bool ansi_active = false;

            for (int x = xmin; x <= row_last_x; x++) {
                Cell *c = frame_cell(ed, ed->current_frame, y, x);
                bool has_cell = (c != NULL && isprint((unsigned char)c->ch) &&
                                 (c->ch != ' ' || c->bg != COLOR_BLACK));
                if (has_cell) {
                    short fg = (c->fg >= 0 && c->fg < 8) ? c->fg : COLOR_WHITE;
                    short bg = (c->bg >= 0 && c->bg < 8) ? c->bg : COLOR_BLACK;
                    int bold = c->bold ? 1 : 0;

                    if (fg != cur_fg || bg != cur_bg || bold != cur_bold) {
                        if (bold) {
                            fprintf(fp, "\033[0;1;%d;%dm", 30 + fg, 40 + bg);
                        } else {
                            fprintf(fp, "\033[0;%d;%dm", 30 + fg, 40 + bg);
                        }
                        cur_fg = fg;
                        cur_bg = bg;
                        cur_bold = bold;
                        ansi_active = true;
                    }
                    fputc(c->ch, fp);
                } else {
                    if (ansi_active) {
                        fputs("\033[0m", fp);
                        cur_fg = -1;
                        cur_bg = -1;
                        cur_bold = -1;
                        ansi_active = false;
                    }
                    fputc(' ', fp);
                }
            }

            if (ansi_active) {
                fputs("\033[0m", fp);
            }
            fputc('\n', fp);
        }
    }

    fclose(fp);
    snprintf(ed->current_txt_file, sizeof(ed->current_txt_file), "%s", path);
    ed->txt_file_set = true;
    ui_set_status(ed, "Saved colored ASCII art to '%s'", path);
    return true;
}

bool fileio_load_txt(Editor *ed, const char *raw_filename) {
    if (ed == NULL || ed->current_frame == NULL || raw_filename == NULL) {
        return false;
    }

    char path[MAX_FILENAME];
    snprintf(path, sizeof(path), "%s", raw_filename);
    fileio_ensure_ext(path, sizeof(path), ".txt");
    if (path[0] == '\0') {
        ui_set_status(ed, "Load cancelled: empty filename.");
        return false;
    }

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        ui_set_status(ed, "Error: cannot open '%s'", path);
        return false;
    }

    canvas_push_undo(ed);
    frame_clear(ed, ed->current_frame);

    char line[8192];
    int y = 0;
    short cur_fg = COLOR_WHITE;
    short cur_bg = COLOR_BLACK;
    bool cur_bold = false;

    while (y < ed->canvas_rows && fgets(line, sizeof(line), fp) != NULL) {
        int x = 0;
        for (size_t i = 0; line[i] != '\0' && x < ed->canvas_cols; i++) {
            unsigned char uch = (unsigned char)line[i];
            if (uch == '\r' || uch == '\n') {
                break;
            }
            if (uch == '\t') {
                int next_tab = ((x / 8) + 1) * 8;
                x = (next_tab < ed->canvas_cols) ? next_tab : ed->canvas_cols;
                continue;
            }
            if (uch == 0x1b && line[i + 1] == '[') {
                i += 2;
                char params[64];
                size_t plen = 0;
                while (line[i] != '\0' &&
                       !((unsigned char)line[i] >= 0x40 && (unsigned char)line[i] <= 0x7e)) {
                    if (plen + 1 < sizeof(params)) {
                        params[plen++] = line[i];
                    }
                    i++;
                }
                params[plen] = '\0';
                char cmd = line[i];
                if (cmd == '\0') {
                    break;
                }
                if (cmd == 'm') {
                    apply_sgr_params(params, &cur_fg, &cur_bg, &cur_bold);
                } else if (cmd == 'C') {
                    int step = (plen > 0) ? atoi(params) : 1;
                    if (step < 1) step = 1;
                    x += step;
                }
                continue;
            }
            if (isprint(uch)) {
                if (uch != ' ' || cur_bg != COLOR_BLACK) {
                    Cell *c = frame_cell(ed, ed->current_frame, y, x);
                    if (c != NULL) {
                        c->ch = (char)uch;
                        c->fg = cur_fg;
                        c->bg = cur_bg;
                        c->bold = cur_bold;
                    }
                }
                x++;
            }
        }
        y++;
    }

    fclose(fp);
    snprintf(ed->current_txt_file, sizeof(ed->current_txt_file), "%s", path);
    ed->txt_file_set = true;
    ui_set_status(ed, "Loaded '%s' into frame %d/%d", path, ed->frame_index + 1, ed->frame_count);
    return true;
}

static void json_write_escaped_char(FILE *fp, char ch) {
    if (ch == '"') {
        fputs("\\\"", fp);
    } else if (ch == '\\') {
        fputs("\\\\", fp);
    } else if (isprint((unsigned char)ch)) {
        fputc(ch, fp);
    } else {
        fputc(' ', fp);
    }
}

bool fileio_export_cast(Editor *ed, const char *raw_filename) {
    if (ed == NULL || ed->first_frame == NULL || raw_filename == NULL) {
        return false;
    }

    char path[MAX_FILENAME];
    snprintf(path, sizeof(path), "%s", raw_filename);
    fileio_ensure_ext(path, sizeof(path), ".cast");
    if (path[0] == '\0') {
        ui_set_status(ed, "Export cancelled: empty filename.");
        return false;
    }

    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        ui_set_status(ed, "Error exporting '%s'", path);
        return false;
    }

    time_t now = time(NULL);
    fprintf(fp,
            "{\"version\": 2, \"width\": %d, \"height\": %d, \"timestamp\": %ld, "
            "\"env\": {\"TERM\": \"xterm-256color\", \"SHELL\": \"/bin/sh\"}}\n",
            ed->canvas_cols, ed->canvas_rows, (long)now);

    double elapsed = 0.0;
    int exported_frames = 0;

    for (Frame *f = ed->first_frame; f != NULL; f = f->next) {
        fprintf(fp, "[%.3f, \"o\", \"\\u001b[0m\\u001b[2J\\u001b[H", elapsed);

        short cur_fg = -1;
        short cur_bg = -1;
        int cur_bold = -1;

        for (int y = 0; y < ed->canvas_rows; y++) {
            int row_last_x = -1;
            for (int x = ed->canvas_cols - 1; x >= 0; x--) {
                Cell *c = frame_cell(ed, f, y, x);
                if (c != NULL && isprint((unsigned char)c->ch)) {
                    row_last_x = x;
                    break;
                }
            }

            if (row_last_x < 0) {
                continue;
            }

            fprintf(fp, "\\u001b[%d;1H", y + 1);

            for (int x = 0; x <= row_last_x; x++) {
                Cell *c = frame_cell(ed, f, y, x);
                if (c != NULL && isprint((unsigned char)c->ch) &&
                    (c->ch != ' ' || c->bg != COLOR_BLACK)) {
                    short fg = (c->fg >= 0 && c->fg < 8) ? c->fg : COLOR_WHITE;
                    short bg = (c->bg >= 0 && c->bg < 8) ? c->bg : COLOR_BLACK;
                    int bold = c->bold ? 1 : 0;

                    if (fg != cur_fg || bg != cur_bg || bold != cur_bold) {
                        if (bold) {
                            fprintf(fp, "\\u001b[0;1;%d;%dm", 30 + fg, 40 + bg);
                        } else {
                            fprintf(fp, "\\u001b[0;%d;%dm", 30 + fg, 40 + bg);
                        }
                        cur_fg = fg;
                        cur_bg = bg;
                        cur_bold = bold;
                    }
                    json_write_escaped_char(fp, c->ch);
                } else {
                    if (cur_fg != COLOR_WHITE || cur_bg != COLOR_BLACK || cur_bold != 0) {
                        fputs("\\u001b[0m", fp);
                        cur_fg = COLOR_WHITE;
                        cur_bg = COLOR_BLACK;
                        cur_bold = 0;
                    }
                    fputc(' ', fp);
                }
            }
        }

        fputs("\\u001b[0m\"]\n", fp);

        int delay = (f->delay_ms >= MIN_FRAME_DELAY_MS) ? f->delay_ms : DEFAULT_FRAME_DELAY_MS;
        elapsed += (double)delay / 1000.0;
        exported_frames++;
    }

    /* Emit final trailing event so asciinema players hold the last frame */
    fprintf(fp, "[%.3f, \"o\", \"\"]\n", elapsed);

    fclose(fp);
    snprintf(ed->current_cast_file, sizeof(ed->current_cast_file), "%s", path);
    ui_set_status(ed, "Exported %d frame(s) to asciinema '%s'", exported_frames, path);
    return true;
}

static bool read_line_alloc(FILE *fp, char **buf, size_t *cap) {
    if (fp == NULL || buf == NULL || cap == NULL) {
        return false;
    }
    if (*buf == NULL || *cap == 0) {
        *cap = 4096;
        *buf = malloc(*cap);
        if (*buf == NULL) {
            err(1, "malloc");
        }
    }

    size_t len = 0;
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (len + 2 >= *cap) {
            *cap *= 2;
            char *nb = realloc(*buf, *cap);
            if (nb == NULL) {
                err(1, "realloc");
            }
            *buf = nb;
        }
        (*buf)[len++] = (char)ch;
        if (ch == '\n') {
            break;
        }
    }

    if (len == 0 && ch == EOF) {
        return false;
    }
    (*buf)[len] = '\0';
    return true;
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static size_t json_unescape_string(const char *src, char *dst, size_t dst_cap) {
    size_t out = 0;
    for (size_t i = 0; src[i] != '\0' && out + 1 < dst_cap; i++) {
        if (src[i] == '"') {
            break;
        }
        if (src[i] == '\\') {
            i++;
            char esc = src[i];
            if (esc == '\0') {
                break;
            }
            if (esc == '"') dst[out++] = '"';
            else if (esc == '\\') dst[out++] = '\\';
            else if (esc == '/') dst[out++] = '/';
            else if (esc == 'b') dst[out++] = '\b';
            else if (esc == 'f') dst[out++] = '\f';
            else if (esc == 'n') dst[out++] = '\n';
            else if (esc == 'r') dst[out++] = '\r';
            else if (esc == 't') dst[out++] = '\t';
            else if (esc == 'u') {
                int h1 = hex_val(src[i + 1]);
                int h2 = hex_val(src[i + 2]);
                int h3 = hex_val(src[i + 3]);
                int h4 = hex_val(src[i + 4]);
                if (h1 >= 0 && h2 >= 0 && h3 >= 0 && h4 >= 0) {
                    int code = (h1 << 12) | (h2 << 8) | (h3 << 4) | h4;
                    if (code > 0 && code < 128) {
                        dst[out++] = (char)code;
                    }
                    i += 4;
                }
            } else {
                dst[out++] = esc;
            }
        } else {
            dst[out++] = src[i];
        }
    }
    dst[out] = '\0';
    return out;
}

static void replay_ansi_into_frame(Editor *ed, Frame *f, const char *data, size_t len) {
    int cy = 0;
    int cx = 0;
    short cur_fg = COLOR_WHITE;
    short cur_bg = COLOR_BLACK;
    bool cur_bold = false;

    for (size_t i = 0; i < len; i++) {
        unsigned char uch = (unsigned char)data[i];
        if (uch == '\r') {
            cx = 0;
        } else if (uch == '\n') {
            if (cy + 1 < ed->canvas_rows) {
                cy++;
            }
        } else if (uch == '\b') {
            if (cx > 0) {
                cx--;
            }
        } else if (uch == '\t') {
            int next_tab = ((cx / 8) + 1) * 8;
            cx = (next_tab < ed->canvas_cols) ? next_tab : (ed->canvas_cols - 1);
        } else if (uch == 0x1b && i + 1 < len && data[i + 1] == '[') {
            i += 2;
            char params[64];
            size_t plen = 0;
            while (i < len && !((unsigned char)data[i] >= 0x40 && (unsigned char)data[i] <= 0x7e)) {
                if (plen + 1 < sizeof(params)) {
                    params[plen++] = data[i];
                }
                i++;
            }
            params[plen] = '\0';
            if (i >= len) {
                break;
            }
            char cmd = data[i];
            if (cmd == 'm') {
                apply_sgr_params(params, &cur_fg, &cur_bg, &cur_bold);
            } else if (cmd == 'H' || cmd == 'f') {
                int row = 1, col = 1;
                if (plen > 0) {
                    const char *semi = strchr(params, ';');
                    if (semi != NULL) {
                        row = atoi(params);
                        col = atoi(semi + 1);
                    } else {
                        row = atoi(params);
                    }
                }
                if (row < 1) row = 1;
                if (col < 1) col = 1;
                cy = (row - 1 < ed->canvas_rows) ? (row - 1) : (ed->canvas_rows - 1);
                cx = (col - 1 < ed->canvas_cols) ? (col - 1) : (ed->canvas_cols - 1);
            } else if (cmd == 'J') {
                int mode = (plen > 0) ? atoi(params) : 0;
                if (mode == 2) {
                    frame_clear(ed, f);
                }
            } else if (cmd == 'K') {
                int mode = (plen > 0) ? atoi(params) : 0;
                if (mode == 0 && cy >= 0 && cy < ed->canvas_rows) {
                    for (int x = cx; x < ed->canvas_cols; x++) {
                        Cell *c = frame_cell(ed, f, cy, x);
                        if (c != NULL) {
                            c->ch = '\0';
                            c->fg = COLOR_WHITE;
                            c->bg = COLOR_BLACK;
                            c->bold = false;
                        }
                    }
                }
            } else if (cmd == 'A') {
                int n = (plen > 0) ? atoi(params) : 1;
                if (n < 1) n = 1;
                cy = (cy - n >= 0) ? (cy - n) : 0;
            } else if (cmd == 'B') {
                int n = (plen > 0) ? atoi(params) : 1;
                if (n < 1) n = 1;
                cy = (cy + n < ed->canvas_rows) ? (cy + n) : (ed->canvas_rows - 1);
            } else if (cmd == 'C') {
                int n = (plen > 0) ? atoi(params) : 1;
                if (n < 1) n = 1;
                cx = (cx + n < ed->canvas_cols) ? (cx + n) : (ed->canvas_cols - 1);
            } else if (cmd == 'D') {
                int n = (plen > 0) ? atoi(params) : 1;
                if (n < 1) n = 1;
                cx = (cx - n >= 0) ? (cx - n) : 0;
            } else if (cmd == 'G') {
                int col = (plen > 0) ? atoi(params) : 1;
                if (col < 1) col = 1;
                cx = (col - 1 < ed->canvas_cols) ? (col - 1) : (ed->canvas_cols - 1);
            }
        } else if (isprint(uch)) {
            if (cy >= 0 && cy < ed->canvas_rows && cx >= 0 && cx < ed->canvas_cols) {
                Cell *c = frame_cell(ed, f, cy, cx);
                if (c != NULL) {
                    if (uch == ' ' && cur_bg == COLOR_BLACK) {
                        c->ch = '\0';
                        c->fg = COLOR_WHITE;
                        c->bg = COLOR_BLACK;
                        c->bold = false;
                    } else {
                        c->ch = (char)uch;
                        c->fg = cur_fg;
                        c->bg = cur_bg;
                        c->bold = cur_bold;
                    }
                }
            }
            cx++;
        }
    }
}

bool fileio_load_cast(Editor *ed, const char *raw_filename) {
    if (ed == NULL || raw_filename == NULL) {
        return false;
    }

    char path[MAX_FILENAME];
    snprintf(path, sizeof(path), "%s", raw_filename);
    fileio_ensure_ext(path, sizeof(path), ".cast");
    if (path[0] == '\0') {
        ui_set_status(ed, "Load cancelled: empty filename.");
        return false;
    }

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        ui_set_status(ed, "Error: cannot open '%s'", path);
        return false;
    }

    char *line = NULL;
    size_t line_cap = 0;

    /* Read header line */
    if (!read_line_alloc(fp, &line, &line_cap)) {
        free(line);
        fclose(fp);
        ui_set_status(ed, "Error: empty .cast file '%s'", path);
        return false;
    }

    Frame *first = NULL;
    Frame *last = NULL;
    double prev_ts = 0.0;
    int loaded_count = 0;

    while (read_line_alloc(fp, &line, &line_cap)) {
        const char *p = line;
        while (*p != '\0' && isspace((unsigned char)*p)) {
            p++;
        }
        if (*p != '[') {
            continue;
        }
        p++;

        char *endp = NULL;
        double ts = strtod(p, &endp);
        if (endp == p) {
            continue;
        }
        p = endp;

        /* Locate first quoted string (event type, e.g. "o") */
        const char *q1 = strchr(p, '"');
        if (q1 == NULL) {
            continue;
        }
        const char *q2 = strchr(q1 + 1, '"');
        if (q2 == NULL) {
            continue;
        }
        if ((q2 - (q1 + 1)) != 1 || q1[1] != 'o') {
            continue;
        }

        /* Locate second quoted string (payload) */
        const char *q3 = strchr(q2 + 1, '"');
        if (q3 == NULL) {
            continue;
        }

        size_t max_decoded = strlen(q3 + 1) + 1;
        char *decoded = malloc(max_decoded);
        if (decoded == NULL) {
            err(1, "malloc");
        }
        size_t decoded_len = json_unescape_string(q3 + 1, decoded, max_decoded);

        /* Compute previous frame's delay from timestamp delta */
        if (last != NULL) {
            int delta_ms = (int)((ts - prev_ts) * 1000.0 + 0.5);
            if (delta_ms < MIN_FRAME_DELAY_MS) delta_ms = MIN_FRAME_DELAY_MS;
            if (delta_ms > MAX_FRAME_DELAY_MS) delta_ms = MAX_FRAME_DELAY_MS;
            last->delay_ms = delta_ms;
        }

        /* Trailing empty event is used only to finalize the last frame's delay */
        if (decoded_len == 0) {
            free(decoded);
            prev_ts = ts;
            continue;
        }

        Frame *nf = frame_create(ed->canvas_rows, ed->canvas_cols, DEFAULT_FRAME_DELAY_MS);
        if (last != NULL) {
            int total = ed->canvas_rows * ed->canvas_cols;
            memcpy(nf->cells, last->cells, (size_t)total * sizeof(Cell));
        }

        replay_ansi_into_frame(ed, nf, decoded, decoded_len);
        free(decoded);

        if (first == NULL) {
            first = nf;
            last = nf;
        } else {
            nf->prev = last;
            last->next = nf;
            last = nf;
        }
        prev_ts = ts;
        loaded_count++;
    }

    free(line);
    fclose(fp);

    if (first == NULL) {
        ui_set_status(ed, "No animation frames found in '%s'", path);
        return false;
    }

    Frame *curr = ed->first_frame;
    while (curr != NULL) {
        Frame *next = curr->next;
        frame_free(curr);
        curr = next;
    }

    ed->first_frame = first;
    ed->current_frame = first;
    ed->frame_index = 0;
    ed->frame_count = loaded_count;
    snprintf(ed->current_cast_file, sizeof(ed->current_cast_file), "%s", path);
    ui_set_status(ed, "Loaded %d frame(s) from '%s'", loaded_count, path);
    return true;
}

bool fileio_load_auto(Editor *ed, const char *raw_filename) {
    if (ed == NULL || raw_filename == NULL) {
        return false;
    }
    if (fileio_has_ext(raw_filename, ".cast")) {
        return fileio_load_cast(ed, raw_filename);
    }
    return fileio_load_txt(ed, raw_filename);
}

void fileio_save_txt_quick(Editor *ed) {
    if (ed == NULL) {
        return;
    }
    if (ed->txt_file_set && ed->current_txt_file[0] != '\0') {
        fileio_save_txt(ed, ed->current_txt_file);
        ui_render(ed);
    } else {
        fileio_prompt_save_txt(ed);
    }
}

void fileio_prompt_save_txt(Editor *ed) {
    char filename[MAX_FILENAME];
    if (ui_prompt_string(ed, "Save colored ASCII (.txt): ", filename, sizeof(filename), ed->current_txt_file)) {
        fileio_save_txt(ed, filename);
    } else {
        ui_set_status(ed, "Save cancelled.");
    }
    ui_render(ed);
}

void fileio_prompt_save_as(Editor *ed) {
    char filename[MAX_FILENAME];
    if (ui_prompt_string(ed, "Save As (.txt or .cast): ", filename, sizeof(filename), ed->current_txt_file)) {
        if (fileio_has_ext(filename, ".cast")) {
            fileio_export_cast(ed, filename);
        } else {
            fileio_save_txt(ed, filename);
        }
    } else {
        ui_set_status(ed, "Save As cancelled.");
    }
    ui_render(ed);
}

void fileio_prompt_load_txt(Editor *ed) {
    char filename[MAX_FILENAME];
    if (ui_prompt_string(ed, "Load file (.txt or .cast): ", filename, sizeof(filename), ed->current_txt_file)) {
        fileio_load_auto(ed, filename);
    } else {
        ui_set_status(ed, "Load cancelled.");
    }
    ui_render(ed);
}

void fileio_prompt_export_cast(Editor *ed) {
    char filename[MAX_FILENAME];
    if (ui_prompt_string(ed, "Export asciinema (.cast): ", filename, sizeof(filename), ed->current_cast_file)) {
        fileio_export_cast(ed, filename);
    } else {
        ui_set_status(ed, "Export cancelled.");
    }
    ui_render(ed);
}

void fileio_prompt_load_cast(Editor *ed) {
    char filename[MAX_FILENAME];
    if (ui_prompt_string(ed, "Load asciinema (.cast): ", filename, sizeof(filename), ed->current_cast_file)) {
        fileio_load_cast(ed, filename);
    } else {
        ui_set_status(ed, "Load .cast cancelled.");
    }
    ui_render(ed);
}
