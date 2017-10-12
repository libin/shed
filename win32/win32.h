/*
$Id: win32.h,v 1.4 2011/06/20 00:41:27 alexsisson Exp $

(C) Copyright 2011 Alex Sisson (alexsisson@gmail.com)

This file is part of shed.

shed is free software; you can redistribute it and/or modify
under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

shed is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with shed; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

===============================================================

This is essentially a bespoke implementation of a tiny subset of curses,
providing just enough features to get shed working in a Windows window with-
out major changes to the shed code itself.

I've kept it non-shed specific to allow for potential reuse, and possible
expansion to be more curses-compliant. However this may pose some challenges
(trying to implement non-event driven curses into the event-driven world of
win32)

*/

#include <Windows.h>

#define OK				0
#define ERR				(-1)

/* map win32 key constants to ncurses constants */
#define KEY_NUDGE		0xff   /* nudge special characters up above the 0xff so they don't clash with chars */
#define KEY_MOUSE		0xffff

#define KEY_UP			(KEY_NUDGE + VK_UP)
#define KEY_DOWN		(KEY_NUDGE + VK_DOWN)
#define KEY_LEFT		(KEY_NUDGE + VK_LEFT)
#define KEY_RIGHT		(KEY_NUDGE + VK_RIGHT)
#define KEY_PPAGE		(KEY_NUDGE + VK_PRIOR)
#define KEY_NPAGE		(KEY_NUDGE + VK_NEXT)
#define KEY_BACKSPACE	(KEY_NUDGE + VK_BACK)
#define KEY_HOME		(KEY_NUDGE + VK_HOME)
#define KEY_END			(KEY_NUDGE + VK_END)
#define KEY_F(N)		(KEY_NUDGE + VK_F1 + ((N)-1))
#define KEY_RESIZE		9000

#define COLOR_PAIR(N)	(N)
#define A_REVERSE		20

#define COLOR_BLACK   0
#define COLOR_RED     1
#define COLOR_GREEN   2
#define COLOR_YELLOW  3
#define COLOR_BLUE    4
#define COLOR_MAGENTA 5
#define COLOR_CYAN    6
#define COLOR_WHITE   7

#define BUTTON1_PRESSED 1

extern int COLS;
extern int LINES;

struct window {
	int _curx, _cury;
};
typedef struct window WINDOW;
extern WINDOW *stdscr;

struct mevent {
	int x, y;
	unsigned long bstate;
};
typedef struct mevent MEVENT;

/* functions related to our crude curses "implementation" */
void sc_set_hwnd(HWND hwnd);
void sc_set_hdc(HDC hdc);
int peek(void);
int ungetchvk(WPARAM vk);

/* curses functions */

WINDOW *initscr();

int start_color(void);
int init_pair(short pair, short f, short b);

int getch(void);
int ungetch(int ch);

int getmouse(MEVENT *event);
int ungetmouse(MEVENT *event);

#define getyx(W,Y,X)	(Y)=(W)->_cury;(X)=(W)->_curx;

int move(int y, int x);
int beep(void);
int refresh(void);
int erase(void);

int attron(int attr);
int attroff(int attr);

int curs_set(int);

int addch(int ch);
int mvaddch(int y, int x, int ch);

int addstr(const char *str);
int mvaddstr(int y, int x, const char *str);

int printw(const char *fmt, ...);
int mvprintw(int y, int x, const char *fmt, ...);

int addwstr(const wchar_t *wstr);

