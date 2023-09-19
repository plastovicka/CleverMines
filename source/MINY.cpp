/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */

#include <windows.h>
#pragma hdrstop
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "miny.h"
#include "lang.h"

#pragma comment(lib,"comctl32.lib")
#pragma comment(lib, "version.lib")

//---------------------------------------------------------------------------
static int dxy[]={13, 14, 0, 15, 0, 16, 0, 17, 0, 18, 0, 20, 0, 21, 0, 0, 23,
24, 0, 0, 25, 26, 0, 27, 0, 28, 0, 29, 0, 30};
const int minGridY=23, maxGridY=sizeA(dxy)+minGridY-1;

enum {
	clOpened, clField=13, clvi, clBlast,
	clX, clBkgnd, clgraf, cllastScore, clText, clGrid,
};
COLORREF colors[] ={
	0x404040,
	0x00ffff, 0xffff00, 0x0000ff, 0x00ff00, 0xff00ff, 0xf0f0f0, //numbers
	0x00b0b0, 0xefc0e0, 0xc0efc0, 0x78fafd0, 0xb0e0f0, 0xffc08f,
	0xbbc8c8L, 0x60b0a0L, 0x6000d0L,
	0x0000f0, 0xc0e0c0L, 0xc00060, 0x7000c0,
	0x100000, 0x70746d
};
static COLORREF custom[16];
static int penIndex[]={ clGrid,clBlast,clOpened, -1 };
static int brushIndex[]={ clField,clvi,clBlast,clOpened, -1 };
const int Ncl=sizeA(colors);
HPEN pens[Ncl], penLight, penDark;
HBRUSH brushes[Ncl];

const int remainW=140, timeW=130;
#define Dtab 10

HDC dc;
HWND hWin;
HINSTANCE inst;
HIMAGELIST himg[3];	//a mine BMP, 3 sizes
DWORD time0;

//---------------------------------------------------------------------------
int
triang=1,      //1=triangular field, 0=rectangular field
 width=23,
 height=15,     //field size (without borders)
 Width, Height,  //window size
 mainLeft=100, mainTop=100, //window position
 NminesRel=18,  //mines density
 level=5,       //difficulty
 gridx, gridy=40,//block size (in pixels)
 Nmines,    //total number of mines
 remainm,   //remaining (not marked) mines, can be negative!
 remaino,   //remaining open blocks
 canOpen,   //not open blocks that can be safely opened
 playtime,  //time in 0.1 sec
 clickSum,  //clicks counter
 clicks,    //clicks in last second
 startGraf,
 rate,
 lastScore[Dtab],//last row added to hiscore table
 isEasyStart=1,  //no mines around the first click
 fastOpen=1,     //block is opened just after mouse button down
 flagOpen=0,     //can open marked block
 x0, y0, xk, yk,    //field bounding rectangle
 xCh=4,yCh=6,
 xmdlg, ymdlg, NminesReldlg; //values in "Custom" dialog

bool
vizmin, vizv, debug, cheat,//show mines and definite blocks (cheat)
 delreg=false,  //user deleted settings
 isMouseDown=false,
 looser, gameOver;//game over

PSquare
board=0, boardk,//field array
 pressed, //clicked block
 bum;     //popped mine

char *title; //window title

int diroff8[9], diroff12a[13], diroff12b[13]; //offsets to adjacent blocks

struct Ttime
{
	WORD wYear;
	BYTE wMonth;
	BYTE wDay;
	BYTE wHour;
	BYTE wMinute;
};

#define Dscore 8
struct TScoreold
{
	char name[16];
	short playtime, height, width, NminesRel, level;
	int speed;
	SYSTEMTIME date;
} score8[10], score12[10];

struct TScore
{
	char name[16];
	short level, playtime, speed;
	BYTE height, width;
	Ttime date;
} score[Dtab][Dscore];

void convertScore(TScore &s, TScoreold o)
{
	//older versions saved score in larger data structure
	memset(&s, 0, sizeof(TScore));
	strcpy(s.name, o.name);
	s.level=o.level;
	s.playtime=o.playtime;
	s.speed=(short)o.speed;
	s.height=(BYTE)o.height;
	s.width=(BYTE)o.width;
	s.date.wYear=o.date.wYear;
	s.date.wMonth=(BYTE)o.date.wMonth;
	s.date.wDay=(BYTE)o.date.wDay;
	s.date.wHour=(BYTE)o.date.wHour;
	s.date.wMinute=(BYTE)o.date.wMinute;
}

char playerName[17];  //last winner

#define Dspeed 5      //speed graph inertia
int speedTab[Dspeed]; //number of clicks in last Dspeed seconds
#define Dgraf 80
int grafTab[Dgraf]; //graph values
int Ispeed, Igraf;   //current position in arrays

const char *subkey="Software\\Petr Lastovicka\\miny";
const struct Treg { char *s; int *i; } regVal[]={
	{"sirka", &width}, {"vyska", &height}, {"pocet min", &NminesRel},
	{"obtiznost", &level}, {"velikost", &gridy},
	{"triang", &triang}, {"easyStart", &isEasyStart},
	{"fastOpen", &fastOpen}, {"flagOpen", &flagOpen},
	{"top", &mainTop}, {"left", &mainLeft},
};

//---------------------------------------------------------------------------
void msg(char *text, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, text);
	vsprintf(buf, text, ap);
	MessageBox(hWin, buf, title, MB_OK|MB_ICONERROR);
	va_end(ap);
}
//---------------------------------------------------------------------------
//convert mouse coordinates to block pointer
PSquare hit(LPARAM lP)
{
	int i, j, x, y, x1, y1, x2, y2, w;

	x= LOWORD(lP)-x0;
	y= HIWORD(lP)-y0;
	if(x<0 || y<0) return 0;
	i= x/gridx;
	j= y/gridy;
	if(triang){
		x1= i*gridx;
		x2= x1+gridx;
		y1= j*gridy;
		y2= y1+gridy;
		if(((i+j)&1)==0){
			w=x2; x2=x1; x1=w;
		}
		if((y2-y1)*x+(x1-x2)*y-x1*y2+x2*y1 < 0) x=i-1; else x=i;
	}
	else{
		x=i;
	}
	y=j;
	if((x>=0)&&(y>=0)&&(x<width)&&(y<height)){
		return board + (x+2)*(height+2) + y+1;
	}
	else{
		return 0;
	}
}
//---------------------------------------------------------------------------
void numOut(int x, int y, char *str, int num)
{
	char buf[128];
	//show text and number
	strcpy(buf, str);
	sprintf(strchr(buf, 0), ": %d", num);
	TextOut(dc, x*Width/450, y, buf, (int)strlen(buf));
}

