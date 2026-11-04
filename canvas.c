#include "4scii.h"

static void undo_stack_clear(UndoNode **head, int *depth) {
    UndoNode *curr = *head;
    while (curr != NULL) {
        UndoNode *next = curr->next;
        free(curr->cells);
        free(curr);
        curr = next;
    }
    *head = NULL;
    *depth = 0;
}

static Cell *cells_clone(const Cell *src, int total_cells) {
    Cell *copy = calloc((size_t)total_cells, sizeof(Cell));
    if (copy == NULL) {
        err(1, "calloc");
    }
    memcpy(copy, src, (size_t)total_cells * sizeof(Cell));
    return copy;
}

Frame *frame_create(int rows, int cols, int delay_ms) {
    Frame *f = calloc(1, sizeof(Frame));
    if (f == NULL) {
        err(1, "calloc");
    }
    int total = rows * cols;
    f->cells = calloc((size_t)total, sizeof(Cell));
    if (f->cells == NULL) {
        err(1, "calloc");
    }
    for (int i = 0; i < total; i++) {
        f->cells[i].ch = '\0';
        f->cells[i].fg = COLOR_WHITE;
        f->cells[i].bg = COLOR_BLACK;
        f->cells[i].bold = false;
    }
    f->delay_ms = delay_ms;
    f->undo_stack = NULL;
    f->undo_depth = 0;
    f->redo_stack = NULL;
    f->redo_depth = 0;
    f->prev = NULL;
    f->next = NULL;
    return f;
}

void frame_free(Frame *f) {
    if (f == NULL) {
        return;
    }
    undo_stack_clear(&f->undo_stack, &f->undo_depth);
    undo_stack_clear(&f->redo_stack, &f->redo_depth);
    free(f->cells);
    free(f);
}

void frame_clear(Editor *ed, Frame *f) {
    if (ed == NULL || f == NULL) {
        return;
    }
    int total = ed->canvas_rows * ed->canvas_cols;
    for (int i = 0; i < total; i++) {
        f->cells[i].ch = '\0';
        f->cells[i].fg = COLOR_WHITE;
        f->cells[i].bg = COLOR_BLACK;
        f->cells[i].bold = false;
    }
}

void editor_init(Editor *ed, int term_rows, int term_cols) {
    memset(ed, 0, sizeof(Editor));
    ed->term_rows = term_rows;
    ed->term_cols = term_cols;
    ed->canvas_rows = (term_rows > 1) ? (term_rows - 1) : 1;
    ed->canvas_cols = (term_cols > 0) ? term_cols : 1;
    ed->cursor_y = ed->canvas_rows / 2;
    ed->cursor_x = ed->canvas_cols / 2;
    ed->brush = '*';
    ed->fg = COLOR_WHITE;
    ed->bg = COLOR_BLACK;
    ed->bold = false;
    ed->draw_mode = MODE_DRAG;
    ed->mouse_down = false;
    ed->stroke_active = false;
    ed->onion_skin = false;
    ed->running = true;
    ed->txt_file_set = false;

    snprintf(ed->current_txt_file, sizeof(ed->current_txt_file), "4scii.txt");
    snprintf(ed->current_cast_file, sizeof(ed->current_cast_file), "4scii.cast");
    snprintf(ed->status_msg, sizeof(ed->status_msg),
             "F2:Menu | Ctrl+S:Save | Ctrl+A:SaveAs | Ctrl+O:Load | Ctrl+E:Cast | Ctrl+P:Play | Ctrl+Q:Quit");

    ed->first_frame = frame_create(ed->canvas_rows, ed->canvas_cols, DEFAULT_FRAME_DELAY_MS);
    ed->current_frame = ed->first_frame;
    ed->frame_index = 0;
    ed->frame_count = 1;
}

void editor_free(Editor *ed) {
    if (ed == NULL) {
        return;
    }
    Frame *curr = ed->first_frame;
    while (curr != NULL) {
        Frame *next = curr->next;
        frame_free(curr);
        curr = next;
    }
    ed->first_frame = NULL;
    ed->current_frame = NULL;
    ed->frame_count = 0;
    ed->frame_index = 0;
}

Cell *frame_cell(Editor *ed, Frame *f, int y, int x) {
    if (ed == NULL || f == NULL) {
        return NULL;
    }
    if (y < 0 || y >= ed->canvas_rows || x < 0 || x >= ed->canvas_cols) {
        return NULL;
    }
    return &f->cells[y * ed->canvas_cols + x];
}

void canvas_push_undo(Editor *ed) {
    if (ed == NULL || ed->current_frame == NULL) {
        return;
    }
    Frame *f = ed->current_frame;
    int total = ed->canvas_rows * ed->canvas_cols;

    UndoNode *node = calloc(1, sizeof(UndoNode));
    if (node == NULL) {
        err(1, "calloc");
    }
    node->cells = cells_clone(f->cells, total);
    node->next = f->undo_stack;
    f->undo_stack = node;
    f->undo_depth++;

    if (f->undo_depth > MAX_UNDO) {
        UndoNode *prev = NULL;
        UndoNode *curr = f->undo_stack;
        while (curr != NULL && curr->next != NULL) {
            prev = curr;
            curr = curr->next;
        }
        if (prev != NULL && curr != NULL) {
            prev->next = NULL;
            free(curr->cells);
            free(curr);
            f->undo_depth--;
        }
    }

    undo_stack_clear(&f->redo_stack, &f->redo_depth);
}

