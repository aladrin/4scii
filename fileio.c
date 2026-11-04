#include "4scii.h"

void fileio_ensure_ext(char *filename, size_t max_len, const char *ext) {
    if (filename == NULL || ext == NULL || max_len == 0) {
        return;
    }

    /* Strip leading/trailing whitespace */
    size_t len = strlen(filename);
    while (len > 0 && isspace((unsigned char)filename[len - 1])) {
        filename[--len] = '\0';
    }
    size_t start = 0;
    while (filename[start] != '\0' && isspace((unsigned char)filename[start])) {
        start++;
    }
    if (start > 0) {
        memmove(filename, filename + start, len - start + 1);
        len -= start;
    }

    if (len == 0) {
        return;
    }

    size_t ext_len = strlen(ext);
    if (len >= ext_len && strcmp(filename + len - ext_len, ext) == 0) {
        return;
    }

    if (len + ext_len < max_len) {
        memcpy(filename + len, ext, ext_len + 1);
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
            for (int x = xmin; x <= row_last_x; x++) {
                Cell *c = frame_cell(ed, ed->current_frame, y, x);
                char out_ch = (c != NULL && isprint((unsigned char)c->ch)) ? c->ch : ' ';
                fputc(out_ch, fp);
            }
            fputc('\n', fp);
        }
    }

    fclose(fp);
    snprintf(ed->current_txt_file, sizeof(ed->current_txt_file), "%s", path);
    ui_set_status(ed, "Saved static ASCII art to '%s'", path);
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

    char line[2048];
    int y = 0;
    while (y < ed->canvas_rows && fgets(line, sizeof(line), fp) != NULL) {
        int x = 0;
        for (size_t i = 0; line[i] != '\0' && x < ed->canvas_cols; i++) {
            char ch = line[i];
            if (ch == '\r' || ch == '\n') {
                break;
            }
            if (ch == '\t') {
                int next_tab = ((x / 8) + 1) * 8;
                x = (next_tab < ed->canvas_cols) ? next_tab : ed->canvas_cols;
                continue;
            }
            if (isprint((unsigned char)ch) && ch != ' ') {
                Cell *c = frame_cell(ed, ed->current_frame, y, x);
                if (c != NULL) {
                    c->ch = ch;
                    c->fg = ed->fg;
                    c->bg = ed->bg;
                    c->bold = ed->bold;
                }
            }
            x++;
        }
        y++;
    }

    fclose(fp);
    snprintf(ed->current_txt_file, sizeof(ed->current_txt_file), "%s", path);
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
                if (c != NULL && isprint((unsigned char)c->ch)) {
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

void fileio_prompt_save_txt(Editor *ed) {
    char filename[MAX_FILENAME];
    if (ui_prompt_string(ed, "Save static ASCII (.txt): ", filename, sizeof(filename), ed->current_txt_file)) {
        fileio_save_txt(ed, filename);
    } else {
        ui_set_status(ed, "Save cancelled.");
    }
    ui_render(ed);
}

void fileio_prompt_load_txt(Editor *ed) {
    char filename[MAX_FILENAME];
    if (ui_prompt_string(ed, "Load static ASCII (.txt): ", filename, sizeof(filename), ed->current_txt_file)) {
        fileio_load_txt(ed, filename);
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
