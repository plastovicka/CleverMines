/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */

#include <stdlib.h>
#include <assert.h>
#include "miny.h"

Plst
recycle=0,
 lockm=0, lockv=0,//locked mines and open blocks
 changed=0;	//reordering result

Plst2
recycle2=0;

Tlst2
wrong(0); //list of open blocks where mines were added or removed

int
inMin,     //number of inside mines
 inFree,    //number of inside open blocks
 Nadded,    //how many mines are in changed
 depth,     //recursion depth
 timeout,   //computation time
 divergence;//how many mines were added during reordering

bool
wellDone;  //reordering has been successful


//---------------------------------------------------------------------------
//create list item
Plst newTlst(PSquare param)
{
	Plst l;
	if(recycle){
		l=recycle;
		recycle=l->nxt;
		l->p=param;
	}
	else{
		l = new Tlst(param);
	}
	return l;
}
//create new item at the beggining of list
void additem(Plst &list, PSquare param)
{
	Plst a = newTlst(param);
	a->nxt=list;
	list=a;
}
//delete the first item and return its value
PSquare delitem(Plst &list)
{
	Plst a=list;
	list=a->nxt;
	a->nxt=recycle;
	recycle=a;
	return a->p;
}
//create item in two-way list
Plst2 newTlst2(PSquare param)
{
	Plst2 l;
	if(recycle2){
		l=recycle2;
		recycle2=l->nxt;
		l->p=param;
	}
	else{
		l = new Tlst2(param);
	}
	return l;
}
//create item at the end of two-way list
//return pointer to new item
Plst2 append(Tlst2 &list, PSquare param)
{
	Plst2 l = newTlst2(param);
	l->prv = list.prv;
	l->nxt = &list;
	list.prv->nxt = l;
	list.prv = l;
	return l;
}
//create item at the beginning of two-way list
//return pointer to new item
Plst2 prepend(Tlst2 &list, PSquare param)
{
	Plst2 l = newTlst2(param);
	l->prv = &list;
	l->nxt = list.nxt;
	list.nxt->prv = l;
	list.nxt = l;
	return l;
}
//delete the first item and return its value
PSquare delitem(Plst2 item)
{
	item->prv->nxt = item->nxt;
	item->nxt->prv = item->prv;
	item->nxt=recycle2;
	recycle2=item;
	return item->p;
}