void charOut(int x, int y, char ch, COLORREF color)
{
	SetTextColor(dc, color);
	TextOut(dc, x, y, &ch, 1);
}

void line(int x1, int y1, int x2, int y2)
{
	MoveToEx(dc, x1, y1, NULL);
	LineTo(dc, x2, y2);
}

bool checkMenu(unsigned item)
{
	return CheckMenuItem(GetMenu(hWin), item,
		MF_BYCOMMAND + MF_CHECKED) != unsigned(-1);
}

bool uncheckMenu(unsigned item)
{
	return CheckMenuItem(GetMenu(hWin), item,
		MF_BYCOMMAND + MF_UNCHECKED)  != unsigned(-1);
}

int minesTab[]={12, 15, 18, 21, 24, 12, 15, 18, 21, 24};
int triangTab[]={1, 1, 1, 1, 1, 0, 0, 0, 0, 0};

int getScoreTab(int _triang, int _NminesRel)
{
	for(int i=0; i<sizeA(minesTab); i++){
		if(minesTab[i]==_NminesRel && triangTab[i]==_triang) return i;
	}
	return -1;
}
//---------------------------------------------------------------------------
void deleteini()
{
	HKEY key;
	DWORD i;

	delreg=true;
	if(RegDeleteKey(HKEY_CURRENT_USER, subkey)==ERROR_SUCCESS){
		if(RegOpenKey(HKEY_CURRENT_USER,
			"Software\\Petr Lastovicka", &key)==ERROR_SUCCESS){
			i=1;
			RegQueryInfoKey(key, 0, 0, 0, &i, 0, 0, 0, 0, 0, 0, 0);
			RegCloseKey(key);
			if(!i)
			 RegDeleteKey(HKEY_CURRENT_USER, "Software\\Petr Lastovicka");
		}
	}
}

void writeini()
{
	HKEY key;

	if(delreg) return;
	if(RegCreateKey(HKEY_CURRENT_USER, subkey, &key)!=ERROR_SUCCESS)
		msg(lng(735, "Cannot write to Windows registry"));
	else{
		for(int k=0; k<sizeA(regVal); k++)
			RegSetValueEx(key, regVal[k].s, 0, REG_DWORD,
				(BYTE *)regVal[k].i, sizeof(int));
		RegSetValueEx(key, "language", 0, REG_SZ, (BYTE *)lang, (DWORD)strlen(lang)+1);
		RegSetValueEx(key, "colors", 0, REG_BINARY, (BYTE *)colors, sizeof(colors));
		RegCloseKey(key);
	}
}

void writeScore()
{
	HKEY key;
	DWORD i;

	for(char *s=(char*)score; s<((char*)score)+sizeof(score); s++)
	 if(*s){
		 if(RegCreateKey(HKEY_LOCAL_MACHINE, subkey, &key)==ERROR_SUCCESS){
			 RegSetValueEx(key, "SCORE", 0, REG_BINARY, (BYTE *)score, sizeof(score));
			 RegCloseKey(key);
		 }
		 return;
	 }
	//table is empty => delete it
	if(RegDeleteKey(HKEY_LOCAL_MACHINE, subkey)==ERROR_SUCCESS){
		if(RegOpenKey(HKEY_LOCAL_MACHINE,
			"Software\\Petr Lastovicka", &key)==ERROR_SUCCESS){
			i=1;
			RegQueryInfoKey(key, 0, 0, 0, &i, 0, 0, 0, 0, 0, 0, 0);
			RegCloseKey(key);
			if(!i)
			 RegDeleteKey(HKEY_LOCAL_MACHINE, "Software\\Petr Lastovicka");
		}
	}
}
//---------------------------------------------------------------------------
DWORD getVer()
{
	HRSRC r;
	HGLOBAL h;
	void *s;
	VS_FIXEDFILEINFO *v;
	UINT i;

	r=FindResource(0, (TCHAR*)VS_VERSION_INFO, RT_VERSION);
	h=LoadResource(0, r);
	s=LockResource(h);
	if(!s || !VerQueryValue(s, "\\", (void**)&v, &i)) return 0;
	return v->dwFileVersionMS;
}

BOOL CALLBACK AboutProc(HWND hWnd, UINT msg, WPARAM wP, LPARAM)
{
	char buf[48];
	DWORD d;

	switch(msg){
		case WM_INITDIALOG:
			setDlgTexts(hWnd, 11);
			d=getVer();
			sprintf(buf, "%d.%d", HIWORD(d), LOWORD(d));
			SetDlgItemText(hWnd, 101, buf);
			return TRUE;

		case WM_COMMAND:
			int cmd=LOWORD(wP);
			switch(cmd){
				case 123:
					GetDlgItemTextA(hWnd, cmd, buf, sizeA(buf)-13);
					if(!strcmp(lang, "Èesky")) strcat(buf, "/indexCS.html");
					ShellExecuteA(0, 0, buf, 0, 0, SW_SHOWNORMAL);
				case IDOK:
				case IDCANCEL:
					EndDialog(hWnd, wP);
					return TRUE;
			}
			break;
	}
	return 0;
}
//---------------------------------------------------------------------------
//player name dialog procedure
BOOL CALLBACK NameProc(HWND hWnd, UINT msg, WPARAM wP, LPARAM)
{
	switch(msg){

		case WM_INITDIALOG:
			setDlgTexts(hWnd, 610);
			SetDlgItemText(hWnd, 101, playerName);
			return 1;

		case WM_COMMAND:
			wP=LOWORD(wP);
			if(wP==IDCANCEL || wP==535 || wP==534){
				if(wP!=IDCANCEL)
					GetDlgItemText(hWnd, 101, playerName, sizeof(playerName)-1);
				EndDialog(hWnd, wP);
			}
			break;
	}
	return 0;
}
//---------------------------------------------------------------------------

int ordScore(TScore &s)
{
	return s.speed + s.width*s.height/6;
}

BOOL CALLBACK ScoreProc(HWND hWnd, UINT msg, WPARAM wP, LPARAM lP);