bool canvas_undo(Editor *ed) {
    if (ed == NULL || ed->current_frame == NULL) {
        return false;
    }
    Frame *f = ed->current_frame;
    if (f->undo_stack == NULL) {
        return false;
    }
    int total = ed->canvas_rows * ed->canvas_cols;

    UndoNode *redo_node = calloc(1, sizeof(UndoNode));
    if (redo_node == NULL) {
        err(1, "calloc");
    }
    redo_node->cells = f->cells;
    redo_node->next = f->redo_stack;
    f->redo_stack = redo_node;
    f->redo_depth++;

    UndoNode *undo_node = f->undo_stack;
    f->undo_stack = undo_node->next;
    f->undo_depth--;

    f->cells = cells_clone(undo_node->cells, total);
    free(undo_node->cells);
    free(undo_node);
    return true;
}

bool canvas_redo(Editor *ed) {
    if (ed == NULL || ed->current_frame == NULL) {
        return false;
    }
    Frame *f = ed->current_frame;
    if (f->redo_stack == NULL) {
        return false;
    }
    int total = ed->canvas_rows * ed->canvas_cols;

    UndoNode *undo_node = calloc(1, sizeof(UndoNode));
    if (undo_node == NULL) {
        err(1, "calloc");
    }
    undo_node->cells = f->cells;
    undo_node->next = f->undo_stack;
    f->undo_stack = undo_node;
    f->undo_depth++;

    UndoNode *redo_node = f->redo_stack;
    f->redo_stack = redo_node->next;
    f->redo_depth--;

    f->cells = cells_clone(redo_node->cells, total);
    free(redo_node->cells);
    free(redo_node);
    return true;
}

void canvas_put_cell(Editor *ed, int y, int x) {
    Cell *c = frame_cell(ed, ed->current_frame, y, x);
    if (c == NULL) {
        return;
    }
    c->ch = ed->brush;
    c->fg = ed->fg;
    c->bg = ed->bg;
    c->bold = ed->bold;
}

void canvas_erase_cell(Editor *ed, int y, int x) {
    Cell *c = frame_cell(ed, ed->current_frame, y, x);
    if (c == NULL) {
        return;
    }
    c->ch = '\0';
    c->fg = COLOR_WHITE;
    c->bg = COLOR_BLACK;
    c->bold = false;
}

void frame_new_after(Editor *ed, bool clone_current) {
    if (ed == NULL || ed->current_frame == NULL) {
        return;
    }
    Frame *curr = ed->current_frame;
    Frame *nf = frame_create(ed->canvas_rows, ed->canvas_cols, curr->delay_ms);

    if (clone_current) {
        int total = ed->canvas_rows * ed->canvas_cols;
        memcpy(nf->cells, curr->cells, (size_t)total * sizeof(Cell));
    }

    nf->prev = curr;
    nf->next = curr->next;
    if (curr->next != NULL) {
        curr->next->prev = nf;
    }
    curr->next = nf;

    ed->current_frame = nf;
    ed->frame_index++;
    ed->frame_count++;
}

void frame_delete_current(Editor *ed) {
    if (ed == NULL || ed->current_frame == NULL) {
        return;
    }
    if (ed->frame_count <= 1) {
        canvas_push_undo(ed);
        frame_clear(ed, ed->current_frame);
        return;
    }

    Frame *curr = ed->current_frame;
    if (curr->prev != NULL) {
        curr->prev->next = curr->next;
    } else {
        ed->first_frame = curr->next;
    }

    if (curr->next != NULL) {
        curr->next->prev = curr->prev;
        ed->current_frame = curr->next;
    } else {
        ed->current_frame = curr->prev;
        ed->frame_index--;
    }

    frame_free(curr);
    ed->frame_count--;
}

void frame_next(Editor *ed) {
    if (ed != NULL && ed->current_frame != NULL && ed->current_frame->next != NULL) {
        ed->current_frame = ed->current_frame->next;
        ed->frame_index++;
    }
}

void frame_prev(Editor *ed) {
    if (ed != NULL && ed->current_frame != NULL && ed->current_frame->prev != NULL) {
        ed->current_frame = ed->current_frame->prev;
        ed->frame_index--;
    }
}

bool frame_get_bounds(Editor *ed, Frame *f, int *ymin, int *ymax, int *xmin, int *xmax) {
    if (ed == NULL || f == NULL) {
        return false;
    }
    int min_y = ed->canvas_rows;
    int max_y = -1;
    int min_x = ed->canvas_cols;
    int max_x = -1;
    bool found = false;

    for (int y = 0; y < ed->canvas_rows; y++) {
        for (int x = 0; x < ed->canvas_cols; x++) {
            Cell *c = frame_cell(ed, f, y, x);
            if (c != NULL && c->ch != '\0') {
                found = true;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
            }
        }
    }

    if (found) {
        if (ymin) *ymin = min_y;
        if (ymax) *ymax = max_y;
        if (xmin) *xmin = min_x;
        if (xmax) *xmax = max_x;
    }
    return found;
}
