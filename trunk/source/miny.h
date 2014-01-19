/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */

struct Tlst2;

//---------------------------------------------------------------------------
struct TSquare  //block (square or triangle)
{
	bool
		 mine,   //mine is here
		 flag,   //marked by user
		 opened, //open, number is visible
		 def,    //is definite, (mine cannot be added or removed)
		 inside, //is inside field, (does not abut opened block)
		 lockedm,
		 lockedv,//only one possible reorder around open block
		 unknown;//surely not definite
	int
		 *diroff, //offsets to adjacent blocks
		 Nmin,    //count of adjacent mines (it is shown on open blocks)
		 Nopened, //count of adjacent open blocks
		 mayMine, //count of adjacent unlocked mines
		 mayFree, //count of adjacent unlocked open blocks
		 divergence, //how many mines were added at this open block during reordering
		 lock;    //0=block can be changed, 100=outside field
	long
		 prior;	//priority for backtracking
	Tlst2
		 *item;	//pointer to item in wrong list, or 0
	TSquare
		 *nxtOpened[13];	//adjacent open blocks, this array is zero terminated
};
typedef struct TSquare *PSquare;
//---------------------------------------------------------------------------

struct Tlst
{
	PSquare p;
	Tlst *nxt;
	Tlst(PSquare x) :p(x){};
};

struct Tlst2
{
	PSquare p;
	Tlst2 *nxt, *prv;
	Tlst2(PSquare x) :p(x){};
};

typedef Tlst *Plst;
typedef Tlst2 *Plst2;

//delete all items
template <class T> void dellist(T* &list)
{
	T *a, *a1;
	for(a=list; a; a=a1){
		a1=a->nxt;
		delete a;
	}
	list=0;
}

//---------------------------------------------------------------------------
template <class T> inline void amin(T &x, int m){ if(x<m) x=m; }
template <class T> inline void amax(T &x, int m){ if(x>m) x=m; }
template <class T> inline void aminmax(T &x, int l, int h){
	if(x<l) x=l;
	if(x>h) x=h;
}

#define sizeA(A) (sizeof(A)/sizeof(*A))

#ifndef __BORLANDC__
#include <time.h>
inline int random(int num){ return(int)(((long)rand()*num)/(RAND_MAX+1)); }
inline void randomize() { srand((unsigned)time(NULL)); }
#endif
//---------------------------------------------------------------------------
extern PSquare board, boardk, bum;
extern bool vizmin, vizv, debug, gameOver, looser;
extern int width, height, Nmines, NminesRel, level, canOpen, isEasyStart, inMin, inFree, remaino, remainm, divergence;
extern Plst recycle;
extern Plst2 recycle2;
extern Tlst2 wrong;
//---------------------------------------------------------------------------
void passAll();
bool open(PSquare p);
bool openAround(PSquare p);
void easyStart(PSquare p);
void endGame();
void addMines(int n);
void backtrack(PSquare p);
void paint(PSquare p);
void invalidateQuest();
void addScore();
//---------------------------------------------------------------------------