void addScore(TScore &s, int tabInd, bool askName)
{
	int i, dlg;

	if(tabInd<0) return;
	TScore *scoreTable= ::score[tabInd];
	//sort
	for(i=0; i<Dscore; i++){
		if(ordScore(s) >= ordScore(scoreTable[i])) break;
	}
	if(i<Dscore){
		if(!memcmp(&s, &scoreTable[i], sizeof(TScore))) return;
		//ask for name
		dlg=103;
		if(askName){
			dlg= (int)DialogBoxParam(inst, "NAME",
				hWin, (DLGPROC)NameProc, 0);
			strcpy(s.name, playerName);
		}
		if(dlg!=IDCANCEL){
			//insert new line to table
			lastScore[tabInd]=i;
			for(int j=Dscore-1; j>i; j--)  scoreTable[j]=scoreTable[j-1];
			scoreTable[i]=s;
			//show table
			if(dlg!=535)
				DialogBoxParam(inst, "HISCORE", hWin, (DLGPROC)ScoreProc, 0);
		}
	}
}

void addScore()
{
	TScore s;
	SYSTEMTIME t;

	//fill in structure
	if(playtime<=0) return;
	s.speed=(short)rate;
	GetLocalTime(&t);
	s.date.wDay=(BYTE)t.wDay;
	s.date.wMonth=(BYTE)t.wMonth;
	s.date.wYear=t.wYear;
	s.date.wMinute=(BYTE)t.wMinute;
	s.date.wHour=(BYTE)t.wHour;
	s.playtime=(short)playtime;
	s.height=(BYTE)height;
	s.width=(BYTE)width;
	s.level=(short)level;

	addScore(s, getScoreTab(triang, NminesRel), true);
}
//---------------------------------------------------------------------------
void initPenBrush()
{
	for(int *p=penIndex; *p>=0; p++)
		pens[*p]=CreatePen(PS_SOLID, 1, colors[*p]);
	for(int *p=brushIndex; *p>=0; p++)
		brushes[*p]=CreateSolidBrush(colors[*p]);
	
	COLORREF c=colors[clField];
	const double K1=0.4;
	penDark=CreatePen(PS_SOLID, 1, RGB(int(GetRValue(c)*K1), int(GetRValue(c)*K1), int(GetBValue(c)*K1)));
	const double K2=1.3;
	penLight=CreatePen(PS_SOLID, 1, RGB(min(255, int(GetRValue(c)*K2)), 
		min(255, int(GetRValue(c)*K2)), min(255, int(GetBValue(c)*K2))));
}

void delPenBrush()
{
	SelectObject(dc, GetStockObject(NULL_PEN));
	SelectObject(dc, GetStockObject(NULL_BRUSH));
	for(int *p=penIndex; *p>=0; p++)
		DeleteObject(pens[*p]);
	for(int *p=brushIndex; *p>=0; p++)
		DeleteObject(brushes[*p]);
	DeleteObject(penDark);
	DeleteObject(penLight);
}
//---------------------------------------------------------------------------
//redraw block
void paint1(PSquare p)
{
	RECT rc;
	HBRUSH br;
	int cl;
	int x, y, xt, yt;
	bool isOpened;

	if(!p) return;
	isOpened = p->opened || (p==pressed && (!p->flag || flagOpen));
	//calculate block position
	x = (int(p-board)/(height+2)-2)*gridx + x0;
	y = (int(p-board)%(height+2)-1)*gridy + y0;
	rc.left=x;
	rc.top=y;
	rc.right=x+gridx;
	rc.bottom=y+gridy;
	x += (gridx>>1);
	y += (gridy>>1);

	//paint block
	cl=clField;
	if(vizv && p->def) cl=clvi;
	if(looser && p==bum) cl=clBlast;
	if(isOpened) cl=clOpened;
	br= brushes[cl];
	if(triang){ //triangular
		POINT b[3];
		b[0].x= rc.right;
		b[1].x= rc.left+1;
		b[2].x= rc.right + gridx-1;
		if(p->diroff==diroff12a){ //triangle on base side
			b[0].y= rc.top;
			b[2].y= b[1].y= rc.bottom-1;
			y+= (gridy>>3);
		}
		else{
			assert(p->diroff==diroff12b); //triangle on vertex
			b[0].y= rc.bottom-1;
			b[2].y= b[1].y= rc.top;
			y-= (gridy>>3);
		}
		SelectObject(dc, GetStockObject(NULL_PEN));
		SelectObject(dc, br);
		Polygon(dc, b, 3);
		SelectObject(dc, isOpened ? pens[clOpened] : penLight);
		if(p->diroff==diroff12a){
			MoveToEx(dc, b[1].x, b[1].y, 0);
			LineTo(dc, b[0].x, b[0].y);
			LineTo(dc, b[2].x, b[2].y);
		}
		else{
			MoveToEx(dc, b[1].x, b[1].y, 0);
			LineTo(dc, b[2].x, b[2].y);
		}
		SelectObject(dc, isOpened ? pens[clGrid] : penDark);
		if(p->diroff==diroff12a){
			LineTo(dc, b[1].x-1, b[1].y);
		}
		else{
			LineTo(dc, b[0].x, b[0].y);
			LineTo(dc, b[1].x, b[1].y);
		}
		x= rc.left+gridx;
	}
	else{ //rectangular
		if(!isOpened){
			rc.left++;
			rc.top++;
		}
		rc.bottom--;
		rc.right--;
		FillRect(dc, &rc, br);
		SelectObject(dc, isOpened ? pens[clGrid] : penLight);
		if(isOpened){
			MoveToEx(dc, rc.right, rc.top, 0);
		}
		else{
			rc.left--;
			rc.top--;
			MoveToEx(dc, rc.left, rc.bottom, 0);
			LineTo(dc, rc.left, rc.top);
			LineTo(dc, rc.right, rc.top);
			SelectObject(dc, penDark);
		}
		LineTo(dc, rc.right, rc.bottom);
		LineTo(dc, rc.left-1, rc.bottom);
	}

	xt= x-xCh;
	yt= y-yCh;
	if(isOpened){
		if(p->Nmin && p->opened){
			//print number
			char ch[2];
			SetTextColor(dc, colors[p->Nmin]);
			if(p->Nmin>9){
				ch[0]= '1';
				ch[1]= (char)(p->Nmin + '0'-10);
				TextOut(dc, xt-xCh, yt, ch, 2);
			}
			else{
				ch[0]= (char)(p->Nmin + '0');
				TextOut(dc, xt, yt, ch, 1);
				if(p->Nmin==1) TextOut(dc, xt, yt, ch, 1);
			}
		}
	}
	else{
		if(p->flag || gameOver && !looser && p->mine){
			if(looser && !p->mine){
				//badly marked block
				charOut(xt, yt, 'X', colors[clX]);
			}
			else{
				//flag
				charOut(xt+2, yt-5, '>', 0x50c0);
				SelectObject(dc, GetStockObject(BLACK_PEN));
				line(x-2, y-9, x-2, y+9);
			}
		}
		else if(p->mine && (looser || vizmin)){
			//mine
			int g= gridx+gridy;
			if(g<48){
				ImageList_Draw(himg[0], 0, dc, x-6, y-6, ILD_TRANSPARENT);
			}
			else if(g<72){
				ImageList_Draw(himg[1], 0, dc, x-8, y-8, ILD_TRANSPARENT);
			}
			else{
				ImageList_Draw(himg[2], 0, dc, x-12, y-12, ILD_TRANSPARENT);
			}
		}
	}
}

