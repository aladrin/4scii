/* Tested on OpenBSD and Linux
 *
 * To compile on OpenBSD:
 * cc -O2 -pipe -c 4scii.c
 * cc -o 4scii 4scii.o -lcurses
 *
 * To compile on Linux:
 * gcc -O2 -pipe -c 4scii.c
 * gcc -o 4scii 4scii.o -lncurses
 */

#include <sys/types.h>
#include <curses.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

void domenubar (int input) {
  WINDOW * menubar = newwin(1, 16, 0, 0);
  wattron(menubar, A_STANDOUT);
  mvwprintw(menubar, 0, 0, " ");
  mvwprintw(menubar, 0, 1, "File");
  mvwprintw(menubar, 0, 5, " ");
  mvwprintw(menubar, 0, 6, "Edit");
  mvwprintw(menubar, 0, 10, " ");
  mvwprintw(menubar, 0, 11, "Help");
  mvwprintw(menubar, 0, 15, " ");
  keypad(menubar, true);
  do {
    input = wgetch(menubar);
    if(input == KEY_MOUSE) {
      getmouse(&mouseinput);
      y = mouseinput.y;
      x = mouseinput.x;
      if((x >= 0) && (x <= 4)) {
        wattron(menubar, A_STANDOUT);
        mvwprintw(menubar, 0, 0, " ");
        mvwprintw(menubar, 0, 1, "File");
        mvwprintw(menubar, 0, 5, " ");
        mvwprintw(menubar, 0, 6, "Edit");
        mvwprintw(menubar, 0, 10, " ");
        mvwprintw(menubar, 0, 11, "Help");
        mvwprintw(menubar, 0, 15, " ");
        wattroff(menubar, A_STANDOUT);
        mvwprintw(menubar, 0, 0, " ");
        mvwprintw(menubar, 0, 1, "File");
        mvwprintw(menubar, 0, 5, " ");
        wattron(menubar, A_STANDOUT);
      } else if((x >= 6) && (x <= 9)) {
        wattron(menubar, A_STANDOUT);
        mvwprintw(menubar, 0, 0, " ");
        mvwprintw(menubar, 0, 1, "File");
        mvwprintw(menubar, 0, 5, " ");
        mvwprintw(menubar, 0, 6, "Edit");
        mvwprintw(menubar, 0, 10, " ");
        mvwprintw(menubar, 0, 11, "Help");
        mvwprintw(menubar, 0, 15, " ");
        wattroff(menubar, A_STANDOUT); 
        mvwprintw(menubar, 0, 5, " ");
        mvwprintw(menubar, 0, 6, "Edit");
        mvwprintw(menubar, 0, 10, " ");
        wattron(menubar, A_STANDOUT);
      } else if((x >= 11) && (x <= 15)) {
        wattron(menubar, A_STANDOUT);
        mvwprintw(menubar, 0, 0, " ");
        mvwprintw(menubar, 0, 1, "File");
        mvwprintw(menubar, 0, 5, " ");
        mvwprintw(menubar, 0, 6, "Edit");
        mvwprintw(menubar, 0, 10, " ");
        mvwprintw(menubar, 0, 11, "Help");
        mvwprintw(menubar, 0, 15, " ");
        wattroff(menubar, A_STANDOUT);
        mvwprintw(menubar, 0, 10, " ");
        mvwprintw(menubar, 0, 11, "Help");
        mvwprintw(menubar, 0, 15, " ");
        wattron(menubar, A_STANDOUT);
      } 
    }
  } while(input != 27);
  wclear(menubar);
  wrefresh(menubar);
  delwin(menubar);
  refresh();
}

int main (int argc, char *argv[]) {
  int input, x, y;
  char character = '*';
  struct termios term;
  putenv("TERM=xterm-1002");
  initscr();
  raw();
  nodelay(stdscr,1);
  noecho();
  curs_set(0);
  nonl();
  keypad(stdscr,1);
  MEVENT mouseinput;
  mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
  int ymax = 0, xmax = 0, ymin, xmin;
  getmaxyx(stdscr, ymin, xmin);
  int canvasHeight = ymin + 1;
  int canvasWidth = xmin + 2;
  char canvas[canvasWidth][canvasHeight];
  for(y = 0; y < canvasHeight; y++) {
    for(x = 0; x < canvasWidth; x++) {
      canvas[y][x] = '\0';
    }
  }

  do {
    input = wgetch(stdscr);
    if(input == KEY_MOUSE) {
      getmouse(&mouseinput);
      y = mouseinput.y;
      x = mouseinput.x;
      canvas[y][x] = character;
      mvaddch(y, x, character);
      if(y > ymax) {
        ymax = y;
      }
      if(y < ymin) {
        ymin = y;
      }
      if(x > xmax) {
        xmax = x;
      }
      if(x < xmin) {
        xmin = x;
      }
      refresh();
    } else if(isprint(input)) {
      character = input;
    } else if(input == 27) {
      domenubar(input); 
    } else if(input == CTRL('Z')) {
    } else if(input == CTRL('Y')) {
    } else if(input == CTRL('S')) {
      endwin();
      character = '\0';
      char canvasout[1 + ymax - ymin][2 + xmax - xmin];
      for (y = 0; y < 1 + ymax - ymin; y++) {
        for (x = 0; x < 2 + xmax - xmin; x++) {
          canvasout[y][x] = character;
        }
      }
      FILE *ofp;
      ofp = fopen("test.file", "w");
      for(y = ymin; y <= ymax; y++) {
        for(x = xmax; x >= xmin; x--) {
          if(isprint(canvas[y][x])) {
            character = ' ';
            canvasout[y - ymin][x - xmin] = canvas[y][x];
          } else {
            canvasout[y - ymin][x - xmin] = character;
          }
        }
        fprintf(ofp, "%s\n", canvasout[y - ymin]);
        character = '\0';
      }
      fclose(ofp);
      exit(0);
    }
  } while(input != CTRL('Q'));
  endwin();
}
