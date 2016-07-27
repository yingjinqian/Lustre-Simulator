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
#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <event.h>
#include "hash.h"

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/

/* CPU, Thread pool Model,
 * It can be used by both Client and Server.*/
#define TLD_FROM_POOL	1
#define TLD_CTX_SWITCH	2

class Message;
class Node;
class Processor;

#define MAX_BUF_CNT	8
#define MAX_BUF_SIZE	128
struct ThreadLocalData;


struct ThreadLocalData {
	int flags;

	Node *n;
	Processor *p;
	Thread *t;
	Message *m;
	/* Backup context */
	Context ctx;
	int state;
	fun_t f;
	void *v;
	int depth;
	int id;

	Context *curctx;
	Context *inctx;
	Context *outctx;
	struct list_head ctxs;
};

class Processor : public Thread
{
protected:
	Scheduler *nrs;
	List idleQ; /* List of idle threads */
	int idle;  /* number of idle threads. */
	int num; /* number of threads in the pool */
	int rnum; /* real number of threads in the pool */
	int counter;
	Node *site;

	enum {
		pool_StartState,
		pool_IdleState,
		pool_RunState,
		pool_FullLoadState,
		pool_LastState,
	} poolState;
	
	void PoolStateMachine();
	static void ThreadPoolManger(void *arg);
	static void ProcessOneTask(void *arg);
    void Print(const char *fmt...);
	Thread *NextIdleThreadFromPool();
    const char *GetPoolState();

public:
	Processor();
    virtual ~Processor();

	void Start();
	void Attach(Node *site, int thnr, Scheduler *nrs);
    virtual void Attach(Node *site, int thnr) {} ;
	int AddNewTask(Message *msg);
	virtual int TaskCompletion(ThreadLocalData *tld);
	virtual void RunOneTask(Thread *t);
	int InitThreadPool();
	int GetThreadCount();
	int AdjustThreadPool(int c, int check);
    void Wakeup();
};

#endif
