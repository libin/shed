/*
$Id: shed.c,v 1.75 2011/09/28 01:01:04 alexsisson Exp $

shed 1.16 source

(C) Copyright 2002-2011 Alex Sisson (alexsisson@gmail.com)

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#define _XOPEN_CURSES 1

#ifndef PACKAGE
#define PACKAGE "shed"
#endif
#ifndef VERSION
#define VERSION "1.16"
#endif

/* SHED_LFS ***************************/
#if SHED_LFS == 1

#if			SHED_WIN32 == 1
#define 		SHED_FSEEK _fseeki64
typedef 		__int64 OFF_T;
#else
#define 		SHED_FSEEK fseeko
#include		<sys/types.h>
typedef			off_t OFF_T;
#endif

#else

#define		SHED_FSEEK fseek
#include	<sys/types.h>
typedef		off_t OFF_T;
#endif
/* SHED_LFS ***************************/

#if SHED_NOFOLLOW == 1
#define _GNU_SOURCE
#endif

/* INCLUDES ***************************/
/* COMMON INCLUDES */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>   /* stat       */
#if SHED_WIN32 == 1
/* WIN32 INCLUDES */
#include <windows.h>
#include "win32/resource.h"
#include "win32/win32.h"
#else
/* POSIX GENERAL INCLUDES */
#include <string.h>     /* strcmp etc */
#include <stdlib.h>     /* exit       */
#include <ctype.h>      /* tolower    */
#include <signal.h>     /* signal     */
#include <fcntl.h>      /* open       */
#include <errno.h>
#include <sys/select.h> /* select     */
#include <getopt.h>     /* getopt     */
#include <unistd.h>     /* dup, dup2  */
#if SHED_COCOA == 1
/* COCOA INCLUDES */
#import  <Cocoa/Cocoa.h>
#elif SHED_WCHAR == 1
/* NCURSES WCHAR INCLUDES */
#include <ncursesw/ncurses.h>
#include <locale.h>
#include <wctype.h>
#include <wchar.h>
#include "enc.h"
#else
/* NCURSES ASCII INCLUDES */
#include <ncurses.h>
#endif
#endif

#include "util.h"


#define LOG
#ifdef LOG
#if SHED_WIN32 == 1
void shedloginit() {
}
void shedlog(const char *fmt, ...) {
	static char str[256];
	va_list va;
	va_start(va,fmt);
	vsprintf_s(str,sizeof(str),fmt,va);
	va_end(va);
	OutputDebugStringA(str);
}
#else
FILE *SHEDLOG;
void shedloginit() {
	SHEDLOG = fopen("shed.log","w");
}
void shedlog(const char *fmt, ...) {
	va_list va;
	va_start(va,fmt);
	vfprintf(SHEDLOG,fmt,va);
	va_end(va);
}
#endif
#else
void shedlog(const char *fmt, ...) {
}
#endif
/* function prototypes */
void finish(int s);
void ctrlc(int s);
void cursorup(int n);
void cursordown(int n);
void cursorleft(void);
void cursorright(void);
void cursorhome(void);
void cursorend(void);
void cursoradjust(void);
int  cursorjump(OFF_T n);
void clearline(int line, int width);
int  search(char* str);
int  getinput(char *prompt, int state, int acceptemptystr, int hexonly);
int  mainloop(void);
int  redraw(void);
int  redraw_key_help(int onlykeyhelp);
int  dump(char *dumpfile, int curses);
int  readstdin(void);
int  shedatoi(char *str, char **end, int base, int all);
int  hloadd(char *str, int getdivisor);


/* defines */
#define INPUTSTATE_MAIN                      0
#define INPUTSTATE_GETSTR                    1
#define INPUTSTATE_GETSTR_HEX                2
#define INPUTSTATE_GETSTR_SEARCH             3
#define INPUTSTATE_INIT                      10

#define COL_ASC 0
#define COL_HEX 1
#define COL_DEC 2
#define COL_OCT 3
#define COL_BIN 4

#define STDINBUFSIZE 0xFFFF
#define MAXFILES 8

#define errstr  (strerror(errno))

typedef struct {
	int		index;							/* shed file index in shed array */
	char	*filename;
	FILE	*f;
	OFF_T	len;							/* file length */
	OFF_T	cursorrow;							/* offset of cursor from SOF */
	int		cursorcol;							/* which column cursor is in (shed column, not ncurses screen column */
	OFF_T	viewport;										/* offset of current view from SOF */
	int		isdevice;										/* is file a device? */
	char	*filetype;										/* description of file type */
	int		binmodecursor;                               /* cursor pos in binary toggle mode */
} SHED;

/* globals */
SHED	shed[MAXFILES], *Shed, *Shedstdin;
int		files				= 0;
int		viewsize			= 0;							/* size of viewport */
int		decmode				= 1;                               /* dec or hex display */
int		ascmode				= 0;                               /* setting for ascii column */
int		readonly			= 0;                               /* readonly flag */
int		preview				= 0;                               /* preview mode on/off */
OFF_T	startoffset			= 0;                               /* arg of --start stored here */
int		offsetwidth			= 0;                               /* width of left column */
int		colbase[5]			= {0,16,10,8,2};                   /* base of each column */
int		colwidth[5]			= {1,2,3,3,8};                     /* width of columns */
int		coloffset[5]		= {0,6,10,14,18};                  /* offset from left for each column */
char	*coltitle[5]		= {"asc","hex","dec","oct","bin"}; /* name of each column */
char	*searchstr[5]		= {0,0,0,0,0};                     /* previous searches */
char	*reply				= NULL;                            /* where input is returned */
int		inputstate			= INPUTSTATE_INIT;                 /* input state */
int		binmode				= 1;                               /* binary column mode */
OFF_T	lenarg				= 0;                               /* --length parameter */
int		nofollow			= 0;                               /* do not follow symlinks? */
int		searchbackwards		= 0;                               /* are we searching backwards? */
int		searchinsensitive	= 0;                               /* are we searching insensitive of case */
char	*message			= NULL;								/* message to display */
int		calclength			= 0;
int		hlo[17];												/* highlight offset array */
char	hloc[17];												/* highlight offset char array */
int		hlocolumn			= 0;								/* are we using a hlo column? */
char	*hlv				= NULL;								/* highlight values */
char	hlvc				= 0;								/* highlight value char */
int		hld					= -1;								/* highlight diffs */
int		editsize			= 1;								/* size of cursor in bytes */
int		bigendianmode		= 1;								/* are we treating multi-byte values as big endian? */
int		signedmode;
char	*dumparg			= NULL;
int		inited_scr			= 0;
int		started_color		= 0;
int		filewidth			= 26;								/* width of columns for one file */
int		syncmode			= 1;								/* sync file movements - always true for now */
OFF_T	len					= 0;								/* sync mode total length: ie length of longest stream */

/* multi-byte related */
int    mb                  = 0;
char  *mb_encoding         = NULL;                          /* character encoding. */
int    mb_charwidth        = 0;                             /* width of current wide character */
wchar_t (*getmbchar)(unsigned char c1, FILE *f, int *width) = NULL;/* pointer to decoding function */

#if SHED_WIN32 == 1
#define MAX_LOADSTRING 100
TCHAR	szTitle[MAX_LOADSTRING];
TCHAR	szWindowClass[MAX_LOADSTRING];
wchar_t nofiletext[256] = L"No file";
HINSTANCE shedhinstance;
HWND	shedhwnd = NULL;
HDC		shedhdc = NULL;
#else
int    fdin              = 0;                               /* for dup()'ed stdin */
fd_set fdset;                                               /* for select()ing fdin */
char   *stdinbuf         = NULL;                            /* buf for stdin */
int (*STAT)(const char *path, struct stat *buf) = stat;		/* pointer to stat function */
#endif

int CURSORX = 0;
int CURSORY = 0;

