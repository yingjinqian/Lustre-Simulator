/***************************************************************************
 *   Copyright (C) 2008 by qian                                            *
 *                                                                         *
 *   Storage Team in NUDT                                                  *
 *   Yingjin Qian <yingjin.qian@sun.com>                                   *
 *                                                                         *
 *   This file is part of Lustre, http://www.lustre.org                    *
 *                                                                         *
 *   Lustre is free software; you can redistribute it and/or               *
 *   modify it under the terms of version 2 of the GNU General Public      *
 *   License as published by the Free Software Foundation.                 *
 *                                                                         *
 *   Lustre is distributed in the hope that it will be useful,             *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Lustre; if not, write to the Free Software                 *
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
 ***************************************************************************/
#ifndef EVENT_H
#define EVENT_H

#include <string.h>
#include <lustre.h>
#include <rbtree.h>
#include "scheduler.h"
#include "hash.h"

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/

struct List
{
	List *suc;
	List *pred;
	List()
		{	suc = pred = this;}
	~List()
		{ assert(suc == this && pred == this);}

	int Empty()
		{
			return (suc == this);
		}

	void Insert(List *other)
	 	{
			suc = other;
			pred = other->pred;
			suc->pred = pred->suc = this;
		}
		
	void InsertTail(List *head)
		{
			suc = head->suc;
			pred = head;
			head->suc = head->suc->pred = this;
			
		}
	void Remove()
	 	{
			suc->pred = pred; 
			pred->suc = suc; 
			suc = pred = this;
		}
		
	int  Size()
		{
			int n = 0; 
			for (List *l = suc; l != this; l = l->suc)
				n++;
			return (n);
		}

	int  Contains(List *other)
		{
			for (List *l = suc; l != this; l = l->suc)
				if (l == other)
					return (1); 
			return (0);
		}
};

typedef cfs_time_t order_t;
#define LAST_ORDER (order_t(0x7fffffff))

struct OrderList : public List
{
	order_t *order;
 	OrderList()
		{order = NULL;}
		
	OrderList(order_t *o)
		{order = o;}
		
	void InsertInorder (OrderList *other)
		{
			OrderList *o = (OrderList *)other->suc;
			while (o != other && (*order) >= (*o->order))
				o = (OrderList *)o->suc;
	  		Insert (o);
		}
		
   order_t NextOrder()
	 	{return (Empty() ? LAST_ORDER : (*((OrderList *)suc)->order));}
};

#define ALGO_FIFO 1
#define ALGO_ORDERLIST 2
#define ALGO_BINHEAP 3
#define ALGO_RBTREE 4

typedef void (*fun_t)(void *);
class Event
{
	rb_node rbnode; /* It must be the first member. */

	static int algo;
	static Scheduler *runQ;

	static Event *NextRunEvent();
	static int RunTimeCompare(void *a, void *b);
	static int BinheapRunTimeCompare(void *a, void *b);
protected:
	int run;
	void *arg;
	fun_t fn;
	cfs_time_t runTime;

	static int over;
	static cfs_time_t now;
	static cfs_time_t maxRunTicks;
	static FILE *printer;

	static Event *current;

	void InsertRunQ();
	void RemoveFromRunQ();

	//static rb_node *GetRbnode(void *e);
	static __u64 __rand64();	
public:
	Event();

	~Event();

	void InitEvent(fun_t f, void *data);
	void Run();
	void RunAfter(cfs_duration_t ticks);
	void RunAt(cfs_time_t	time);

	static cfs_time_t Clock();
	static __u32 CurrentSeconds();
	static void SetClock(cfs_time_t t);
	static void Schedule();
	static void SetAlgorithm(int a);
	static void SetRunTicks(cfs_duration_t ticks);
	static cfs_duration_t Rand64(cfs_duration_t max);
	static cfs_duration_t Rand(cfs_duration_t max);
	static int SignRand(cfs_duration_t max);
	static void DumpEvents();
};

#define FUN_FIRST_STATE	0
#define FUN_LAST_STATE	500

#define MAX_CTX_BUF_CNT	8
#define MAX_CTX_BUF_SIZE	128

struct Context {
	fun_t tldf;
	fun_t fn;
	int state;
	void *v;

	struct list_head list;

	__u32 bufcount;
	__u32 bufsize;
	__u32 buflens[MAX_CTX_BUF_CNT];
	__u32 buf[MAX_CTX_BUF_SIZE];
};

struct Context;
class ThreadLocalData;
class Thread: public Event, public OrderList
{
	int pid;

