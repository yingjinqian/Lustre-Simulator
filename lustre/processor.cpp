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
#include "processor.h"
#include "node.h"

/* thread pool implementation */

Processor::Processor()
    : Thread()
{
	counter = 0;
	num = rnum = 0;
	nrs = NULL;
	idle = 0;
	poolState = pool_StartState;
}

Processor::~Processor()
{
	Thread *t;

	while ((t = NextIdleThreadFromPool()) != NULL) {
		ThreadLocalData *tld;

		tld = t->GetTLD();
		delete tld;
		delete t;
	}
}

void Processor::Print(const char *fmt...)
{
    if (params.debug.Pool /*&& nrs != NULL &&
            (nrs->GetAlgo()) == NRS_ALGO_TBF*/) {
		va_list args;

		va_start(args,fmt);
		vfprintf(printer,fmt, args);
		va_end(args);
	}
}

Thread *Processor::NextIdleThreadFromPool()
{
	Thread *t;
	
	if (idleQ.Empty())
		return NULL;
	
	t = (Thread *)(idleQ.suc);
	t->Remove();
	idle--;
	if (idle == 0)
		poolState = pool_FullLoadState;
	return t;
}

int Processor::AddNewTask(Message *msg)
{
    Print(NOW "Processor enqueues ioreq@%p, %d:%llu:%llu\n",
          Event::Clock(), &msg->io, msg->io.cmd, msg->io.off, msg->io.count);
    if (nrs->Enqueue(msg)) {
		printf(NOW "Enqueue message failed.\n", now);
		exit(1);
	}
	
	if (poolState <= pool_IdleState) {
        Wakeup();
	}
	return 0;
}

int Processor::TaskCompletion(ThreadLocalData *tld)
{
	nrs->Finish(tld->m);

	tld->f = NULL;
	tld->m = NULL;
	tld->v = NULL;
	tld->state = 0;

	/* After shrink the thread number of the pool...*/
	if (idle + 1 > num) {
		delete tld->t;
		delete tld;
	} else {
		tld->t->Insert(&idleQ);
		idle++;
	}

	if (idle > 0 && nrs && (poolState == pool_FullLoadState)) {
		Signal();
		poolState = pool_RunState;
	}

	return 0;
}

void Processor::Wakeup()
{
    if (poolState == pool_IdleState) {
        assert(idle > 0);
        Signal();
        poolState = pool_RunState;
    }
}

void Processor::ProcessOneTask(void *arg)
{
	ThreadLocalData *tld = (ThreadLocalData *)arg;
	
	tld->f(tld);
}

void Processor::RunOneTask(Thread *t)
{
	t->Run();
}

void Processor::PoolStateMachine()
{
	switch (poolState) {
	case pool_RunState: {
		Message *msg;
		Thread *t;
		ThreadLocalData *tld; 

        /* I/O is throttled. */
        if (nrs->Throttling()) {
            poolState = pool_IdleState;
            return;
        }
		/* Initialize the working thread to handle
		 * various tasks and start it in the next tick. */
		msg = (Message *)nrs->Dequeue();
		if (msg == NULL) {
			poolState = pool_IdleState;
			return;
		}

        Print(NOW "Processor dequeues ioreq@%p, %d:%llu:%llu\n",
              Event::Clock(), &msg->io, msg->io.cmd, msg->io.off, msg->io.count);

		t = NextIdleThreadFromPool();
		assert(t != NULL);
		tld = t->GetTLD();
		tld->m = msg;

		tld->f = site->GetHandler();
		//t->Run();
		RunOneTask(t);

		/* Check for left queued tasks. */
		PoolStateMachine();
		break;
	}
	case pool_StartState:
		poolState = pool_IdleState;
	case pool_IdleState:
	case pool_FullLoadState:
		break;
	}
}

void Processor::ThreadPoolManger(void *arg)
{
	Processor *p = (Processor *) arg;
	
	p->PoolStateMachine();
}

void Processor::Attach(Node *n, int threads, Scheduler *s)
{
	site = n;
	rnum = num = threads;
	counter = threads;
	nrs = s;
    s->Attach(this);
}

int Processor::InitThreadPool()
{
	/* Create @num threads for the thread pool. */
	assert(num > 0);

	for (int i = 0; i < num; i++) {
		Thread *t;
		ThreadLocalData *tld;

		t = new Thread;
		tld = new ThreadLocalData;
		memset(tld, 0, sizeof(*tld));
		tld->m = NULL;
		tld->p = this;
		tld->n = site;
		tld->t = t;
		tld->id = i;
		tld->flags = TLD_FROM_POOL;

		t->CreateThread(ProcessOneTask, tld);
		t->Insert(&idleQ);
		/* if (site)
			Print(NOW "Start TID@%d PID@%d for %s\n",
				now, i, t->GetPid(), site->GetDeviceName());*/
	}
	idle = num;
}

int Processor::AdjustThreadPool(int c, int check)
{
	Thread * t;
	ThreadLocalData *tld;

	if (num == c)
		return rnum;

	num = c;
	if (rnum > c) { 
		/* First shrink the idle threads in the pool. */
		while (rnum > c) {
			t = NextIdleThreadFromPool();
			if (t == NULL)
				break;

			tld = t->GetTLD();
			delete t;
			delete tld;
			rnum--;
		}
		/*
		 * If there are some threads still in work state,
		 * release it once finishe the work.
		 */
		//num = c;
		LASSERT(idle >= 0);
		if (idle == 0)
			poolState = pool_FullLoadState;
	} else { /* add the new threads into the pool */
		while (rnum < c) {
			t = new Thread;
			tld = new ThreadLocalData;
			memset(tld, 0, sizeof(*tld));
			tld->m = NULL;
			tld->p = this;
			tld->n = site;
			tld->t = t;
			tld->id = counter++;
			tld->flags = TLD_FROM_POOL;

			t->CreateThread(ProcessOneTask, tld);
			t->Insert(&idleQ);
			rnum++;
			idle++;
		}
		if (poolState == pool_FullLoadState && idle > 0 && check) {
			Signal();
			poolState = pool_RunState;
		}
		
	}

	return rnum;
}

const char *Processor::GetPoolState()
{
	switch(poolState) {
	case pool_StartState:
		return "pool_StartState";
	case pool_IdleState:
		return "pool_IdleState";
	case pool_RunState:
		return "pool_RunState";
	case pool_FullLoadState:
		return "pool_FullLoadState";
	case pool_LastState:
		return "pool_LastState";
	}
}

int Processor::GetThreadCount()
{
	return num;
}

void Processor::Start()
{
    Print(NOW "Start processor...\n", now);
	InitThreadPool();
	CreateThread(ThreadPoolManger, this);
	RunAfter(1);
}