void paint(PSquare p)
{
	initPenBrush();
	paint1(p);
	delPenBrush();
}
//---------------------------------------------------------------------------
void drawGraph(int cl)
{
	int i, y, dbl;
	float x, dx;
	bool b;

	HGDIOBJ oldPen=SelectObject(dc, CreatePen(PS_SOLID, 1, colors[cl]));
	dbl=1;
	for(i=0; i<Dgraf; i++){
		if(2*grafTab[i]>y0) dbl=0;
	}
	x=remainW;
	dx= (Width-95-x)/Dgraf;
	b=true;
	for(i=startGraf; i<Dgraf; i++){
		y= y0 - (grafTab[(Igraf+i)%Dgraf] << dbl) - 1;
		if(b){ MoveToEx(dc, (int)x, y, NULL); b=false; }
		else LineTo(dc, (int)x, y);
		x+=dx;
	}
	DeleteObject(SelectObject(dc, oldPen));
}
//---------------------------------------------------------------------------
void graph()
{
	int i, S;

	clickSum+=clicks;
	if(clicks<0) clicks=0;
	speedTab[Ispeed++]=clicks;
	if(Ispeed==Dspeed) Ispeed=0;
	clicks=0;

	//erase graph
	drawGraph(clBkgnd);

	//calculate next value
	S=0;
	for(i=0; i<Dspeed; i++) S+=speedTab[i];
	grafTab[Igraf++]=S;
	if(Igraf==Dgraf) Igraf=0;
	if(startGraf>0) startGraf--;

	//show graph
	drawGraph(clgraf);
}
//---------------------------------------------------------------------------
void invalidateQuest()
{
	RECT rc;
	rc.top=Height-HIWORD(GetDialogBaseUnits());
	rc.bottom=Height;
	rc.left= Width-50; rc.right=Width;
	InvalidateRect(hWin, &rc, TRUE);
}
//---------------------------------------------------------------------------
//right mouse button - put or delete flag
void mark(PSquare p)
{
	RECT rc;

	assert(p>=board && p<boardk);
	p->flag= !p->flag;
	if(p->flag) remainm--; else remainm++;

	//draw flag or empty block
	paint(p);

	//print number of remaining mines
	rc.top=2; rc.bottom=y0-1;
	rc.left=1; rc.right=remainW;
	InvalidateRect(hWin, &rc, TRUE);
}
//---------------------------------------------------------------------------
void endGame()
{
	DWORD t = GetTickCount()-time0;
	if(t>1000000000) t=0;
	playtime= t/1000;
	if(playtime) rate= (int)((clickSum*1000000L + (t>>1))/t);
	gameOver=true;
	InvalidateRect(hWin, NULL, TRUE);
}
//---------------------------------------------------------------------------
//set block size according to window size
//scale down window which is larger than screen
void resize()
{
	RECT rc, rcs;
	int w, h, sx, sy, wc, hc, wb, hb, border;

	border = 2*GetSystemMetrics(92 /*SM_CXPADDEDBORDER*/);
	wb= 20 + border;
	hb= 14 + 2*HIWORD(GetDialogBaseUnits()) + GetSystemMetrics(SM_CYMENU) + GetSystemMetrics(SM_CYCAPTION) + border;
	amin(gridy, triang ? minGridY : 18);
	SystemParametersInfo(SPI_GETWORKAREA, 0, &rcs, 0);
	sx=rcs.right-rcs.left;
	sy=rcs.bottom-rcs.top;
	GetWindowRect(hWin, &rc);
	//window size cannot be greater than desktop size
	amax(gridy, int((sx-wb)/(width+triang)*(triang ? 1.732 : 1)));
	amax(gridy, (sy-hb)/height);
	gridx=gridy;
	if(triang){
		amax(gridy, maxGridY);
		for(;;){
			gridx=dxy[gridy-minGridY];
			if(gridx) break;
			gridy++;
		}
	}
	wc=(width+triang)*gridx;
	w=wc+wb;
	hc=height*gridy;
	h=hc+hb;
	amin(w, 380*GetDeviceCaps(dc, LOGPIXELSX)/96);
	//move window
	amax(rc.left, rcs.right-w);
	amax(rc.top, rcs.bottom-h);
	SetWindowPos(hWin, 0, rc.left, rc.top, w, h, SWP_NOZORDER|SWP_NOCOPYBITS);
	//calculate field border coordinates
	GetClientRect(hWin, &rc);
	Width=rc.right;
	Height=rc.bottom;
	x0 = (rc.right-wc)/2;
	y0 = (rc.bottom-hc)/2;
	xk = x0 + wc;
	yk = y0 + hc;
	//redraw window
	InvalidateRect(hWin, NULL, TRUE);
}
//---------------------------------------------------------------------------
void checkMenus()
{
	checkMenu(level+200);
	if(checkMenu(width*100+height)) uncheckMenu(106); else checkMenu(106);
	if(checkMenu(NminesRel)) uncheckMenu(107); else checkMenu(107);
	checkMenu(115-triang);
	uncheckMenu(114+triang);
}
//---------------------------------------------------------------------------
//message WM_PAINT
void repaint()
{
	PSquare p;
	int y;
	PAINTSTRUCT ps;

	BeginPaint(hWin, &ps);

	SetTextColor(dc, colors[clText]);

	LONG units=GetDialogBaseUnits();
	xCh=LOWORD(units)/2;
	yCh=HIWORD(units)/2 - 1;
	y=Height-HIWORD(units);
	if(ps.rcPaint.bottom > yk){
		//status bar
		numOut(5, y, lng(600, "Width"), width);
		numOut(110, y, lng(601, "Height"), height);
		numOut(220, y, lng(602, "Mines"), Nmines);
		if(canOpen==0 && playtime>=0 && !gameOver){
			TextOut(dc, Width - 50, y, "(?)", 3);
		}
		if(gameOver && playtime>4){
			numOut(329, y, lng(603, "Rate"), rate);
		}
	}
	if(ps.rcPaint.top < y0){
		//area below menu
		drawGraph(clgraf);
		if(playtime>=0){
			SetTextAlign(ps.hdc, TA_RIGHT);
			numOut(440, 2, lng(604, "Time"), playtime);
			SetTextAlign(ps.hdc, TA_LEFT);
		}
		if(remainm<Nmines) numOut(9, 2,
			lng(605, "Remains"), remainm);
		/*if(gameOver){
			TextOut(dc, Width/2 - 20, 2, "Konec hry",9);
			}*/
	}

	if(ps.rcPaint.top <= yk  &&  ps.rcPaint.bottom >= y0){
		//field
		if(board){
			initPenBrush();
			if(triang){
				for(p=board; p<boardk; p++)
					if(p->diroff==diroff12b && p->lock<99) paint1(p);
				for(p=board; p<boardk; p++)
					if(p->diroff==diroff12a && p->lock<99) paint1(p);
			}
			else{
				for(p=board; p<boardk; p++)
					if(p->lock<99) paint1(p); //all block except border
			}
			delPenBrush();
		}
	}

	EndPaint(hWin, &ps);
}
//---------------------------------------------------------------------------
//new game - generate mines randomly
void newGame()
{
	PSquare p;
	int k, *s, x, y;
	bool outside;

	dellist(recycle);
	dellist(recycle2);
	delete[] board;

	aminmax(NminesRel, 1, 40);
	aminmax(width, 5, 50);
	aminmax(height, 5, 30);
	aminmax(level, 1, 5);
	//allocate field
	k =(width+4)*(height+2);
	board = new TSquare[k];
	boardk = board+k;
	diroff12a[0]= diroff12b[0]= diroff8[0]= 1;
	diroff12a[1]= diroff12b[1]= diroff8[1]= -1;
	diroff12a[2]= diroff12b[2]= diroff8[2]= (height+2);
	diroff12a[3]= diroff12b[3]= diroff8[3]= -(height+2);
	diroff12a[4]= diroff12b[4]= diroff8[4]= (height+2)-1;
	diroff12a[5]= diroff12b[5]= diroff8[5]= -(height+2)-1;
	diroff12a[6]= diroff12b[6]= diroff8[6]= (height+2)+1;
	diroff12a[7]= diroff12b[7]= diroff8[7]= -(height+2)+1;
	diroff12a[8]= diroff12b[8]= 2*(height+2);
	diroff12a[9]= diroff12b[9]= -2*(height+2);
	diroff12a[10]= 2*(height+2)+1;
	diroff12a[11]= -2*(height+2)+1;
	diroff12b[10]= 2*(height+2)-1;
	diroff12b[11]= -2*(height+2)-1;

	//initialize global variables
	gameOver=false;
	looser=false;
	playtime=-1;
	Nmines=width*height*NminesRel/100 +1;
	remainm=Nmines;
	remaino=width*height-Nmines;
	inMin=0;
	inFree=width*height;
	canOpen=0;
	divergence=0;
	wrong.nxt= wrong.prv= &wrong;
	if(!vizmin && !vizv && !debug) cheat=false;

	//reset graph
	memset(grafTab, 0, sizeof(grafTab));
	memset(speedTab, 0, sizeof(speedTab));
	clickSum=clicks=0;
	Igraf=Ispeed=0;
	startGraf=Dgraf;

	//initialize field
	for(p=board; p<boardk; p++){
		outside= p<board+2*(height+2) || p>=boardk-2*(height+2) || (p-board+1)%(height+2)<2;
		p->mine= false;
		p->flag= false;
		p->opened= false;
		p->lockedm= p->lockedv= false;
		p->def= outside;
		p->inside= !outside;
		p->Nmin=0;
		p->Nopened=0;
		p->nxtOpened[0]=0;
		p->mayMine=0;
		p->lock= outside ? 100 : 0;
		p->divergence=0;
		p->item=0;
		p->diroff=diroff8;
	}
	if(triang){
		p=board;
		for(x=0; x<width+4; x++){
			for(y=0; y<height+2; y++){
				p->diroff= ((x^y)&1) ? diroff12a : diroff12b;
				p++;
			}
		}
		assert(p==boardk);
	}
	for(p=board; p<boardk; p++){
		if(!p->def){
			p->mayFree=0;
			for(s=p->diroff; *s; s++){
				if(!(p+(*s))->def) p->mayFree++;
			}
		}
	}
	//redraw window and adjust size
	resize();
	//generate mines
	addMines(Nmines);
	//check menu items
	checkMenus();
}
//---------------------------------------------------------------------------
void setMinesRel(int m)
{
	if(m>0){
		if(m!=NminesRel){
			uncheckMenu(NminesRel);
			NminesRel=m;
			newGame();
		}
	}
}