//---------------------------------------------------------------------------
//block is definite
void setDef(PSquare p)
{
	PSquare g, h;
	int *s, *s1, i;
	bool ismine, ok;

	assert(p>=board && p<boardk);
	if(p->def) return;
	p->def = true;
	if(vizv) paint(p);
	p->lock++;
	ismine=p->mine;
	if(!ismine && !p->opened) canOpen++;

	/*u okolních odkrytých polí zjisti, jestli:
	 všechny miny jsou urèeny => zbylá pole volná
	 èíslo se rovná poètu neurèených polí => všechny jsou miny
	 */
	for(s=p->diroff; *s; s++){
		g= p+(*s);
		if(ismine) g->mayMine--; else g->mayFree--;
		if(g->opened){
			if(g->mayMine==0 && g->mayFree>0 || g->mayFree==0 && g->mayMine>0)
				for(s1=g->diroff; *s1; s1++){
					setDef(g+(*s1));
				}
		}
	}
	// equivalent blocks will also be definite
	if(!p->opened && p->Nopened){
		g=p->nxtOpened[0];
		for(s=g->diroff; *s; s++){
			h= g+(*s);
			if(h->mine==ismine && h->Nopened==p->Nopened){
				ok=true;
				for(i=p->Nopened -1; i>=0; i--)
					if(p->nxtOpened[i] != h->nxtOpened[i]){ ok=false; break; }
				if(ok) setDef(h);
			}
		}
	}
}
//---------------------------------------------------------------------------
//add or remove a mine
void invertm(PSquare p)
{
	PSquare g;
	int *s;
	bool m;

	m=p->mine;
	p->mine=!m;
	if(p->inside){
		if(m) inMin--, inFree++;
		else   inMin++, inFree--;
	}
	for(s=p->diroff; *s; s++){
		g= p+(*s);
		if(m) g->Nmin--, g->mayMine--, g->mayFree++;
		else   g->Nmin++, g->mayMine++, g->mayFree--;
	}
	if(vizmin) paint(p);
}
//---------------------------------------------------------------------------
//lock a block, it will not be changed during backtracking
bool lockp(PSquare p)
{
	PSquare h, g, *o;
	int *s, i1, i2;
	bool ok;

	if(p->lock++) return true; //already locked
	for(o= p->nxtOpened; *o; o++){
		if(p->mine) (*o)->mayMine--; else (*o)->mayFree--;
	}
	for(o= p->nxtOpened; *o; o++){
		g = *o;
		//find out possibilities, how to place mines around block g
		i1= g->mayMine - g->divergence;
		i2= g->mayFree + g->divergence;
		if(i1<0 || i2<0) return false; //wrong combination
		if(g->lockedv || !g->mayFree) i1=1;
		if(g->lockedm || !g->mayMine) i2=1;
		if(i1 && i2) continue;
		ok=true;

		if(!i1){
			//put no more mines around block g
			g->lockedv=true;
			additem(lockv, g);
			for(s=g->diroff; *s; s++){
				h=g+(*s);
				if(!h->mine){
					if(!lockp(h)) ok=false;
				}
			}
		}
		if(!i2){
			//remove no more mines around block g
			g->lockedm=true;
			additem(lockm, g);
			for(s=g->diroff; *s; s++){
				h=g+(*s);
				if(h->mine){
					if(!lockp(h)) ok=false;
				}
			}
		}
		if(!ok) return false; //error
		//only one possibility to correct divergence
		if(!wellDone){
			for(s=g->diroff; *s; s++){
				h=g+(*s);
				if(!h->lock && (!h->mine && !i2 || h->mine && !i1)){
					backtrack(h);
					if(!wellDone) return false;
					return true;
				}
			}
		}
	}
	return true;
}
//---------------------------------------------------------------------------
//unlock block
void unlockp(PSquare p)
{
	PSquare *o;

	if(!--p->lock){
		for(o= p->nxtOpened; *o; o++){
			if(p->mine) (*o)->mayMine++; else (*o)->mayFree++;
		}
	}
}
//---------------------------------------------------------------------------
void unlockm(PSquare p)
{
	PSquare h;
	int *s;

	p->lockedm=false;
	for(s=p->diroff; *s; s++){
		h=p+(*s);
		if(h->mine) unlockp(h);
	}
}
//---------------------------------------------------------------------------
void unlockv(PSquare p)
{
	PSquare h;
	int *s;

	p->lockedv=false;
	for(s=p->diroff; *s; s++){
		h=p+(*s);
		if(!h->mine) unlockp(h);
	}
}
//---------------------------------------------------------------------------
//backtracking to reorder mines
void backtrack(PSquare p)
{
	PSquare g, h, g1, *o;
	int *s;
	bool ok, ismine, setfree;
	int k, i, N, db;
	Plst lockv1, lockm1;
	Plst2 l, lm=0;
	long nm;
	struct{ PSquare p; int b; } tahy[12];

	assert(!p->lock);
	if(timeout<=0){
		timeout=0;
		return; //it runs too long
	}
	timeout--;
	depth++;
	ismine=p->mine;
	lockv1=lockv;
	lockm1=lockm;
	//correct divergence on adjacent blocks
	for(o=p->nxtOpened; *o; o++){
		g=*o;
		if(ismine) g->divergence--; else g->divergence++;
		//remember all blocks where visible number is not equal to number of mines
		if(g->divergence){
			if(!g->item){ //there must not be duplicities
				g->item=append(wrong, g);
			}
		}
		else{
			if(g->item){
				delitem(g->item);
				g->item=0;
			}
		}
	}
	if(ismine) divergence--; else divergence++;
	if(debug){ p->mine= !ismine; paint(p); p->mine=ismine; }

	if(lockp(p) && !wellDone){
		if(wrong.nxt != &wrong){
			//find block which has highest priority
			nm=-0x7fffffff;
			for(l=wrong.nxt; l!=&wrong; l=l->nxt){
				if(l->p->prior > nm){ lm=l; nm=l->p->prior; }
			}
			h=lm->p;
			h->prior+= depth;
			//h is open block where mine needs to be added or removed
			assert(h->divergence && h->opened);
			setfree= h->divergence >0;
			//go through blocks adjacent to h and put them to array tahy
			N=0;
			for(s=h->diroff; *s; s++){
				assert(s-h->diroff<13);
				g= h+(*s);
				if(g->mine==setfree && !g->lock){
					//calculate how much will be wrong values corrected
					db=0;
					for(o=g->nxtOpened; *o; o++){
						if((*o)->divergence>0 && setfree  ||  (*o)->divergence<0 && !setfree)  db++;
						else db--;
					}
					//tahy will be sorted by db; from best to worst
					for(k=N; k>0 && tahy[k-1].b<db; k--);
					//choose only one block among equivalent blocks
					if(k>0 && (g1=tahy[k-1].p)->Nopened == g->Nopened){
						for(i= g->Nopened-1;; i--){
							if(i<0) goto l1;
							if(g->nxtOpened[i] != g1->nxtOpened[i]) break;
						}
					}
					//add block g to list tahy
					for(i=N; i>k; i--) tahy[i]=tahy[i-1];
					N++;
					tahy[k].b=db;
					tahy[k].p=g;
				l1:;
				}
			}
			//recurse
			assert(N<13);
			for(k=0; k<N; k++){
				backtrack(tahy[k].p);
				if(wellDone) break;
			}
		}
		else
	//reordering succeeded, but total count of mines might be wrong
			if((divergence<=inMin) && (divergence>=-inFree)){
				wellDone=true;
				Nadded=divergence;
			}
	}
	//restore everything that was changed in this function
	if(debug) paint(p);
	if(ismine) divergence++; else divergence--;
	for(o= p->nxtOpened; *o; o++){
		g=*o;
		if(ismine) g->divergence++; else g->divergence--;
		if(g->divergence){
			if(!g->item){
				g->item=prepend(wrong, g);
			}
		}
		else{
			if(g->item){
				delitem(g->item);
				g->item=0;
			}
		}
	}
	unlockp(p);
	while(lockv!=lockv1){
		unlockv(delitem(lockv));
	}
	while(lockm!=lockm1){
		unlockm(delitem(lockm));
	}
	depth--;

	if(wellDone){
		//solution has been found; write result
		additem(changed, p);
		p->unknown=true;
		//equivalent blocks will be unknown, too
		if(p->Nopened){
			g=p->nxtOpened[0];
			for(s=g->diroff; *s; s++){
				h= g+(*s);
				if(h->mine==ismine && h->Nopened==p->Nopened){
					ok=true;
					for(i=p->Nopened -1; i>=0; i--)
						if(p->nxtOpened[i] != h->nxtOpened[i]){ ok=false; break; }
					if(ok) h->unknown=true;
				}
			}
		}
	}
}
//---------------------------------------------------------------------------
//add or remove mine on block p and reorder other mines, 
//  so that open blocks numbers needn't change
bool invert(PSquare p)
{
	PSquare h;

	if(vizmin || debug){ p->flag= !p->flag; paint(p); }

	for(h=board; h<boardk; h++)  h->prior=0;
	dellist(changed);
	wellDone=false;
	depth=0;
	timeout=50000;
	backtrack(p);

	assert(!lockv && !lockm);
	assert(!divergence && wrong.nxt==&wrong);
	for(h=board; h<boardk; h++){
		assert(!h->divergence && !h->item);
		assert(!h->lockedv && !h->lockedm);
		assert(!h->lock || h->def);
	}

	if(vizmin || debug){ p->flag= !p->flag; paint(p); }
	return wellDone;
}
//---------------------------------------------------------------------------
//list changed contains blocks which will be inverted
//variable Nadded is number of mines to removed from inside blocks
void makeChange()
{
	PSquare p, h=0;
	bool ismine;
	int k, q;

	//at first, change mines on inside blocks according to total number of mines 
	ismine=Nadded>0;
	for(k=abs(Nadded); k>0; k--){
		q=0;
		for(p=board; p<boardk; p++)
		 if(p->inside && (p->mine==ismine)){
			 q++;
			 if(!random(q)) h=p;
		 }
		invertm(h);
	}

	while(changed){
		invertm(delitem(changed));
	}
}