/* win32 main */
#if SHED_WIN32 == 1
INT_PTR CALLBACK wndproc_about(HWND hdlg, UINT msg, WPARAM wparam, LPARAM lparam) {
	UNREFERENCED_PARAMETER(lparam);
	switch(msg) {
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;
	case WM_COMMAND:
		if(LOWORD(wparam)==IDOK || LOWORD(wparam)==IDCANCEL) {
			EndDialog(hdlg,LOWORD(wparam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

INT_PTR CALLBACK wndproc_getinput(HWND hdlg, UINT msg, WPARAM wparam, LPARAM lparam) {
	UNREFERENCED_PARAMETER(lparam);
	switch(msg) {
	case WM_INITDIALOG:
		SetDlgItemTextA(hdlg,IDC_GETINPUT_STATIC,message);
		free(message);
		message = NULL;
		return (INT_PTR)TRUE;
	case WM_COMMAND:
		switch(LOWORD(wparam)) {
			case IDOK:
				GetDlgItemTextA(hdlg,IDC_GETINPUT_EDIT,reply,256);
				EndDialog(hdlg,LOWORD(wparam));
				return (INT_PTR)TRUE;
			case IDCANCEL:
				free(reply);
				reply = NULL;
				EndDialog(hdlg,LOWORD(wparam));
				return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

void file_open(char *path) {
	struct _stati64 st;
	shedlog("OPEN: %s\n",path);
	if(f) {
		fclose(f);
	}
	if(_stati64(path,&st)==0) {
		len = st.st_size;
		f = fopen(path,"r+b");
		if(!f) {
			shedlog("failed to open\n");
			swprintf_s(nofiletext,256,L"Failed to open file: %S",path);
		} else {
			shedlog("open success!\n");
			filetype = " (regular file)";
		}
	}
	/* calculate the width for the offset column and round it */
	if(len) {
		offsetwidth = calcwidth(len,10);
		while(offsetwidth%4!=0)
			offsetwidth++;
	} else {
		offsetwidth = 16;
	}
}

void menu_file_open() {
	OPENFILENAMEA ofn;
	char fn[256];
	*fn = 0;

	memset(&ofn,0,sizeof(ofn));
	ofn.lStructSize			= sizeof(ofn);
	ofn.hwndOwner			= shedhwnd;
	ofn.hInstance			= shedhinstance;
	ofn.lpstrFile			= fn;
	ofn.nMaxFile			= sizeof(fn);
	ofn.Flags				= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

	if(!GetOpenFileNameA(&ofn)) {
		// cancelled or error
		return;
	}

	filename = _strdup(ofn.lpstrFile);
	file_open(filename);
	redraw();
}

LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	int id, ev;
	PAINTSTRUCT ps;
	short mw = 0;
	MEVENT me;

	switch(msg) {
		case WM_COMMAND:
			id = LOWORD(wParam);
			ev = HIWORD(wParam);
			switch(id) {
				case IDM_FILE_OPEN:
					menu_file_open();
					break;
				case IDM_FILE_EXIT:
					DestroyWindow(hwnd);
					break;
				case IDM_ABOUT:
					DialogBox(shedhinstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hwnd, wndproc_about);
					break;
				default:
					return DefWindowProc(hwnd,msg,wParam,lParam);
			}
			break;

		case WM_CHAR:
			shedlog("WM_CHAR: VK=%d [0x%08x] '%c'\n",wParam,wParam,isprint(wParam)?wParam:' ');
			//if(!isprint(wParam))
			//	return 1;
			ungetch(wParam);
			break;

		case WM_KEYDOWN:
			shedlog("WM_KEYDOWN: VK=%d [0x%08x] '%c'\n",wParam,wParam,isprint(wParam)?wParam:' ');
			return ungetchvk(wParam);
			break;

		case WM_PAINT:
			shedlog("WM_PAINT:\n");
			shedhwnd = hwnd;
			sc_set_hwnd(hwnd);
			shedhdc = BeginPaint(hwnd,&ps);
			sc_set_hdc(shedhdc);
			viewsize = LINES - 6;
			shedlog("SIZE: COLS=%d LINES=%d viewsize=%d\n",COLS,LINES,viewsize);
			if(f) {
				redraw();
			} else {
				//addstr(nofiletext);
				TextOut(shedhdc,0,0,nofiletext,wcslen(nofiletext));
			}
			//SelectObject(hdc,holdfont);
			EndPaint(hwnd,&ps);
			sc_set_hdc(NULL);
			shedhdc = NULL;
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		case WM_MOUSEWHEEL:
			mw = ((signed short)HIWORD(wParam))/WHEEL_DELTA;
			shedlog("WM_MOUSEWHEEL: %d\n",mw);
			if(mw>0)
				cursorup(mw);
			else
				cursordown(mw*-1);
			redraw();
			break;

		case WM_LBUTTONUP:
			memset(&me,0,sizeof(MEVENT));
			me.x = LOWORD(lParam);
			me.y = HIWORD(lParam);
			shedlog("WM_LBUTTONUP: (%d,%d)\n",me.x,me.y);
			me.bstate |= BUTTON1_PRESSED;
			ungetmouse(&me);
			ungetch(KEY_MOUSE);
			break;

		default:
			return DefWindowProc(hwnd,msg,wParam,lParam);
	}
	return 0;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	WNDCLASSEX	wcex;
	HWND		hwnd;

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	initscr();

	OutputDebugStringA("CMD LINE:");
	OutputDebugStringA(lpCmdLine);
	OutputDebugStringA("\n");

	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_SHED, szWindowClass, MAX_LOADSTRING);

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= wndproc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SHED));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)GetStockObject(BLACK_BRUSH);//WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_SHED);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
	RegisterClassEx(&wcex);

	shedhinstance = hInstance;
	hwnd = CreateWindow(szWindowClass,szTitle,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,0,CW_USEDEFAULT,0,NULL,NULL,hInstance,NULL);
	if(!hwnd) {
		return FALSE;
	}

	/* open file on command-line*/
	if(lpCmdLine && (*lpCmdLine)) {
		shedlog("got something on command line\n");
		filename = _strdup(lpCmdLine);
		file_open(filename);
	}

	//SetClassLong(shedhwnd,GCLP_HBRBACKGROUND,GetStockObject(GRAY_BRUSH));
	ShowWindow(hwnd,nCmdShow);
	UpdateWindow(hwnd);

	inputstate = INPUTSTATE_MAIN;
	return mainloop();
}

#else
/* ncurses/posix main */
int main(int argc, char **argv) {
	int i;

	memset(hlo,0,sizeof(hlo));
	memset(hloc,0,sizeof(hloc));

	/* getopt long opts */
	struct option opts[] = {	{"help",0,0,'h'},
								{"version",0,0,'v'},
								{"hex",0,0,'H'},
#if SHED_WCHAR == 1
								{"encoding",1,0,'c'},
#endif
								{"dump",1,0,'d'},
								{"length",1,0,'L'},
#if SHED_NOFOLLOW == 1
								{"nofollow",0,0,'n'},
#endif
								{"readonly",0,0,'r'},
								{"start",1,0,'s'},

								{"hlo",1,0,'O'},
								{"hld",1,0,'D'},
								{0,0,0,0}
							};
#ifdef LOG
	shedloginit();
#endif

#if SHED_WCHAR == 1
	setlocale(LC_ALL,"");
#endif

	/* hack for getopt's error messages */
	argv[0] = strdup(PACKAGE);

	/* process args */
	while(1) {
		i = getopt_long(argc,argv,getopt_makeoptstring(opts),opts,0);
		if(i<0)
			break;
		shedlog("init: arg:%C optarg:%s\n",i,optarg?optarg:NULL);
		switch(i) {

		case 'h':
			printf("usage: %s [OPTIONS] [FILE]\n\n",PACKAGE);
			printf("options:\n");
#if SHED_WCHAR == 1
			printf("  -c / --encoding=ENC              set encoding (utf8)\n");
#endif
			printf("  -d / --dump=FILE                 dump to file and exit\n");
			printf("  -H / --hex                       start with hex offsets\n");
			printf("  -L / --length                    set length (for device files)\n");
			printf("  -r / --readonly                  open FILE read only\n");
			printf("  -s / --start=OFFSET              position cursor to offset\n");
			printf("  -o / --hlo=DIVISOR[CHAR|COLOR]   \n");
#if SHED_NOFOLLOW == 1
			printf("  -n / --nofollow                  do not follow symlinks\n");
#endif
			printf("\n");
			printf("  -h / --help                      show help and exit\n");
			printf("  -v / --version                   show version and exit\n");
			printf("\n");
			return 0;

		case 'v':
			printf("%s %s",PACKAGE,VERSION);
#if SHED_LFS == 1
			printf(" (lfs)");
#endif
#if SHED_WCHAR == 1
			printf(" (utf8)");
#endif
#if SHED_CYGFIX == 1
			printf(" (cygfix)");
#endif
#if SHED_NOFOLLOW == 1
			printf(" (nofollow)");
#endif
			printf("\n");
			return 0;
		case 'c':
#if SHED_WCHAR == 1
			mb_encoding = strdup(optarg);
			if(strcmp(mb_encoding,"utf8")==0)
				getmbchar = getmbchar_utf8;
			else {
				fprintf(stderr,"%s: unknown character encoding '%s'\n",PACKAGE,optarg);
				return 1;
			}
			coltitle[0] = mb_encoding;
			for(i=1;i<5;i++)
				coloffset[i] += (strlen(mb_encoding)-3);  // diff between len of encoding name and "asc"
#else
			fprintf(stderr,"%s: character encoding support not enabled\n",PACKAGE);
#endif
			break;
		case 'd':
			dumparg = strdup(optarg);
			break;
		case 'H':
			decmode = 0;
			break;
		case 'L':
			lenarg = atoll(optarg);
			//printf("%lld\n",lenarg);
			break;
		case 'r':
			readonly = 1;
			break;
		case 's':
			startoffset = atoi(optarg);
			break;
		case 'n':
			nofollow = 1;
			STAT = lstat;
			break;
		case 'O':
			hloadd(optarg,1);
			break;
		case 'D':
			hld = hloadd(optarg,0);
			break;
		case '?':
			return 1;
		}
	}

	/* open streams **********************************************************************************************/

	/* check for stdin */
	if(!isatty(STDIN_FILENO)) {
		shedlog("init: found stdin\n");
		Shedstdin = malloc(sizeof(SHED));
		memset(Shedstdin,0,sizeof(SHED));
		Shedstdin->f = tmpfile();
		if(!Shedstdin->f) {
			fprintf(stderr,"%s: failed to create tmpfile() for stdin buffer\n",PACKAGE);
			return 1;
		}
		readonly = 1;
		/* sort out fd's so we can still press keys */
		fdin = dup(STDIN_FILENO);
		dup2(STDOUT_FILENO,STDIN_FILENO);
		stdinbuf = malloc(STDINBUFSIZE);
		Shedstdin->filename = NULL;
		Shedstdin->filetype = "";
	}

	for(files=0;files<(argc-optind);files++) {
		shedlog("init: nonopt[%02d] \"%s\"\n",files,argv[optind+files]);
		Shed = &(shed[files]);
		if(argv[optind+files][0]=='-' && strlen(argv[optind+files])==1) {
			/* stdin */
			if(!Shedstdin) {
				fprintf(stderr,"%s: input from stdin must be piped/redirected.\n",PACKAGE);
				return 1;
			}
			shedlog("init:   using stdin\n");
			memcpy(Shed,Shedstdin,sizeof(SHED));
			free(Shedstdin);
			Shedstdin = NULL;
		} else {
			shedlog("init:   opening...\n");
			Shed->filename = argv[optind+files];
			/* open file */
			if(!readonly) {
				i = O_RDWR;
#if SHED_NOFOLLOW == 1
				if(nofollow)
					i |= O_NOFOLLOW;
#endif
				i = open(Shed->filename,i);
				if(i>=0)
					Shed->f = fdopen(i,"r+");
			}
			if(readonly || i<0) {
				i = O_RDONLY;
#if SHED_NOFOLLOW == 1
				if(nofollow)
					i |= O_NOFOLLOW;
#endif
				i = open(shed[files].filename,i);
				if(i<0) {
					fprintf(stderr,"%s: could not open %s (%s)\n",PACKAGE,shed[files].filename,errstr);
					return 1;
				}
				shed[files].f = fdopen(i,"r");
				readonly = 1;
			}

			/* stat file */
			struct stat st;
			if(STAT(Shed->filename,&st)<0) {
				fprintf(stderr,"%s: could not stat %s\n",PACKAGE,Shed->filename);
				return 1;
			}

			if(S_ISREG(st.st_mode))
				Shed->filetype = " (regular file)";
			else if(S_ISDIR(st.st_mode))
				Shed->filetype = " (directory)";
			else if(S_ISCHR(st.st_mode))
				Shed->filetype = " (character device)";
			else if(S_ISBLK(st.st_mode))
				Shed->filetype = " (block device)";
			else if(S_ISFIFO(st.st_mode))
				Shed->filetype = " (named pipe)";
			else if(S_ISLNK(st.st_mode))
				Shed->filetype = " (symbolic link)";
			else if(S_ISSOCK(st.st_mode))
				Shed->filetype = " (socket)";

			Shed->len = st.st_size;
			fgetc(Shed->f);
			if(!Shed->len) {
				if(feof(Shed->f)) {
					fprintf(stderr,"%s: %s has zero size\n",PACKAGE,Shed->filename);
					return 1;
				}
				/* else some kind of special file (eg device) */
				Shed->isdevice = S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode);
				if(lenarg>0) {
					Shed->len = lenarg;
				} else if(calclength) {
					rewind(Shed->f);
					while(1) {
						i = fgetc(Shed->f);
						if(i<0)
							break;
						if((Shed->len%1000)==0)
							printf("%llu\n",(unsigned long long)Shed->len);
						Shed->len++;
					}

				}
			}
		}
		Shed->index = files;
	}

	if(Shedstdin) {
		/* add stdin if it was found, but not explicity mentioned on command line (with a -) */
		memcpy(&(shed[files++]),Shedstdin,sizeof(SHED));
		free(Shedstdin);
		Shedstdin = NULL;
	}

	switch(files) {
		case 0:
			/* no streams */
			fprintf(stderr,"%s: no streams\n",PACKAGE);
			return 1;
		case 1:
			if(hld>=0) {
				fprintf(stderr,"%s: cannot diff one file\n",PACKAGE);
				return 1;
			}
		default:
			break;
	}



	/* Now find stdin in shed array so that we can keep a pointer to it to update when new stdin data arrives */
	/* Also len */
	for(i=0;i<files;i++) {
		if(!Shedstdin && !shed[i].filename) {
			Shedstdin = &(shed[i]);
			Shedstdin->filename = "(stdin)";
		}
		if(shed[i].len>len) {
			len = shed[i].len;
		}
	}


	Shed = &(shed[0]);

	/* calculate the width for the offset column and round it */
	if(Shed->len) {
		offsetwidth = calcwidth(Shed->len,10);
		while(offsetwidth%4!=0)
			offsetwidth++;
	} else {
		offsetwidth = 16;
	}

	/* handle --dump */
	if(dumparg) {
		while(fdin) {
			readstdin();
		}
		i = dump(dumparg,0) * -1;
		if(message) {
			fprintf(stderr,"%s: %s\n",PACKAGE,message);
		}
		return i;
	}

	/* init ncurses */
	signal(SIGINT,ctrlc);
	initscr();
	inited_scr = 1;
	keypad(stdscr,TRUE);
	cbreak();
	noecho();
	halfdelay(1);

#if SHED_MOUSE == 1
	/* init mouse */
	if(has_mouse()) {
		mousemask(BUTTON1_PRESSED,NULL);
	}
#endif

	for(i=0;i<17;i++) {
		if(hlo[i]&&(hloc[i]<8)) {
			/* color requested on cmd line */
			if(!has_colors()) {
				fprintf(stderr,"%s: terminal does not support colors (has_colors() returned false)\n",PACKAGE);
				return 1;
			}
			if(!started_color) {
				start_color();
				started_color = 1;
			}
			init_pair(hloc[i],hloc[i],COLOR_BLACK);
		}
	}

	/* set size of viewport to LINES - 6 (2 reservered for top + 4 for bottom area) */
	viewsize = LINES - 6;

	if(startoffset) {
		if(Shed->len && startoffset>Shed->len) {
			startoffset = Shed->len-1;
		}
		cursorjump(startoffset);
	}

	inputstate = INPUTSTATE_MAIN;
	redraw();
	mainloop();
	finish(0);
	return 0;
}
#endif


/*

Key handlers


*/



#define H_UNKNOWN			0 /* unhandled key */
#define H_OK				1 /* key handled */
#define H_OK_REDRAW			2 /* key handled, redraw needed */
#define H_OK_REDRAWKEYHELP	3 /* key handled, redraw key help needed */
#define H_OK_ENDSUCCESS		4 /* key handled, end current state (mainloop returns 0) */
#define H_OK_ENDFAIL		5 /* key handled, end current state (mainloop returns -1) */

int keydown_inputstate_getstr() {
	int key,r = H_OK_REDRAW;

	shedlog("keyhandler:getstr: begin\n");

	key = getch();
	shedlog("keyhandler:getstr: getch() %d\n",key);

	if(inputstate==INPUTSTATE_GETSTR_SEARCH) {
		/* search specific keys */
		switch(key) {
			case 2: /* ctrl b */
				searchbackwards = !searchbackwards;
				r = H_OK_REDRAW;
				break;
			case 9: /* ctrl i */
				searchinsensitive = !searchinsensitive;
				r = H_OK_REDRAW;
			default:
				break;
		}
	}

	switch(key) {
		case 3:  /* ctrlc */
		case '\n':
		case '\r':
			inputstate = INPUTSTATE_MAIN;
			if(message) {
				free(message);
				message = NULL;
			}
			r = (key==3) ? H_OK_ENDFAIL : H_OK_ENDSUCCESS;
			break;

		case KEY_BACKSPACE:
		case 127: /* bkspc */
		case 8:   /* ^H */
			shedlog("keyhandler:getstr: bkspc\n");
			if(strlen(reply)) {
				reply[strlen(reply)-1] = 0;
				getyx(stdscr,CURSORY,CURSORX);
				mvaddch(CURSORY,CURSORX-1,' ');
				move(CURSORY,CURSORX-1);
				shedlog("keyhandler:getstr: bkspc: reply: \"%s\"\n",reply);
			}
			break;

		default:
			if(inputstate==INPUTSTATE_GETSTR_HEX) {
				if(!isxdigit(key)) {
					switch(toupper(key)) {
						case 'T':
							decmode = !decmode;
							break;
					}
					key = 0;
				}
			} else if(key>0xFF || (!isprint(key))) {
				shedlog("keyhandler:getstr: ignored\n");
				key = 0;
				r = H_UNKNOWN;
			}
			if(key) {
				reply[strlen(reply)] = key;
				addch(key);
				shedlog("keyhandler:getstr: adding to reply: \"%s\"\n",reply);
				refresh();
			}
			break;
	}
	return r;
}

int keydown_inputstate_main() {
	int key, r = H_OK_REDRAW, i, c, m;
	static char s[256];
	char *mi = message;
#if SHED_MOUSE == 1
	MEVENT mevent;
	OFF_T mrow;
	int mcol;
#endif

	shedlog("keydown:inputstate:MAIN\n");
	key = toupper(getch());
	shedlog("keydown:getch() %03d ['%C']\n",key,isprint(key)?key:' ');
	switch(key) {
		case KEY_UP:
			shedlog("KEYUP\n");
			cursorup(1);
			break;
		case KEY_DOWN:
			shedlog("KEYDOWN\n");
			cursordown(1);
			break;
		case KEY_LEFT:
			shedlog("KEYUP\n");
			cursorleft();
			break;
		case KEY_RIGHT:
		case 9: /* tab */
			cursorright();
			break;
		case KEY_PPAGE:
			case 25: /* ctrl Y */
			cursorup(16);
			break;
		case KEY_NPAGE:
		case 22: /* ctrl V */
			cursordown(16);
			break;
		case KEY_HOME:
		case 1: /* ctrl A */
			cursorhome();
			break;
		case KEY_END:
		case 5: /* ctrl E */
			cursorend();
			break;

		/* edit */
		case ' ':
		case 'E':
			if(readonly) {
				beep();
				break;
			}
			if(Shed->cursorcol==COL_BIN && binmode) {
				/* bit toggle */
				clearerr(Shed->f);
				SHED_FSEEK(Shed->f,Shed->cursorrow,SEEK_SET);
				c = fgetc(Shed->f);
				clearerr(Shed->f);
				SHED_FSEEK(Shed->f,Shed->cursorrow,SEEK_SET);
				m = 0x80 >> Shed->binmodecursor;
				fputc(c&m?c&(~m):c|m,Shed->f);
			} else {
				/* else prompt for new value */
				if(Shed->cursorcol && editsize>1)
					sprintf(s,"new value [%d bit %s] (%s)",editsize*8,bigendianmode?"b.e.":"l.e.",coltitle[Shed->cursorcol]);
				else
					sprintf(s,"new value (%s)",coltitle[Shed->cursorcol]);
				getinput(s,0,0,Shed->cursorcol);
				if(!reply)
					break; /* input cancelled */
				clearerr(Shed->f);
				SHED_FSEEK(Shed->f,Shed->cursorrow,SEEK_SET);
				if(!Shed->cursorcol) {
					 /* ascii column */
					fputc((int)reply[0],Shed->f);
				} else {
					int64_t m64 = 1, c64 = parsestring(reply,colbase[Shed->cursorcol]);
					m64 <<= editsize * 8;
					if(c64<0 || c64>m64-1)
						break;
					for(i=1;i<=editsize;i++) {
						if(bigendianmode)
							c = 0xff & (c64 >> ((editsize-i)*8));
						else
							c = 0xff & (c64 >> ((i-1)*8));
						fputc(c,Shed->f);
					}
					break;
				}
			}
			break;

		/* toggle endianness */
		case '`':
			bigendianmode = !bigendianmode;
			break;

		/* exit */
		case 'X':
		case 24:  /* ^X */
			shedlog("EXIT\n");
			r = H_OK_ENDSUCCESS;
			break;

		/* jump to */
		case 'J':
			sprintf(s,"jump to (%s)",decmode?"dec":"hex");
			getinput(s,0,0,1);
			if(!reply)
				break;
			if(strequ(reply,"top"))
				lenarg = 0;
			else if(strequ(reply,"end")) {
				if(Shed->len)
					lenarg = Shed->len - 1;
				else {
					lenarg = Shed->cursorrow;
					beep();
				}
			} else {
				i = 1;
				switch(toupper(reply[strlen(reply)-1])) {
					/*              case '%':
						break;*/
					case 'G':
						i *= 1024;
					case 'M':
						i *= 1024;
					case 'K':
						i *= 1024;
						reply[strlen(reply)-1]=0;
						break;
					default:
						break;
					}
					lenarg = i * parsestring(reply,decmode?10:16);
					if(lenarg<0)
						break;
					if(Shed->len && lenarg>=Shed->len)
						lenarg = Shed->len-1;
				}
				cursorjump(lenarg);
				break;

		/* repeat search */
		case KEY_F(3): /* F3 */
		case 'R':
		case 'N':
			if(searchstr[Shed->cursorcol]) {
				search("");
				break;
			}
			/* drop through */
			if(0)

		/* ^B/^F search */
		case 2:
		case 6:
			searchbackwards = key == 2;
			/* drop through */

		/* new search */
		case 'S':
		case 23: /* ^W */
		case 'W':
		case 'F':
		case '/':
			sprintf(s,"search for (%s)",coltitle[Shed->cursorcol]);
			if(searchstr[Shed->cursorcol])
				sprintf(s+strlen(s),"[%s]",searchstr[Shed->cursorcol]);
			getinput(s,INPUTSTATE_GETSTR_SEARCH,1,0);
			search(reply);
			break;

		/* toggle dec/hex */
		case 'T':
			decmode = !decmode;
			break;

		/* ascii mode change */
		case 'A':
			ascmode++;
			if(ascmode>2)
				ascmode=0;
			break;

		/* preview */
		case 'P':
			preview = !preview;
			if(preview)
				editsize = 1;
			break;

		/* dump */
		case 'D':
			sprintf(s,"dump to file");
			getinput(s,0,0,0);
			if(!reply)
				break;
			dump(reply,1);
			free(reply);
			reply = NULL;
			break;

		/* bit edit mode */
		case 'B':
			binmode = !binmode;
			Shed->binmodecursor = 0;
			break;

		case 'L':
			if(Shed->isdevice) {
				sprintf(s,"length");
				getinput(s,0,0,0);
				if(!reply)
					break;
				Shed->len = atoi(reply);
			} else
				beep();
			break;

		/* set edit size */
		case '1':
		case '2':
		case '4':
			editsize = key - '0';
			if(editsize>1)
				preview = 0;
			cursoradjust();
			break;

		/* highlight offset */
		case 'H':
			sprintf(s,"highlight offset");
			getinput(s,0,0,0);
			if(!reply)
				break;
			hloadd(reply,1);
			break;

		/* redraw */
		case 12: /* ^L */
			erase();
			refresh();
			break;

		/* resize */
		case KEY_RESIZE:
			refresh();
			viewsize = LINES - 6;
			if(Shed->cursorrow>=Shed->viewport+viewsize)
				Shed->viewport = Shed->cursorrow;
			if(Shed->viewport+viewsize>Shed->len) {
				while(Shed->viewport+viewsize>Shed->len)
					cursorup(1);
			}
			cursordown(viewsize);
			for(i=1;i<LINES-1;i++)
				clearline(i,COLS);
			refresh();
			break;

#if SHED_MOUSE == 1
		/* mouse */
		case KEY_MOUSE:
			shedlog("KEY_MOUSE\n");
			if(getmouse(&mevent)==OK) {
				shedlog("  getmouse OK\n");
				if(mevent.bstate&BUTTON1_PRESSED) {
					mrow = cursorrow - viewport;
					shedlog("  BUTTON1_PRESSED: (%d,%d) currentrow %d\n",mevent.x,mevent.y,mrow);
					if(mevent.y<2)
						mevent.y = 2;
					else if(mevent.y>=(2+viewsize))
						mevent.y = 1 + viewsize;
					mevent.y -= 2;
					if(mevent.y<mrow) {
						cursorup(mrow-mevent.y);
					} else {
						cursordown(mevent.y-mrow);
					}
				}
			}
			break;
#endif

		default:
			shedlog("UNHANDLED KEY\n");
			r = H_UNKNOWN;
			break;
	}
	if(r!=H_UNKNOWN && message && message==mi) {
		shedlog("* clearing message\n");
		free(message);
		message = NULL;
	}
	return r;
}


/*

Main loop


*/

int mainloop() {
	int r = 0;
#if SHED_WIN32 == 1
	MSG	msg;
	HACCEL hat = LoadAccelerators(shedhinstance,MAKEINTRESOURCE(IDC_SHED));
#endif
	shedlog("Mainloop begin: state %d\n",inputstate);

	while(1) {
		//shedlog("  Mainloop start of loop:\n");
#if SHED_WIN32 == 1
		Sleep(50);
		if(1/*inputstate==INPUTSTATE_MAIN*/) {
			if(!GetMessage(&msg,NULL,0,0)) {
				return(int)msg.wParam;
			}
			if(!TranslateAccelerator(msg.hwnd,hat,&msg)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			if(!f) {
				continue;
			}
			if(peek()==-1) {
				continue;
			}
			shedlog("KEY\n");
		}
#endif
		switch(inputstate) {

			/* handle user input prompts */
			case INPUTSTATE_GETSTR:
			case INPUTSTATE_GETSTR_HEX: /* allows toggle */
			case INPUTSTATE_GETSTR_SEARCH:
				r = keydown_inputstate_getstr();
				break;

			/* normal key handling */
			case INPUTSTATE_MAIN:
#if SHED_WIN32 != 1
				if(fdin) {
					readstdin();
				}
				redraw();
#endif
				r = keydown_inputstate_main();
		}

		switch(r) {
			case H_UNKNOWN:
				shedlog("keyhandler returned H_UNKNOWN\n");
				break;
			case H_OK:
				shedlog("keyhandler returned H_OK\n");
				break;
			case H_OK_REDRAW:
				shedlog("keyhandler returned H_OK_REDRAW\n");
				break;
			case H_OK_REDRAWKEYHELP:
				shedlog("keyhandler returned H_OK_REDRAWKEYHELP\n");
				redraw_key_help(1);
				break;
			case H_OK_ENDSUCCESS:
				shedlog("keyhandler returned H_OK_ENDSUCCESS\n");
				return 0;
			case H_OK_ENDFAIL:
				shedlog("keyhandler returned H_OK_ENDFAIL\n");
				return -1;
		}


#if SHED_WIN32 == 1
		RedrawWindow(shedhwnd,NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW);
#endif

	}
}



/* functions */

/* cursor movements functions */

#define calccursorsize() (Shed->cursorcol?((Shed->cursorcol==COL_BIN&&binmode)?1:editsize):1)

void cursorup(int n) {
	SHED *s = Shed;
	int i   = 0;
	int N   = n;
LOOP:
	n = N;
	if(syncmode) {
		Shed = &(shed[i]);
	}
	while(n--) {
		if(Shed->cursorrow) {
			Shed->cursorrow--;
			if(Shed->cursorrow<Shed->viewport)
				Shed->viewport--;
		}
		else
			beep();
	}
	if(syncmode && (++i<files)) {
		goto LOOP;
	}
	Shed = s;
}

void cursordown(int n) {
	int cs  = calccursorsize();
	SHED *s = Shed;
	int i   = 0;
	int N   = n;
	int L   = syncmode ? len : Shed->len;
LOOP:
	n = N;
	if(syncmode) {
		Shed = &(shed[i]);
	}
	while(n--) {
		if(Shed->cursorrow<L-cs || !L) {
			Shed->cursorrow++;
			while(Shed->cursorrow+(cs-1)>=Shed->viewport+viewsize)
				Shed->viewport++;
		} else {
			beep();
		}
	}
	if(syncmode && (++i<files)) {
		goto LOOP;
	}
	Shed = s;
}

void cursorleft(void) {
	if(Shed->cursorcol==COL_BIN) {
		if(binmode && Shed->binmodecursor)
			Shed->binmodecursor--;
		else
			Shed->cursorcol--;
	}
	else if(Shed->cursorcol)
		Shed->cursorcol--;
	else if(Shed->index) {
		shed[Shed->index-1].cursorrow = Shed->cursorrow;
		Shed = &(shed[Shed->index-1]);
	} else {
		beep();
		return;
	}
	cursoradjust();
}

void cursorright(void) {
	if(Shed->cursorcol<COL_BIN)
		Shed->cursorcol++;
	else {
		if(binmode && Shed->binmodecursor<7)
			Shed->binmodecursor++;
		else {
			if(Shed->index+1<files) {
				shed[Shed->index+1].cursorrow = Shed->cursorrow;
				Shed = &(shed[Shed->index+1]);
			} else {
				beep();
			}
			return;
		}
	}
	cursoradjust();
}

void cursorhome() {
	if(Shed->cursorcol==COL_BIN && binmode && Shed->binmodecursor) {
		Shed->binmodecursor = 0;
	} else if(Shed->cursorcol) {
		Shed->cursorcol = 0;
	} else if(Shed->index) {
		shed[Shed->index-1].cursorrow = Shed->cursorrow;
		Shed = &(shed[Shed->index-1]);

	}
}

void cursorend() {
	if(shed->cursorcol==COL_BIN && ((!binmode)||Shed->binmodecursor==7) && (Shed->index+1<files)) {
		shed[Shed->index+1].cursorrow = Shed->cursorrow;
		Shed = &(shed[Shed->index+1]);
	} else {
		Shed->cursorcol = COL_BIN;
		Shed->binmodecursor = 7;
	}
}

void cursoradjust(void) {
	int cs;
	if(editsize==1) {
		return;
	}
	cs = calccursorsize();
	while(Shed->cursorrow>(Shed->len-cs)) {
		cursorup(1);
	}
	while(Shed->cursorrow+(cs-1)>=Shed->viewport+viewsize) {
		Shed->viewport++;
	}
}

/* clears a line on the screen */
void clearline(int line, int width) {
#if SHED_CYGFIX == 1
	width--;
#endif
	move(line,0);
	while(width--)
		addch(' ');
}

/* search */
#define search_char_cmp(c1,c2) ((searchinsensitive && isalpha((c1))) ? tolower((c1))==(c2) || toupper((c1))==(c2) : (c1) == (c2))

int search(char *str) {
	int c;
	int i,slen;
	unsigned char *search = NULL;
	long l;

	if(!str)
		return 0;

	if(strlen(str)) {
		/* user entered a string, so make a copy to searchstr for repeat searches. */
		free(searchstr[Shed->cursorcol]);
		searchstr[Shed->cursorcol] = strdup(str);
	} else if(!searchstr[Shed->cursorcol]) {
		/* else user just pressed enter, but no previous search */
		return 0;
	}

	slen = strlen(searchstr[Shed->cursorcol]);
	search = malloc(slen+2);
	strcpy((char*)search,searchstr[Shed->cursorcol]);

	if(Shed->cursorcol) {
		/* parse string */
		char *p = malloc(slen+2);
		strcpy(p,(char*)search);
		p = strtok(p," :,.\0");
		for(i=0;p;i++) {
			l = parsestring(p,colbase[Shed->cursorcol]);
			if(l<0 || l>255)
				return 0;
			search[i] = (unsigned char)l;
			p = strtok(NULL," :,.\0");
		}
		free(p);
		search[i] = 0;
		slen = i;
	}

	clearerr(Shed->f);

	/* backwards */
	if(searchbackwards) {
		shedlog("search: backwards for '%s'\n",search);
		cursorup(1);
		SHED_FSEEK(Shed->f,Shed->cursorrow-1,SEEK_SET);
		for(;Shed->cursorrow;cursorup(1)) {
			c = fgetc(Shed->f);
			shedlog("search: %d: c=%d(%c)\n",Shed->cursorrow,c,isprint(c)?c:'.');
			if(search_char_cmp((unsigned char)c,search[0])) {
				shedlog("search:   first letter match:\n");
				for(i=1;i<slen;i++) {
					shedlog("search:     c=%d(%c)\n",c,isprint(c)?c:'.');
					c = fgetc(Shed->f);
					if(c==EOF)
						break;
					if(!search_char_cmp(c,search[i]))
						break;
				}
				if(i==slen) {
					cursorup(i-1);
					cursordown(i-1);
					free(search);
					return 1;
				}
			}
			clearerr(Shed->f);
			SHED_FSEEK(Shed->f,Shed->cursorrow-1,SEEK_SET);
		}

		/* forwards */
	} else {
		cursordown(1);
		SHED_FSEEK(Shed->f,Shed->cursorrow,SEEK_SET);
		for(;Shed->cursorrow< Shed->len-1;cursordown(1)) {
			c = fgetc(Shed->f);
			if(search_char_cmp((unsigned char)c,search[0])) {
				for(i=1;i<slen;i++) {
					c = fgetc(Shed->f);
					if(c==EOF)
						break;
					if(!search_char_cmp(c,search[i]))
						break;
				}
				if(i==slen) {
					cursordown(i-1);
					cursorup(i-1);
					free(search);
					return 1;
				}
				clearerr(Shed->f);
				SHED_FSEEK(Shed->f,Shed->cursorrow+1,SEEK_SET);
			}
		}
	}

	free(search);
	return 0;
}

int cursorjump(OFF_T n) {
	if(syncmode&&files>1) {
		int i;
		SHED *s = Shed;
		syncmode = 0;
		for(i=0;i<files;i++) {
			Shed = &(shed[i]);
			cursorjump(n);
		}
		syncmode = 1;
		Shed = s;
	} else {
		if(Shed->cursorrow>n) {
			if(n< Shed->viewport || n>Shed->viewport+viewsize) {
				Shed->cursorrow = n;
				Shed->viewport = Shed->cursorrow;
			} else {
				cursorup(Shed->cursorrow-n);
			}
		} else {
			if(n<Shed->viewport || n>Shed->viewport+viewsize) {
				Shed->cursorrow = n;
				Shed->viewport  = Shed->cursorrow - (viewsize - 1);
			} else {
				cursordown(n-Shed->cursorrow);
			}
		}
	}
	return 0;
}

int redraw() {
	int i,c,n,X,h,f;
	char str[256];
	char C[MAXFILES];
#if SHED_WCHAR == 1
	wchar_t wstr[5];
	wstr[0]=L' ';
	wstr[1]=L' ';
	wstr[2]=L' ';
	wstr[3]=L' ';
	wstr[4]=L'\0';
#endif

	shedlog("redraw: begin\n");

#if SHED_WIN32 == 1
	if(!shedhdc) {
		shedlog("redraw: win32 no hdc\n");
		RedrawWindow(shedhwnd,NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW);
		return -1;
	}
#endif

	/* calculate position of preview/other stuff */
	X = offsetwidth + (hlocolumn?4:0) + coloffset[COL_BIN] + colwidth[COL_BIN] + 1;

	/* redraw top */
	attron(A_REVERSE);
	clearline(0,COLS);
	for(f=0;f<files;f++) {
		mvprintw(0,(files==1?0:offsetwidth+2+f*filewidth),"%s%s%s",shed[f].filename,shed[f].filetype,readonly?" (read only)":"");
	}
	mvprintw(0,COLS-10,"shed %s",VERSION);
	attroff(A_REVERSE);

	/* draw column headers */
	clearline(1,COLS);
	mvprintw(1,0,"%s",(offsetwidth==4)?"offs":"offset");
	for(f=0;f<files;f++) {
		move(1,offsetwidth+2+f*filewidth);
		if(hlocolumn)
			printw("hlo ");
		for(i=0;i<5;i++)
			printw("%s ",coltitle[i]);
	}
	if(files==1) {
		mvprintw(1,X,"%s",preview?"preview":"       ");
	}
	//addwstr(L"LOL \u0333");

	/* seek to current part of file and display */
	for(i=0;i<files;i++)
		clearerr(shed[i].f);

	for(i=0;i<viewsize;i++) {
		/* check for hlo highlight */
		h = -1;
		for(n=0;n<17;n++) {
			if(!hlo[n])
				break;
			if(n==hld)
				continue;
			if(((Shed->viewport+i)%(hlo[n]))==0) {
				h = n;
				if(hloc[h]<8) {
					attron(COLOR_PAIR(hloc[h]));
				}
				break;
			}
		}

		mvprintw(i+2,0,"%s: ",getstring(Shed->viewport+i,str,(decmode)?10:16,offsetwidth));

		/* print highlight char if needed */
		if(h>=0 && hloc[h]>=8) {
			printw(" %c  ",hloc[n]);
		} else if(hlocolumn) {
			printw("    ");
		}

		/* read each file byte for this line */
		for(f=0;f<files;f++) {
			SHED_FSEEK(shed[f].f,shed[f].viewport+i,SEEK_SET);
			C[f] = fgetc(shed[f].f);
			if(C[f]==EOF) {
				shedlog("redraw: file:%02d EOF\n",f);
				continue;
			}
		}

		if(hld>=0) {
			/* diff on */
			for(n=0;n<files-1;n++) {
				if(C[n]!=C[n+1]) {
					shedlog("redraw: diff C[%02d]:%C C[%02d]:%C hld=%d hloc[hld]=%d\n",n,isprint(C[n])?C[n]:' ',n+1,isprint(C[n+1])?C[n+1]:' ',hld,hloc[hld]);
					attron(COLOR_PAIR(hloc[hld]));
					break;
				}
			}
		}

		/* print values */
		for(f=0;f<files;f++) {
			move(i+2,offsetwidth+2+filewidth*f);

#if SHED_WCHAR == 1
			if(mb) {
				if(!(mb_charwidth--)) {
					wstr[1] = getmbchar(c,Shed->f,&mb_charwidth);
					if(!iswprint(wstr[1]))
						wstr[1] = L' ';
					addwstr(wstr);
				}
			} else
#endif
			printw("%s " ,getascii (C[f],str,ascmode));
			printw("%s  ",getstring(C[f],str,16,2));
			printw("%s " ,getstring(C[f],str,10,3));
			printw("%s " ,getstring(C[f],str, 8,3));
			printw("%s " ,getstring(C[f],str, 2,8));
		}

		if(n<files-1) {
			shedlog("redraw: diff color off\n");
			attroff(COLOR_PAIR(hloc[hld]));
		}
		if(h>=0&&hloc[h]<8) {
			attroff(COLOR_PAIR(hloc[h]));
		}
	}



	/* draw cursor */
	clearerr(Shed->f);
	attron(A_REVERSE);
	SHED_FSEEK(Shed->f,Shed->cursorrow,SEEK_SET);
	for(i=0;i<editsize;i++) {
		n = (Shed->cursorrow-Shed->viewport) + 2 + i;
		c = fgetc(Shed->f);
		if(!Shed->cursorcol) {
			/* ascii column */
			move(n,offsetwidth+hlocolumn+3+(Shed->index*filewidth));
#if SHED_WCHAR == 1
			//      if(utf8) {
			//        wstr[1] = getutf8(c);
			//        if(!iswprint(wstr[1]))
			//          wstr[1] = L' ';
			//          addwstr(wstr+1);
			//      } else
#endif
			addch(isprint(c)?c:' ');
			break;
		} else {
			/* other columns */
			getstring(c,str,colbase[Shed->cursorcol],colwidth[Shed->cursorcol]);
			int cc = offsetwidth + hlocolumn + (Shed->index*filewidth) + coloffset[Shed->cursorcol];
			if(Shed->cursorcol==COL_BIN && binmode) {
				mvprintw(n,cc+Shed->binmodecursor,"%C",str[Shed->binmodecursor]);
				break;
			} else {
				mvaddstr(n,cc,str);
			}
		}
	}
	attroff(A_REVERSE);

	if(files==1) {
		/* draw preview */
		if(preview) {
			SHED_FSEEK(Shed->f,Shed->cursorrow,SEEK_SET);
			n = X;
			move((Shed->cursorrow-Shed->viewport)+2,n);
			for(i=0;i<32;i++) {
				c = fgetc(Shed->f);
				if(c==EOF)
					break;
				addch(isprint(c)?c:'.');
			}
		} else if(inputstate==INPUTSTATE_MAIN) {
			/* draw multi-byte value */
			if(Shed->cursorcol) {
				if(editsize>1 && !(Shed->cursorcol==COL_BIN&&binmode)) {
					uint32_t v=0,c32;
					SHED_FSEEK(Shed->f,Shed->cursorrow,SEEK_SET);
					for(i=1;i<=editsize;i++) {
						c = fgetc(Shed->f);
						if(c>=0) {
							c32 = c;
							if(bigendianmode)
								v += c32 << ((editsize-i)*8);
							else
								v += c32 << ((i-1)*8);
						}
					}
					move((Shed->cursorrow-Shed->viewport)+2,X);
					n = Shed->cursorcol==COL_HEX ? calcwidth((((uint64_t)1)<<(editsize*8))-1,colbase[Shed->cursorcol]) : 0;
					getstring(v,str,colbase[Shed->cursorcol],n);
					printw("%s (%d bit unsigned %s %s)",str,editsize*8,bigendianmode?"b.e.":"l.e.",coltitle[Shed->cursorcol]);
				}
			}
		}
#if SHED_WCHAR == 1
		else if(mb) {

		}
#endif
		else {
			n = X;
			move(i+2,n);
			for(;n<COLS;n++)
				addch(' ');
		}
	}

	/* draw status line */
	attron(A_REVERSE);
	clearline(LINES-3,COLS);
	c = decmode?10:16;
	if(message) {
		mvaddstr(LINES-3,0,message);
		if(reply) {
			addstr(reply);
		}
	} else if(inputstate==INPUTSTATE_MAIN) {
		if(files>1) {
			for(f=0;f<files;f++) {
				mvprintw(LINES-3,(files==1?0:offsetwidth+2+f*filewidth),"%s/",getstring(shed[f].cursorrow,str,c,0));
				addstr(getstring(shed[f].len,str,c,0));
			}
		} else if(Shed->cursorrow==Shed->len-calccursorsize()) {
			mvaddstr(LINES-3,0,"(end)");
		}
	}

	mvaddstr(LINES-3,COLS-30,getstring(Shed->index,str,10,0));

	if(files==1) {
		mvaddstr(LINES-3,COLS-(calcwidth(Shed->cursorrow,c)+calcwidth(Shed->len,c)+8),getstring(Shed->cursorrow,str,c,0));
		addstr("/");
		addstr(getstring(Shed->len,str,c,0));
	}

	mvaddstr(LINES-3,COLS-5,(decmode)?"(dec)":"(hex)");
	attroff(A_REVERSE);

	/* draw key help */
	redraw_key_help(0);

	refresh();
	shedlog("redraw: end\n");
	return 0;
}


#define ONOFF(s) (s?"on":"off")

/* factored out of redraw so could be called during INPUTSTATE_GETSTR* */
int redraw_key_help(int onlykeyhelp) {
	if(onlykeyhelp)
		attroff(A_REVERSE);
	clearline(LINES-2,COLS);
	clearline(LINES-1,COLS);
	switch(inputstate) {
	case INPUTSTATE_MAIN:
		mvaddstr(LINES-2,0,"SPACE|E edit  S|W|F search  J jump to   T dec/hex   D dump     1|2|4 cursor");
		mvaddstr(LINES-1,0,"X       exit  R|N   repeat  B bin edit  A ext. asc  P preview  `     endian");
		break;
	case INPUTSTATE_GETSTR:
		mvaddstr(LINES-2,0,"ENTER accept");
		mvaddstr(LINES-1,0,"^C    cancel");
		break;
	case INPUTSTATE_GETSTR_HEX:
		mvaddstr(LINES-2,0,"ENTER accept  T toggle");
		mvaddstr(LINES-1,0,"^C    cancel");
		break;
	case INPUTSTATE_GETSTR_SEARCH:
		mvprintw(LINES-2,0,"ENTER accept  ^B search backwards (%s)",ONOFF(searchbackwards));
		mvprintw(LINES-1,0,"^C    cancel");
		if(!Shed->cursorcol)
			printw("  ^I ignore case (%s)",ONOFF(searchinsensitive));
		break;
	default:
		break;
	}
	if(onlykeyhelp) {
		attron(A_REVERSE);
		move(LINES-3,strlen(message)+strlen(reply));
		curs_set(1);
	}
	else {
		mvaddstr(LINES-1,COLS-1," ");
		curs_set(0);
	}
	return 0;
}


/* gets user input */
int getinput(char *prompt, int state, int acceptemptystr, int hexonly) {
	shedlog("getinput: begin: state:%d acceptempty:%d hexonly:%d\n",state,acceptemptystr,hexonly);
	message = malloc(strlen(prompt)+5);
	sprintf(message,"%s: ",prompt);
	free(reply);
	reply   = malloc(128);
	memset(reply,0,128);

#if FOO//SHED_WIN32 == 1
	if(DialogBox(shedhinstance, MAKEINTRESOURCE(IDD_GETINPUT), shedhwnd, wndproc_getinput)==IDOK) {
#else
	inputstate = state ? state : INPUTSTATE_GETSTR; //hexonly ? 2 : 1;
	redraw();
	move(LINES-3,strlen(message));
	curs_set(1);
	attron(A_REVERSE);
	if(mainloop()==0) {
#endif
		// ok
		shedlog("getinput: mainloop returned success: strlen(reply): %d\n",strlen(reply));
		if(acceptemptystr||strlen(reply)) {
			shedlog("getinput: string ok\n");
			attroff(A_REVERSE);
			return 0;
		}
	}
	shedlog("getinput: cancelled\n");
	// else cancelled
	attroff(A_REVERSE);
	free(reply);
	reply = NULL;
	return -1;
}


/* dump shed output to another file */
int dump(char *dumpfile, int curses) {
	int i,c;
	FILE *df = NULL;
	char str[32];

	shedlog("dump: %s\n",dumpfile);

	if(!curses && strlen(dumpfile)==1 && *dumpfile=='-')
		df = stdout;
	if(!df) {
		struct stat is, ds;
		int i;
		if(STAT(dumpfile,&ds)==0) {
			for(i=0;i<files;i++) {
				if(STAT(shed[i].filename,&is)==0) {
					if((ds.st_dev==is.st_dev)&&(ds.st_ino==is.st_ino)) {
						/* dump file is an input file */
						free(message);
						message = strdup("Error: cannot dump to input file");
						shedlog("dump: %s\n",message);
						return -2;
					}
				}
			}
		}
		df = fopen(dumpfile,"w");
		if(!df) {
			free(message);
			message = malloc(64);
			sprintf(message,"Error: failed to open '%s' for writing",dumpfile);
			shedlog("dump: %s\n",message);
			return -1;
		}
	}
	if(curses) {
		attron(A_REVERSE);
		mvaddstr(LINES-3,0,"dumping...");
		mvprintw(LINES-3,11+offsetwidth,"/%s",getstring(Shed->len,str,decmode?10:16,offsetwidth));
	}
	switch(offsetwidth) {
		case  4: fprintf(df,"offs");             break;
		case  8: fprintf(df,"offset  ");         break;
		case 12: fprintf(df,"offset      ");     break;
		case 16: fprintf(df,"offset          "); break;
	}
	fprintf(df,"  asc hex dec oct bin\n");
	rewind(Shed->f);
	for(i=0;i<Shed->len;i++) {
		c = fgetc(Shed->f);
		if(c<0)
			break;
		if(curses && (!(i%1024))) {
			mvaddstr(LINES-3,11,getstring(i,str,decmode?10:16,offsetwidth));
			refresh();
		}
		fprintf(df,"%s:  ",getstring(i,str,decmode?10:16,offsetwidth));
		fprintf(df,"%c  ",(char)((c>32&&c<127)?c:' '));
		fprintf(df,"%s  ",getstring(c,str,16,2));
		fprintf(df,"%s ",getstring(c,str,10,3));
		fprintf(df,"%s ",getstring(c,str, 8,3));
		fprintf(df,"%s\n",getstring(c,str, 2,8));
	}
	if(curses)
		attroff(A_REVERSE);
	fclose(df);
	shedlog("dump: ok\n");
	return 0;
}

int shedatoi(char *str, char **end, int base, int all) {
	int r = 0;
	if(!(*str)) {
		fprintf(stderr,"%s: invalid numerical argument: '%s'\n",PACKAGE,str);
		finish(1);
	}
	r = strtol(str,end,base);
	if(end) {
		if(all&&(**end)) {
			fprintf(stderr,"%s: invalid numerical argument: '%s'\n",PACKAGE,str);
			finish(1);
		}
	}
	return r;
}

/* add highlight info to array */
int hloadd(char *str, int getdivisor) {
	int i;
	char error[256];
	*error = 0;

	shedlog("hloadd: string:%s\n",str);

	for(i=0;i<17;i++) {
		if(!hlo[i]) {
			char *e = NULL;
			if(getdivisor) {
				hlo[i] = shedatoi(str,&e,10,0);
				shedlog("hloadd: [%02d] divisor:%d\n",i,hlo[i]);
				if(!hlo[i]) {
					sprintf(error,"Error: invalid divisor");
					goto Error;
				}
				if(!(*e)) {
					sprintf(error,"Error: expecting hlo character or color");
					goto Error;
				}
			} else {
				shedlog("hloadd: [%02d] skipped divisor check\n",i);
				hlo[i] = -1;
				e = str;
			}
			if(strlen(e)>1) {
				shedlog("hloadd: [%02d] color string: \"%s\"\n",i,e);
				hloc[i] =	!strcmp(e,"black")   ? 0 :\
							!strcmp(e,"red")     ? 1 :\
							!strcmp(e,"green")   ? 2 :\
							!strcmp(e,"yellow")  ? 3 :\
							!strcmp(e,"blue")    ? 4 :\
							!strcmp(e,"magenta") ? 5 :\
							!strcmp(e,"cyan")    ? 6 :\
							!strcmp(e,"white")   ? 7 :\
							-1;
				if(hloc[i]==-1) {
					sprintf(error,"Error: unknown color '%s'",e);
					goto Error;
				}
				if(inputstate==INPUTSTATE_MAIN) {
					/* from interface */
					if(!started_color) {
						start_color();
						started_color = 1;
					}
					init_pair(hloc[i],hloc[i],COLOR_BLACK);
				}
			} else {
				hloc[i] = *e;
				if(!hlocolumn) {
					hlocolumn = 4;
					filewidth += hlocolumn;
				}
			}
			break;
		}
	}
Error:
	if(*error) {
		shedlog("hloadd: %s\n",error);
		if(inputstate==INPUTSTATE_INIT) {
			fprintf(stderr,"%s: %s\n",PACKAGE,error);
			finish(1);
		} else {
			hlo[i] = 0;
			hloc[i] = 0;
			free(reply);
			reply = NULL;
			message = strdup(error);
		}
		return -1;
	}
	shedlog("hloadd: ok\n");
	return i;
}

#if SHED_WIN32 == 1
void finish(int s) {
	shedlog("FINISH");
	DestroyWindow(shedhwnd);
}
#else
/* ends ncurses and quits */
void finish(int s) {
	if(inited_scr) {
		endwin();
		printf("\n");
	}
	exit(s);
}

/* handles ctrl c */
void ctrlc(int s) {
	ungetch(3);
}

int readstdin() {
	int r = 0;
	struct timeval tv = {0,0};
	FD_ZERO(&fdset);
	FD_SET(fdin,&fdset);
	if(select(fdin+1,&fdset,0,0,&tv)>0) {
		if(FD_ISSET(fdin,&fdset)) {
			r = read(fdin,stdinbuf,STDINBUFSIZE);
			if(r>0) {
				SHED_FSEEK(Shedstdin->f,Shedstdin->len,SEEK_SET);
				fwrite(stdinbuf,1,r,Shedstdin->f);
				Shedstdin->len += r;
				if(Shedstdin->len>len) {
					len = Shedstdin->len;
				}
			} else {
				/* eof / err */
				fdin = 0;
			}
		}
	}
	return r;
}
#endif

