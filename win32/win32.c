/*
$Id: win32.c,v 1.6 2011/06/20 00:41:27 alexsisson Exp $

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
*/

#include <Windows.h>
#include <stdio.h>
#include <stdarg.h>
#include "win32.h"

static char debug[256];

WINDOW	*stdscr = NULL;
int		COLS = 0;
int		LINES = 0;

static HWND sc_hwnd = NULL;
static HDC  sc_hdc  = NULL;
static int	CW = 0; /* char width  */
static int	CH = 0; /* char height */

static char sc_str_buffer[256];
static int    kb[32], *kp = kb; /* key buffer, and pointer used for getch / ungetch */
static MEVENT mb[32], *mp = mb; /* mouse event buffer and pointer */

static unsigned long color[8];

void sc_set_hwnd(HWND hwnd) {
	sc_hwnd = hwnd;
}

void sc_set_hdc(HDC hdc) {
	HFONT		hfont, holdfont;
	RECT		r;
	TEXTMETRIC	tm;
	double		d;

	sc_hdc		= hdc;
	if(hdc) {
		hfont		= (HFONT)GetStockObject(ANSI_FIXED_FONT); 
		holdfont	= (HFONT)SelectObject(hdc,hfont);
		GetClientRect(sc_hwnd,&r);
		GetTextMetricsW(hdc,&tm);
		CW = tm.tmMaxCharWidth;
		CH = tm.tmHeight;
		d = r.bottom - r.top;
		d /= CH;
		LINES = (int)d;
		d = r.right - r.left;
		d /= CW;
		COLS = (int)d;
	}
}

int peek(void) {
	return (kb==kp) ? -1 : *kp;
}

int ungetchvk(WPARAM vk) {
	OutputDebugStringA("ungetchvk: begin\n");
	switch(vk) {
		case VK_UP:
		case VK_DOWN:
		case VK_LEFT:
		case VK_RIGHT:
		case VK_PRIOR:
		case VK_NEXT:
		case VK_BACK:
		case VK_HOME:
		case VK_END:
		case VK_F1:
		case VK_F2:
		case VK_F3:
		case VK_F4:
		case VK_F5:
		case VK_F6:
		case VK_F7:
		case VK_F8:
		case VK_F9:
		case VK_F10:
		case VK_F11:
		case VK_F12:
			ungetch(vk+KEY_NUDGE);
			break;
		default:
			return 1;
/*
		case VK_SHIFT:
			OutputDebugStringA("ungetchvk: shift (ignored)\n");
			break;
		case VK_CONTROL:
			OutputDebugStringA("ungetchvk: ctrl (ignored)\n");
			break;
		default:
			if(isdigit(vk)||isspace(vk)) {
				OutputDebugStringA("ungetchvk: digit/space\n");
				ungetch(vk);
			} else if(isupper(vk)) {
				if(GetKeyState(VK_SHIFT)&0x8000) {
					OutputDebugStringA("ungetchvk: shift+alpha\n");
					ungetch(GetKeyState(VK_CAPITAL)&0x01?tolower(vk):vk);
				} else if(GetKeyState(VK_CONTROL)&0x8000) {
					OutputDebugStringA("ungetchvk: ctrl+alpha\n");
					ungetch(vk - 64);
				} else {
					OutputDebugStringA("ungetchvk: alpha\n");
					ungetch(GetKeyState(VK_CAPITAL)&0x01?vk:tolower(vk));
				}
			} else switch(vk) {
				case VK_CAPITAL:
					return 1;
				default:
					OutputDebugStringA("ungetchvk: special\n");
					ungetch(KEY_NUDGE + vk);
					break;
			}
			break;
*/
	}
	OutputDebugStringA("ungetchvk: end\n");
	return 0;
}

/* ncurses functions */