//---------------------------------------------------------------------------
//set p->def to true if block p is definite (block cannot be inverted)
//try to maintain (inMin==(inMin+inFree)*NminesRel/100)
void passAll()
{
	PSquare p;

	if(gameOver) return;
	for(p=board; p<boardk; p++){
		p->unknown=false;
	}
	for(p=board; p<boardk; p++)
	 if(!(p->inside || p->def || p->unknown)){
		 if(!invert(p)){
			 if(timeout>0) setDef(p);
		 }
		 else if(Nadded && (
			 (Nadded>0) == ((inFree+inMin)*NminesRel/100 < inMin -Nadded)
				|| Nadded>0 && !inFree)){
			 makeChange();
		 }
	 }
	//redraw (?)
	invalidateQuest();
}
//---------------------------------------------------------------------------
//left mouse button - open a block
bool open(PSquare p)
{
	PSquare g;
	int *s;
	bool b;

	assert(p>=board && p<boardk);
	if(p->opened || p->flag || p->lock>99) return false;
	//move mine to another block or put mine on this block (according to level)
	b= canOpen>0 || (level==5 && p->inside && p->Nmin==p->mayMine && inMin!=Nmines);
	if(!p->def && (p->mine && (level==1 || !b || level==2 && !p->inside)
	 || !p->mine && level>3 && b))
		if(invert(p)) makeChange();
	//mine => game over
	if(p->mine){
		endGame();
		looser=true;
		bum=p;
	}
	else{
		//open block p
		p->opened=true;
		if(!p->mine && p->def) canOpen--;

		//adjacent blocks and block p are not inside anymore
		for(s=p->diroff;; s++){
			g= p+(*s);
			if(g->inside){
				g->inside=false;
				if(g->mine) inMin--; else inFree--;
			}
			if(!*s) break;
		}
		//append block p to list of open blocks
		for(s=p->diroff; *s; s++){
			g= p+(*s);
			g->nxtOpened[g->Nopened++]=p;
			g->nxtOpened[g->Nopened]=0;
		}
		//p is definite, there can't be mine
		if(p->mayMine==0 && p->mayFree>0 || p->mayFree==0 && p->mayMine>0){
			for(s=p->diroff; *s; s++)
				setDef(p+(*s));
		}
		setDef(p);
		//show number
		paint(p);
		//open neighbors if there are no mines
		if(p->Nmin==0){
			for(s=p->diroff; *s; s++){
				open(p+(*s));
			}
		}
		//game over when all blocks are open
		remaino--;
		if(!remaino){
			endGame();
			if(remainm!=Nmines) remainm=0;
			if(!cheat) addScore();
		}
	}
	return true;
}
//---------------------------------------------------------------------------
//open block was clicked when all adjacent mines are marked
bool openAround(PSquare p)
{
	int *s;
	int nz=0;
	bool result=false;

	assert(p>=board && p<boardk);
	//count markes around p
	for(s=p->diroff; *s; s++){
		if((p+(*s))->flag) nz++;
	}
	if(nz==p->Nmin){
		//open neighbors except marked blocks
		for(s=p->diroff; *s; s++)
			result |= open(p+(*s));
	}
	passAll();
	return result;
}
//---------------------------------------------------------------------------
void addMines(int n)
{
	PSquare p;

	while(n--){
		do{
			p= board + random((width+4)*(height+2));
		} while(p->mine || p->def);
		invertm(p);
	}
}
//---------------------------------------------------------------------------
void easyStart(PSquare p)
{
	int n, *s;
	PSquare g;

	if(!isEasyStart) return;
	for(;;){
		n=0;
		for(s=p->diroff;; s++){
			g= p+(*s);
			if(g->mine){ invertm(g); n++; }
			if(!*s) break;
		}
		if(n==0) break;
		addMines(n);
	}
}
//---------------------------------------------------------------------------