void setSize(int x, int y)
{
	if(x<=0) x=width;
	if(y<=0) y=height;
	if(x!=width || y!=height){
		uncheckMenu(width*100+height);
		width=x;
		height=y;
		newGame();
	}
}

void setLevel(int l)
{
	if(l>0){
		amax(l, 5);
		uncheckMenu(level+200);
		level=l;
		checkMenu(level+200);
	}
}
//---------------------------------------------------------------------------
void readini()
{
	HKEY key;
	DWORD d;
	int i;

	if(RegOpenKey(HKEY_CURRENT_USER, subkey, &key)==ERROR_SUCCESS){
		for(int k=0; k<sizeA(regVal); k++){
			d=sizeof(int);
			RegQueryValueEx(key, regVal[k].s, 0, 0, (BYTE *)regVal[k].i, &d);
		}
		d=sizeof(lang);
		RegQueryValueEx(key, "language", 0, 0, (BYTE *)lang, &d);
		d=sizeof(colors);
		RegQueryValueEx(key, "colors", 0, 0, (BYTE *)colors, &d);
		RegCloseKey(key);
	}

	if(RegOpenKey(HKEY_LOCAL_MACHINE, subkey, &key)==ERROR_SUCCESS){
		d=sizeof(score8);
		RegQueryValueEx(key, "HISCORE", 0, 0, (BYTE *)score8, &d);
		d=sizeof(score12);
		RegQueryValueEx(key, "HISCORE3", 0, 0, (BYTE *)score12, &d);
		d=sizeof(score);
		RegQueryValueEx(key, "SCORE", 0, 0, (BYTE *)score, &d);
		RegCloseKey(key);
	}
	for(i=0; i<10; i++){
		TScore s;
		convertScore(s, score8[i]);
		addScore(s, getScoreTab(0, score8[i].NminesRel), false);
		convertScore(s, score12[i]);
		addScore(s, getScoreTab(1, score12[i].NminesRel), false);
	}
	strcpy(playerName, score[0][0].name);
	for(i=0; i<Dtab; i++) lastScore[i]=-1;
}
//---------------------------------------------------------------------------
//dialog procedure
BOOL CALLBACK ColorProc(HWND hWnd, UINT msg, WPARAM wP, LPARAM lP)
{
	static bool chng;
	static CHOOSECOLOR chc;
	static COLORREF clold[Ncl];
	DRAWITEMSTRUCT *di;
	HBRUSH br;
	int cmd;

	switch(msg){
		case WM_INITDIALOG:
			setDlgTexts(hWnd, 611);
			memcpy(clold, colors, sizeof(clold));
			chng=false;
			return TRUE;

		case WM_DRAWITEM:
			di = (DRAWITEMSTRUCT*)lP;
			br= CreateSolidBrush(colors[di->CtlID-100]);
			FillRect(di->hDC, &di->rcItem, br);
			DeleteObject(br);
			break;

		case WM_COMMAND:
			cmd=LOWORD(wP);
			switch(cmd){
				default: //colors
					chc.lStructSize= sizeof(CHOOSECOLOR);
					chc.hwndOwner= hWnd;
					chc.hInstance= 0;
					chc.rgbResult= colors[cmd-100];
					chc.lpCustColors= custom;
					chc.Flags= CC_RGBINIT|CC_FULLOPEN;
					if(ChooseColor(&chc)){
						colors[cmd-100]=chc.rgbResult;
						InvalidateRect(hWin, 0, cmd-100==clBkgnd);
						InvalidateRect(GetDlgItem(hWnd, cmd), 0, TRUE);
						chng=true;
					}
					break;
				case IDCANCEL:
					if(chng){
						memcpy(colors, clold, sizeof(clold));
						InvalidateRect(hWin, 0, TRUE);
					}
				case IDOK:
					EndDialog(hWnd, cmd);
			}
			break;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
//dialog procedure
BOOL CALLBACK CustomProc(HWND hWnd, UINT msg, WPARAM wP, LPARAM lP)
{
	static bool dlg1;
	int cmd;

	switch(msg){

		case WM_INITDIALOG:
			setDlgTexts(hWnd, 612);
			dlg1 = (lP==106);
			SetDlgItemInt(hWnd, 103, dlg1 ? xmdlg : width, FALSE);
			SetDlgItemInt(hWnd, 104, dlg1 ? ymdlg : height, FALSE);
			SetDlgItemInt(hWnd, 105, dlg1 ? NminesRel : NminesReldlg, FALSE);
			if(dlg1) return TRUE;
			//activate editbox Mines:
			SetFocus(GetDlgItem(hWnd, 105));
			SendDlgItemMessage(hWnd, 105, EM_SETSEL, 0, 2);
			break;

		case WM_COMMAND:
			cmd=LOWORD(wP);
			if(cmd==IDOK){
				setSize(GetDlgItemInt(hWnd, 103, NULL, FALSE),
										GetDlgItemInt(hWnd, 104, NULL, FALSE));
				setMinesRel(GetDlgItemInt(hWnd, 105, NULL, FALSE));
				if(dlg1) xmdlg=width, ymdlg=height;
				else NminesReldlg=NminesRel;
				EndDialog(hWnd, cmd);
			}
			if(cmd==IDCANCEL) EndDialog(hWnd, cmd);
			break;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
void langChanged()
{
	static int subId[]={405, 404, 403, 402, 400, 401};
	loadMenu(hWin, "MENU1", subId);

	MENUITEMINFO mii;
	mii.cbSize=sizeof(mii);
	mii.fMask=MIIM_TYPE;
	mii.fType=MFT_STRING;
	mii.dwTypeData=lng(450, 0);
	if(mii.dwTypeData) SetMenuItemInfo(GetMenu(hWin), 1209, FALSE, &mii);
	mii.dwTypeData=lng(451, 0);
	if(mii.dwTypeData) SetMenuItemInfo(GetMenu(hWin), 2315, FALSE, &mii);
	mii.dwTypeData=lng(452, 0);
	if(mii.dwTypeData) SetMenuItemInfo(GetMenu(hWin), 3218, FALSE, &mii);
	DrawMenuBar(hWin);

	checkMenus();
	SetWindowText(hWin, title= lng(606, "Clever Mines"));
	InvalidateRect(hWin, 0, TRUE);
}
//---------------------------------------------------------------------------
void eraseBkgnd()
{
	int i, j, x, y;
	RECT rc;
	POINT b[64];

	HBRUSH brush=CreateSolidBrush(colors[clBkgnd]);

	//paint area around field
	rc.left= 0;
	rc.right=Width;
	rc.top=0;
	rc.bottom= y0;
	FillRect(dc, &rc, brush); //top
	rc.top= yk;
	rc.bottom=Height;
	FillRect(dc, &rc, brush); //bottom
	if(triang){
		//left
		x=x0+1;
		for(i=0, y=y0; i<=height; i++, y+=gridy){
			b[i].y=y;
			b[i].x= (i&1) ? x : x+gridx;
		}
		b[i].y=y;
		b[i].x=0;
		i++;
		b[i].y=y0;
		b[i].x=0;
		i++;
		HRGN rgn = CreatePolygonRgn(b, i, ALTERNATE);
		FillRgn(dc, rgn, brush);
		DeleteObject(rgn);
		//right
		x=x0+width*gridx;
		j=width;
		for(i=0, y=y0; i<=height; i++, y+=gridy){
			b[i].y=y;
			b[i].x= (j&1) ? x : x+gridx;
			j++;
		}
		b[i].y=y;
		b[i].x=Width;
		i++;
		b[i].y=y0;
		b[i].x=Width;
		i++;
		rgn = CreatePolygonRgn(b, i, ALTERNATE);
		FillRgn(dc, rgn, brush);
		DeleteObject(rgn);
	}
	else{
		rc.right= x0;
		rc.top=y0;
		rc.bottom= yk+1;
		FillRect(dc, &rc, brush); //left
		rc.left= xk;
		rc.right=Width;
		FillRect(dc, &rc, brush); //reight
	}
	DeleteObject(brush);
}
//---------------------------------------------------------------------------
//dialog - best scores table
BOOL CALLBACK ScoreProc(HWND hWnd, UINT msg, WPARAM wP, LPARAM)
{
	static int tabInd;
	const int colX[]={104, 127, 164, 192, 231, 262};
	int cmd;

	switch(msg){

		case WM_INITDIALOG:
			setDlgTexts(hWnd);
			tabInd= getScoreTab(triang, NminesRel);
			if(tabInd>=0){
				SetFocus(GetDlgItem(hWnd, NminesRel+200));
				return FALSE;
			}
			return TRUE;

		case WM_COMMAND:
			cmd=LOWORD(wP);
			if(cmd==IDOK){
				if(playtime<0) setMinesRel(minesTab[tabInd]);
				EndDialog(hWnd, cmd);
			}
			if(cmd==IDCANCEL) EndDialog(hWnd, cmd);
			if(cmd>=200 && cmd<300){
				tabInd= getScoreTab(triang, cmd-200);
				InvalidateRect(hWnd, 0, TRUE);
			}
			break;

		case WM_PAINT:{
			PAINTSTRUCT ps;
			RECT rc;
			char buf[64];

			if(tabInd<0) tabInd= (1-triang)*Dtab/2;
			strcpy(buf, lng(607, "High scores - "));
			strcat(buf, triang ? lng(608, "triangular field") :
				lng(609, "rectangular field"));
			sprintf(strchr(buf, 0), " - %d%%", minesTab[tabInd]);
			SetWindowText(hWnd, buf);
			TScore *s= score[tabInd];
			BeginPaint(hWnd, &ps);
			SetBkMode(ps.hdc, TRANSPARENT);
			int dpi=GetDeviceCaps(ps.hdc, LOGPIXELSY);
			for(int i=0; i<Dscore; i++, s++){
				if(!s->playtime) continue;
				int y=(24*i+35)*dpi/96;
				SetTextColor(ps.hdc, lastScore[tabInd]==i ? colors[cllastScore] : 0);
				TextOut(ps.hdc, 10, y, s->name, (int)strlen(s->name)); //name
				SetTextAlign(ps.hdc, TA_RIGHT);
				for(int j=0; j<6; j++){
					rc.right=colX[j];
					switch(j){
						case 0: //date
							SYSTEMTIME t;
							t.wYear=s->date.wYear;
							t.wMonth=s->date.wMonth;
							t.wDay=s->date.wDay;
							t.wHour=s->date.wHour;
							t.wMinute=s->date.wMinute;
							t.wSecond=t.wMilliseconds=0;
							GetDateFormat(0, 0, &t, 0, buf, sizeof(buf));
							break;
						case 1: //hour
							GetTimeFormat(LOCALE_USER_DEFAULT,
								TIME_FORCE24HOURFORMAT|TIME_NOSECONDS,
								&t, 0, buf, sizeof(buf));
							if(strlen(buf)>6) rc.right+=10;
							break;
						case 2: //field size
							sprintf(buf, "%dx%d", s->width, s->height);
							break;
						case 3: //level
							sprintf(buf, "%d", s->level);
							break;
						case 4: //time
							sprintf(buf, "%d", s->playtime);
							break;
						case 5: //rate
							sprintf(buf, "%d", s->speed);
							break;
					}
					rc.left=rc.top=rc.bottom=0;
					MapDialogRect(hWnd, &rc);
					TextOut(ps.hdc, rc.right, y, buf, (int)strlen(buf));
				}
				SetTextAlign(ps.hdc, TA_LEFT);
			}
			EndPaint(hWnd, &ps);
			break;
		}
	}
	return FALSE;
}
//---------------------------------------------------------------------------
//dialog Options
BOOL CALLBACK OptionsProc(HWND hWnd, UINT msg, WPARAM wP, LPARAM)
{
	int cmd;

	switch(msg){
		case WM_INITDIALOG:
			setDlgTexts(hWnd, 613);
			CheckDlgButton(hWnd, 529, isEasyStart ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWnd, 530, !flagOpen ? BST_CHECKED : BST_UNCHECKED);
			CheckRadioButton(hWnd, 531, 532, 532-fastOpen);
			return TRUE;

		case WM_COMMAND:
			cmd=LOWORD(wP);
			if(cmd==IDOK){
				isEasyStart= IsDlgButtonChecked(hWnd, 529);
				flagOpen= !IsDlgButtonChecked(hWnd, 530);
				fastOpen= IsDlgButtonChecked(hWnd, 531);
				EndDialog(hWnd, cmd);
			}
			if(cmd==IDCANCEL) EndDialog(hWnd, cmd);
			break;
	}
	return FALSE;
}
//---------------------------------------------------------------------------
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT mesg, WPARAM wP, LPARAM lP)
{
	PSquare p;
	RECT rc;
	int cmd;

	switch(mesg){

		case WM_COMMAND:
			cmd=LOWORD(wP);
			if(setLang(cmd)) break;
			switch(cmd){
				case 100: //new game
					newGame();
					break;
				case 101: //zoom in
					if(!triang || gridy<maxGridY){
						gridy++;
						resize();
					}
					break;
				case 102: //zoom out
					if(triang){
						if(gridy<=minGridY) break;
						do{
							gridy--;
						} while(!dxy[gridy-minGridY]);
					}
					else gridy--;
					resize();
					break;
				case 103: //show mines (cheat)
					cheat=true;
					vizmin= !vizmin;
					InvalidateRect(hWnd, NULL, FALSE);
					break;
				case 104: //highlight definite blocks (cheat)
					cheat=true;
					vizv= !vizv;
					InvalidateRect(hWnd, NULL, FALSE);
					break;
				case 108:
					cheat=true;
					debug= !debug;
					if(debug) vizmin=true, vizv=true;
					InvalidateRect(hWnd, NULL, FALSE);
					break;
				case 110: //delete settings
					deleteini();
					break;
				case 105: //exit
					SendMessage(hWin, WM_CLOSE, 0, 0);
					break;
				case 106: //custom size
				case 107: //custom number of mines
					DialogBoxParam(inst, "CUSTOM", hWnd, (DLGPROC)CustomProc, cmd);
					break;
				case 111: //best scores
					DialogBoxParam(inst, "HISCORE", hWnd, (DLGPROC)ScoreProc, cmd);
					break;
				case 201: case 202: case 203: case 204: case 205: //level
					setLevel(cmd-200);
					break;
				case 112:
					ShowWindow(hWnd, SW_MINIMIZE);
					break;
				case 120: //readme
				{
					char buf[256];
					getExeDir(buf, lng(13, "Miny.txt"));
					if(ShellExecute(0, "open", buf, 0, 0, SW_SHOWNORMAL)==(HINSTANCE)ERROR_FILE_NOT_FOUND){
						msg(lng(739, "File %s not found"), buf);
					}
				}
					break;
				case 121: //about
					DialogBox(inst, "ABOUT", hWnd, (DLGPROC)AboutProc);
					break;
				case 114:
				case 115:
					if(triang!= 115-cmd){
						triang=115-cmd;
						newGame();
					}
					break;
				case 116: //change colors
					DialogBox(inst, "COLORS", hWnd, (DLGPROC)ColorProc);
					break;
				case 117: //delete best scores
					if(MessageBox(hWnd,
						lng(799, "Do you really want to delete all hiscores ?"), title,
						MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2) == IDYES){
						memset(score, 0, sizeof(score));
					}
					break;
				case 118: //options dialog
					DialogBox(inst, "OPTIONS", hWnd, (DLGPROC)OptionsProc);
					break;
				default:
					if(cmd<100) //change number of mines
					 setMinesRel(cmd);
					if(cmd>300) //change size
					 setSize(cmd/100, cmd%100);
			}
			break;

		case WM_PAINT:
			repaint();
			break;

		case WM_LBUTTONUP:
			if(isMouseDown){
				ReleaseCapture();
				isMouseDown=false;
				pressed=0;
				p=hit(lP);
				if(p && !gameOver){
					if(p->opened){
						if(openAround(p)) clicks++;
					}
					else{
						if(p->flag && flagOpen) mark(p);
						if(open(p)) clicks++;
						passAll();
					}
				}
			}
			break;

		case WM_MOUSEMOVE:
			if(isMouseDown){
				PSquare p0=pressed;
				pressed=hit(lP);
				if(p0!=pressed){
					paint(p0);
					paint(pressed);
				}
			}
			break;

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			if(HIWORD(lP)<y0 && gameOver) newGame();
			p=hit(lP);
			if(p && !gameOver){
				if(playtime<0){
					//start time after the first click
					time0= GetTickCount();
					playtime=0;
					SetTimer(hWin, 10, 1000, NULL);
					if(fastOpen) mesg=WM_LBUTTONDOWN;
					easyStart(p);
				}
				if(mesg==WM_LBUTTONDOWN){
					isMouseDown=true;
					if(fastOpen){
						SendMessage(hWnd, WM_LBUTTONUP, wP, lP);
					}
					else{
						SetCapture(hWnd);
						paint(pressed=p);
					}
				}
				if(mesg==WM_RBUTTONDOWN){
					if(p->opened){
						if(openAround(p)) clicks++;
					}
					else{
						mark(p);
						if(p->flag) clicks++; else clicks--;
					}
				}
				if(mesg==WM_MBUTTONDOWN) {
					if(p->opened) {
						if(openAround(p)) clicks++;
					}
				}
			}
			break;

		case WM_TIMER:
			if(!gameOver && playtime>=0){
				if(!IsIconic(hWnd)){
					int l=playtime;
					playtime= (GetTickCount()-time0)/1000;
					//redraw graph
					while(l++ < playtime) graph();
					//redraw time
					rc.top=2; rc.bottom=y0-1;
					rc.left=Width-timeW; rc.right=Width;
					InvalidateRect(hWnd, &rc, TRUE);
				}
				else{
					time0+=1000;
				}
			}
			break;

		case WM_MOVE:
			if(!IsIconic(hWin) && !IsZoomed(hWin)) {
				GetWindowRect(hWin, &rc);
				mainTop= rc.top;
				mainLeft= rc.left;
			}
			break;

		case WM_ERASEBKGND:
			eraseBkgnd();
			return 1;

		case WM_QUERYENDSESSION:
			writeini();
			writeScore();
			return TRUE;

		case WM_CLOSE:
			writeini();
			writeScore();
			DestroyWindow(hWnd);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hWnd, mesg, wP, lP);
	}
	return 0;
}
//---------------------------------------------------------------------------
int pascal WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPSTR, int cmdShow)
{
	const int minW[]={13, 18, 26}; //mine bitmap size
	MSG msg;
	WNDCLASS wc;
	HACCEL haccel;
	int i;

	inst=hInstance;

	//DPIAware
	typedef BOOL(WINAPI *TGetProcAddress)();
	TGetProcAddress getProcAddress = (TGetProcAddress) GetProcAddress(GetModuleHandle("user32"), "SetProcessDPIAware");
	if(getProcAddress) getProcAddress();

	//read settings from registry
	readini();
	initLang();
	for(i=0; i<3; i++){
		himg[i]=ImageList_LoadBitmap(inst, MAKEINTRESOURCE(20+i), minW[i], 1, RGB(248, 0, 0));
	}
	memset(custom, 200, sizeof(custom));

	wc.style=		CS_OWNDC;
	wc.lpfnWndProc=	MainWndProc;
	wc.cbClsExtra=		0;
	wc.cbWndExtra=		0;
	wc.hInstance=		hInstance;
	wc.hIcon=		LoadIcon(hInstance, MAKEINTRESOURCE(1));
	wc.hCursor=		LoadCursor(0, IDC_ARROW);
	wc.hbrBackground=	0;
	wc.lpszMenuName=	0;
	wc.lpszClassName=	"MinaWCls";
	if(!hPrevInst && !RegisterClass(&wc)) return 1;

	hWin = CreateWindowEx(0, "MinaWCls", "",
		WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX,
		mainLeft, mainTop, 50, 50,
		0, 0, hInstance, NULL
	);
	if(!hWin) return 2;
	dc=GetDC(hWin);
	SetBkMode(dc, TRANSPARENT);

	xmdlg=width; ymdlg=height; NminesReldlg=NminesRel;
	if(debug) vizmin=true, vizv=true;
	if(!debug) randomize();

	langChanged();
	newGame();
	ShowWindow(hWin, cmdShow);
	haccel=LoadAccelerators(hInstance, MAKEINTRESOURCE(3));

	while(GetMessage(&msg, 0, 0, 0)==TRUE){
		if(TranslateAccelerator(hWin, haccel, &msg)==0){
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	for(i=0; i<3; i++) ImageList_Destroy(himg[i]);
	return 0;
}
//---------------------------------------------------------------------------