WINDOW *initscr() {
	stdscr = (WINDOW*)calloc(1,sizeof(WINDOW));
	color[COLOR_BLACK]   = RGB(0x00,0x00,0x00);
	color[COLOR_RED]     = RGB(0xFF,0x00,0x00);
	color[COLOR_GREEN]   = RGB(0x00,0xFF,0x00);
	color[COLOR_YELLOW]  = RGB(0xFF,0xFF,0x00);
	color[COLOR_BLUE]    = RGB(0x00,0x00,0xFF);
	color[COLOR_MAGENTA] = RGB(0xFF,0x00,0xFF);
	color[COLOR_CYAN]    = RGB(0x00,0xFF,0xFF);
	color[COLOR_WHITE]   = RGB(0xFF,0xFF,0xFF);
	return stdscr;
}

int start_color() {
	return 0;
}

int init_pair(short pair, short f, short b) {
	return 0;
}

int getch(void) {
	return (kb==kp) ? ERR : *(--kp);
}

int ungetch(int ch) {
	*(kp++) = ch;
	return OK;
}

int getmouse(MEVENT *event) {
	double d;
	memset(event,0,sizeof(MEVENT));
	if(mb==mp)
		return ERR;
	memcpy(event,--mp,sizeof(MEVENT));
	d = event->x;
	d /= CW;
	event->x = d;
	d = event->y;
	d /= CH;
	event->y = d;
	return OK;
}

int ungetmouse(MEVENT *event) {
	memcpy(mp++,event,sizeof(MEVENT));
	return OK;
}

int move(int y, int x) {
	stdscr->_curx = x;
	stdscr->_cury = y;
	return OK;
}

int beep(void) {
	return ERR;
}

int refresh(void) {

}

int erase(void) {

}

int attron(int attr) {
	switch(attr) {
		case A_REVERSE:
			SetTextColor(sc_hdc, color[COLOR_BLACK]); SetBkColor(sc_hdc, color[COLOR_WHITE]);	break;

		default:
			/* else assume COLOR_PAIRs */
			/* warning this is a massive hack as we don't actually have a color pair array ! */
			SetTextColor(sc_hdc, color[attr]); SetBkColor(sc_hdc, color[COLOR_BLACK]);	break;


	}
	return OK;
}

int attroff(int attr) {
	switch(attr) {
		case A_REVERSE:
			SetTextColor(sc_hdc, color[COLOR_WHITE]); SetBkColor  (sc_hdc, color[COLOR_BLACK]);	break;
		default:
			SetTextColor(sc_hdc, color[COLOR_WHITE]); SetBkColor  (sc_hdc, color[COLOR_BLACK]);	break;
	}
	return OK;
}

int curs_set(int f) {

}

int addch(int ch) {
	sc_str_buffer[0] = ch;
	sc_str_buffer[1] = '\0';
	addstr(sc_str_buffer);
	return OK;
}

int mvaddch(int y, int x, int ch) {
	move(y,x);
	addch(ch);
	return OK;
}

int addstr(const char *str) {
	int len = strlen(str);
	int release = 0;
	if(!sc_hdc) {
		sc_hdc = GetDC(sc_hwnd);
		if(!sc_hdc) {
			return -1;
		}
		release = 1;
	}
	TextOutA(sc_hdc,stdscr->_curx*CW,stdscr->_cury*CH,str,len);
	stdscr->_curx += len;
	if(release) {
		ReleaseDC(sc_hwnd,sc_hdc);
		sc_hdc = NULL;
	}
	return OK;
}

int mvaddstr(int y, int x, const char *str) {
	move(y,x);
	addstr(str);
	return OK;
}

int printw(const char *fmt, ...) {
	va_list va;
	va_start(va,fmt);
	vsprintf(sc_str_buffer,fmt,va);
	va_end(va);
	addstr(sc_str_buffer);
	return OK;
}

int mvprintw(int y, int x, const char *fmt, ...) {
	va_list va;
	va_start(va,fmt);
	vsprintf(sc_str_buffer,fmt,va);
	va_end(va);
	mvaddstr(y,x,sc_str_buffer);
	return OK;
}

int addwstr(const wchar_t *wstr) {

}