	static uint32_t num;
public:
	Thread();
	~Thread();
	
	void CreateThread(fun_t f, void *data);
	ThreadLocalData *GetTLD();
	void PushContext(int state, fun_t f);
	void PopContext();
	void Signal();
	void Sleep(cfs_duration_t to);
	void Suspend();
	int GetPid();
	static Thread *Current();
	static __u32 CurrentPid();
	static ThreadLocalData *CurrentTLD();
	static void PushCurCtxt(int state, fun_t f);
	static void PopCurCtxt(); 
	static void WakeupAll(List *waitq);

	static Context *CurrentContext();
	static void *CurrentContextData();
	static void SaveContext(int state);
	static void RestoreContext();
	static void MoveState(int state);
	static int CurrentState();
	static void InitTLD(ThreadLocalData *data);

	static Context *CreateContext(fun_t f);
	static Context *SwitchContext(int prev_state, fun_t f);
	static inline int SizeRound(int val) { /*return (val + 4) & (~0x3);*/ return val; }
	//static void CtxSetBufLen(Context *ctx, int n, int len);
	static void ContextPushParameter(Context *ctx, int idx, int len, char *buf);
	static void *ContextPopParameter(Context *ctx, int idx, int len);
	static void *ContextGetParameter(Context *ctx, int idx, int len);

	static void IN(int state, fun_t f);
	static void OUT();
	static void *SETV(int idx, int len);
	static void *GETP(int idx, int len);
	//static void INV(int idx ,ine len , void *p);
	static void INP(int idx, int len, void *p);
	//static void OUTV(int idx, int len);
	static void *OUTP(int idx, int len);


	static inline void MoveLastState();

	void SetTLD(ThreadLocalData *tld);
};

struct Layout {
	obd_count stripe_size;
	uint32_t stripe_count;
	uint32_t stripe_pattern;
	int StripeAlign(obd_off off)
		{
			return !(off & (stripe_size - 1));
		}
	int Check(obd_count count)
		{
			return stripe_size % count == 0 || count % stripe_size == 0;
		}
};

class Object
{
	obd_id id;

public:
	Thread *waiter;
public:
	Object() { id = 0;}
	~Object() {}
	obd_id GetId() { return id; }
	void SetId(obd_id oid) { id = oid; }
};

class ChunkObject : public Object
{
public:
	obd_id ost_idx;
	obd_id ost_gen;
	obd_count kms;

public:
	ChunkObject()
		: Object()
		{
			ost_idx = ost_gen = 0;
			kms = 0;
		}
	~ChunkObject()
		{}
};

#define FILE_HAS_LAYOUT 1

class FileObject : public Object
{
public:
	int flags;
	obd_id gr;
	ChunkObject *cobj; /* children objects */
	Layout layout;

public:
	FileObject() : Object()
		{
			flags = 0;
			gr = 0;
			cobj = NULL;
			waiter = NULL;
			memset(&layout, 0, sizeof(layout));
		}
	~FileObject() 
		{
			if (cobj) {
				delete [] cobj;
			}
		}
	Layout *GetLayout()
		{
			return &layout;
		}
	void SetLayout(Layout *lyt)
		{
			memcpy(&layout, lyt, sizeof(*lyt));
		}
	uint32_t StripeCount()
		{
			return layout.stripe_count;
		}
};

#define IO_FLG_DEADLINE 1
#define IO_FLG_DYNAMIC_DEADLINE 2
#define IO_FLG_MANDATORY_DEADLINE 4
#define IO_FLG_DEADLINE_1 8
#define IO_FLG_DEADLINE_2 16
#define IO_FLG_LOCK_CB 32
#define IO_FLG_XXX 64
#define IO_FLG_PRIO 128
#define IO_FLG_IOPS 256

#define LUSTRE_JOBID_SIZE       32

struct IO {
	int cmd;
	int ref;
	int prio;
	int flags;
	obd_id fid;
	obd_off off;
	obd_count count;
	obd_count left;
    char jobid[LUSTRE_JOBID_SIZE];
	cfs_time_t deadline;
    cfs_time_t arrivalTime;
	Object *obj;
	Thread *waiter;
	IO *parent;
	void *data;
    void *ptr;

	void Refer(IO *io)
		{
			ref++;
			io->cmd = cmd;
			io->flags = flags;
			io->deadline = deadline;
			io->prio = prio;
		}
};

struct Request : public List
{
	int type;
	int ref;

	union {
		IO io;
	} u;

	Thread *worker;
};

#endif
