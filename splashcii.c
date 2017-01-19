/*	$OpenBSD: splashcii.c,v 1.20 2013/02/05 06:40:14 aladrin Exp $	*/

/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Tested on OpenBSD and Linux
 *
 * To compile on OpenBSD:
 * cc -O2 -pipe -c splashcii.c
 * cc -o splashcii splashcii.o -lcurses
 *
 * To compile on Linux:
 * gcc -O2 -pipe -c splashcii.c
 * gcc -o splashcii splashcii.o -lncurses
 */

#include <sys/types.h>
#include <curses.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

int
main(int argc, char *argv[])
{
	int CO, LI, top, bottom, left, right, input, x, y;
	char character = '*';
	struct termios term;

	initscr();
	raw();
	nodelay(stdscr,1);
	noecho();
	curs_set(0);
	nonl();
	keypad(stdscr,1);
	MEVENT mouseinput;
	mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);

	CO = COLS;
	LI = LINES;
	top = 0;
	bottom = LI - 1;
	left = 0;
	right = CO - 1;
	int ymax = 0;
	int xmax = 0;
	int xmin = right;
	int ymin = bottom;
	char canvas[LI][CO+1];
	for(y = 0; y < LI; y++) {
		for(x = 0; x < CO+1; x++) {
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
