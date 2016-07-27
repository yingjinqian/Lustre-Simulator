/***************************************************************************
 *   Copyright (C) 2008 by qian                                            *
 *                                                                         *
 *   Storage Team in NUDT                                                  *
 *   Nong Xiao <nongxiao@nudt.edu.cn>                                      *
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
#include <limits.h>
#include "event.h"
#include "nrsrbtree.h"
#include "nrsbinheap.h"

//Event

cfs_time_t Event::now;
cfs_time_t Event::maxRunTicks = ~0;
Scheduler *Event::runQ = new NrsRbtree(RunTimeCompare);
int Event::algo = ALGO_RBTREE;
//Scheduler *Event::runQ = new NrsBinheap(BinheapRunTimeCompare);
//int Event::algo = ALGO_BINHEAP;
FILE *Event::printer = stdout;
Event *Event::current;
int Event::over;

Event::Event()
{
	runTime = 0;
	arg = NULL;
	fn = NULL;
	run = 0;
	memset(&rbnode, 0, sizeof(rbnode));
	rbnode.rb_entry = this;
}

Event::~Event()
{}

void Event::InitEvent(fun_t f, void *data)
{
	fn = f;
	arg = data;
}

void Event::InsertRunQ()
{
	assert(!run);

	//printf(NOW"Insert event %p runTime %llu.\n", now, this, runTime);
	runQ->Enqueue(this);
	run = 1;
}

void Event::RemoveFromRunQ()
{
	assert(run);
	runQ->Erase(this);
	run = 0;
}

void Event::Run()
{
	current = this;
	fn(arg);
}

void Event::RunAfter(cfs_duration_t ticks)
{
	assert(!run);
	runTime = now + ticks;
	InsertRunQ();
}

void Event::RunAt(cfs_time_t time)
{
	assert(!run && (time > now));
	runTime = time;
	InsertRunQ();
}

Event *Event::NextRunEvent()
{
	Event *ev;

	ev = (Event *)runQ->Dequeue();
	if (ev)
		ev->run = 0;

	return ev;
}

void Event::Schedule()
{
	Event *e;

	now = 0;
	srand((int)time(0));

	while (e = NextRunEvent()) {
        if (now > e->runTime)
            printf(NOW"Event %p runTime %llu.\n", now, e, e->runTime);
		assert(now <= e->runTime);
		now = e->runTime;
		if (now == 502235345)
			printf("Hit the time!\n");
		e->Run();
		if (now > maxRunTicks) {
			printf("Reached max run ticks %llu (%llus).\n", 
				maxRunTicks, maxRunTicks / params.TimeUnit);
			break;
		}
	}
	
	while (NextRunEvent() != NULL);
		
	printf("Timer is stopped. Total run time %llu\n", now);
}

cfs_time_t Event::Clock()
{
	return now;
}

__u32 Event::CurrentSeconds()
{
	return now / params.TimeUnit;
}

void Event::SetClock(cfs_time_t t)
{
	now = t;
}

void Event::SetRunTicks(cfs_duration_t ticks)
{
	maxRunTicks = ticks ? : maxRunTicks;
}

/*rb_node *Event::GetRbnode(void *e)
{
	return &(((Event *)e)->rbnode);
}*/

int Event::RunTimeCompare(void *a, void *b)
{
	Event *e1, *e2;
	
	e1 = (Event *) a;
	e2 = (Event *) b;
	LASSERT(e1 != NULL && e2 != NULL);
	return (((Event *)a)->runTime > ((Event *)b)->runTime) ? 1: -1;
}

int Event::BinheapRunTimeCompare(void *a, void *b)
{
	return (((Event *)b)->runTime >= ((Event *)a)->runTime);
}

void Event::SetAlgorithm(int a)
{
	if (algo > 0)
		delete runQ;

	algo = a;
	if (algo == ALGO_RBTREE)
		runQ = new NrsRbtree(RunTimeCompare);
	else if (algo == ALGO_BINHEAP)
		runQ = new NrsBinheap(BinheapRunTimeCompare);
}

__u64 Event::__rand64()
{
	return rand() ^ ((__u64)rand() << 15) ^ ((__u64)rand() << 30) ^ 
		((__u64)rand() << 45) ^ ((__u64)rand() << 60);
}

cfs_duration_t Event::Rand(cfs_duration_t max)
{
	return params.debug.Rand ?
		(1 + (cfs_duration_t) (max * (rand() / (RAND_MAX + 1.0)))) : 0;
}

cfs_duration_t Event::Rand64(cfs_duration_t max)
{
	return params.debug.Rand ?
		(1 + max * __rand64() / (ULLONG_MAX + 1.0)) : 0;
}

int Event::SignRand(cfs_duration_t max)
{
	return params.debug.Rand ?
		(rand() % (2 * max + 1) - max) : 0;
}

void Event::DumpEvents()
{
	Event *e;

	while (e = NextRunEvent()) {
		printf(NOW"run event %p.\n", e->runTime);
	}
}

/**************************************************************************
 * class Thread.                                                          *
 **************************************************************************/
#include "processor.h"
uint32_t Thread::num;

Thread::Thread()
{
	pid = num++;
}

Thread::~Thread()
{}

void Thread::CreateThread(fun_t f, void *data)
{
	InitEvent(f, data);
}

void Thread::Signal()
{
	assert(!run);
    if (pid == 79)
        printf(NOW "PID@%d receive singal.\n", now, pid);
	runTime = now + Rand(params.thread.CtxSwitchTicks);
	InsertRunQ();
}

void Thread::Sleep(cfs_duration_t timeout)
{
	assert(!run);
	RunAfter(timeout);
}

int Thread::GetPid()
{
	return pid;
}

void Thread::PushContext(int state, fun_t f)
{
	ThreadLocalData *tld = (ThreadLocalData *)arg;
	
	assert((tld != NULL) && 
	       (tld->flags & TLD_FROM_POOL ||
	        tld->flags & TLD_CTX_SWITCH) &&
		(tld->depth == 0));

	tld->ctx.tldf = tld->f;
	tld->ctx.state = tld->state;
	tld->ctx.v = tld->v;
	tld->f = f;
	tld->state = state;
	tld->depth++;

	if (tld->flags & TLD_CTX_SWITCH) {
		tld->ctx.fn = fn;
		fn = f;
	}
}

void Thread::PopContext()
{
	ThreadLocalData *tld = (ThreadLocalData *)arg;

	assert((tld != NULL) && 
	       (tld->flags & TLD_FROM_POOL ||
	        tld->flags & TLD_CTX_SWITCH) &&
		(tld->depth == 1));

	tld->f = tld->ctx.tldf;
	tld->state = tld->ctx.state;
	tld->v = tld->ctx.v;
	tld->depth--;

	if (tld->flags & TLD_CTX_SWITCH)
		fn = tld->ctx.fn;
}

void Thread::WakeupAll(List *waitq)
{
	Thread *t;

	while (!waitq->Empty()) {
		t = (Thread *)(waitq->suc);
		t->Remove();
		t->Signal();
	}
}

ThreadLocalData *Thread::GetTLD()
{
	ThreadLocalData *tld = (ThreadLocalData *)arg;

	assert((tld != NULL) /*&& 
	       (tld->flags & TLD_FROM_POOL || tld->flags & TLD_CTX_SWITCH)*/);

	return tld;
}

Thread *Thread::Current()
{
	return (Thread *)current;
}

__u32 Thread::CurrentPid()
{
	return Current()->GetPid();
}

ThreadLocalData *Thread::CurrentTLD()
{
	return Current()->GetTLD();
}

void Thread::PushCurCtxt(int state, fun_t f)
{
	return Current()->PushContext(state, f);
}

void Thread::PopCurCtxt()
{
	return Current()->PopContext();
}

Context *Thread::CurrentContext()
{
	ThreadLocalData *tld = CurrentTLD();

	return tld->curctx;
}

void *Thread::CurrentContextData()
{
	return CurrentContext()->v;
}

void Thread::SaveContext(int state)
{
	ThreadLocalData *tld = CurrentTLD();
	Context *ctx = tld->curctx;

	MoveState(state);
	list_add_tail(&ctx->list, &tld->ctxs);
}

Context *Thread::CreateContext(fun_t f)
{
	Context *ctx = new Context;

	memset(ctx, 0, sizeof(*ctx));
	CFS_INIT_LIST_HEAD(&ctx->list);
	ctx->fn = f;

	return ctx;
}

Context *Thread::SwitchContext(int prev_state, fun_t f)
{
	ThreadLocalData *tld = CurrentTLD();
	Context *ctx = CreateContext(f);

	SaveContext(prev_state);
	tld->curctx = ctx;
	tld->inctx = ctx;
	tld->depth++;
	Current()->fn = f;
}

void Thread::RestoreContext()
{
	ThreadLocalData *tld = CurrentTLD();
	Context *ctx;

	LASSERT(!list_empty(&tld->ctxs));

	tld->outctx = tld->curctx;
	tld->curctx = ctx = list_entry(tld->ctxs.prev, struct Context, list);
	list_del(&tld->outctx->list);

	if (tld->depth == 0) {
		tld->state = ctx->state;
	} else {
		tld->depth--;
		Current()->fn = ctx->fn;
		ctx->fn(tld);
	}
}

void Thread::IN(int state, fun_t f)
{
	SwitchContext(state, f);
}

void Thread::OUT()
{
	ThreadLocalData *tld = CurrentTLD();

	delete tld->outctx;
	tld->outctx = NULL;
}

void *Thread::SETV(int idx, int len)
{
	Context *ctx = CurrentContext();

	LASSERT((ctx->buflens[idx] == 0 && idx == ctx->bufcount) ||
		ctx->buflens[idx] == len);

	ctx->buflens[idx] = len;
	ctx->bufcount++;
	return ContextGetParameter(ctx, idx, len);
}

void *Thread::GETP(int idx, int len)
{
	Context *ctx = CurrentContext();
	int offset = 0, i;

	LASSERT(ctx->buflens[idx] == len);

	for (i = 0; i < idx; i++)
		offset += SizeRound(ctx->buflens[i]);

	return *(void **) ((char*)ctx->buf + offset);
}

void Thread::INP(int idx, int len, void *p)
{
	ThreadLocalData *tld = CurrentTLD();
	Context *ctx = tld->inctx;
	char * ptr;
	void **pp;

	
	LASSERT(ctx != NULL);
	if (idx > ctx->bufcount || idx > MAX_CTX_BUF_CNT ||
            ctx->bufsize >= MAX_CTX_BUF_SIZE)
		LBUG();

	ctx->buflens[idx] = len;
	pp = (void **)((char *)ctx->buf + ctx->bufsize);
	*pp = p;

	ctx->bufsize += SizeRound(len);
	ctx->bufcount++;
}

void *Thread::OUTP(int index, int len)
{
	ThreadLocalData *tld = CurrentTLD();
	Context *ctx = tld->outctx;

	LASSERT(ctx != NULL);
	return ContextPopParameter(ctx, index, len);
}

void Thread::SetTLD(ThreadLocalData *tld)
{
	arg = tld;
}

void Thread::MoveState(int state)
{
	ThreadLocalData *tld = CurrentTLD();

	if (tld->depth == 0)
		tld->state = state; 

	tld->curctx->state = state;
}

void Thread::MoveLastState()
{
	MoveState(FUN_LAST_STATE);
}

int Thread::CurrentState()
{
	ThreadLocalData *tld = CurrentTLD();

	LASSERT(tld != NULL);

	if (tld->depth == 0)
		return tld->state;

	return tld->curctx->state;
}

void Thread::InitTLD(ThreadLocalData *tld)
{
	memset(tld, 0, sizeof(*tld));
	CFS_INIT_LIST_HEAD(&tld->ctx.list);
	CFS_INIT_LIST_HEAD(&tld->ctxs);
	tld->curctx = &tld->ctx;
}

void Thread::ContextPushParameter(Context *ctx, int idx, int len, char *buf)
{
	char **ptr;

	if (idx > ctx->bufcount || idx > MAX_CTX_BUF_CNT ||
            ctx->bufsize >= MAX_CTX_BUF_SIZE)
		LBUG();

	ctx->buflens[idx] = len;
	*ptr = (char *)&ctx->buf[0];
	*ptr += ctx->bufsize;

	if (buf)
		*ptr = buf;

	ctx->bufsize += SizeRound(len);
	ctx->bufcount++;
}

void *Thread::ContextPopParameter(Context *ctx, int idx, int len)
{
	void *buf;

	LASSERT(idx == ctx->bufcount - 1);
	buf = 	ContextGetParameter(ctx, idx, len);
	ctx->bufcount--;
	ctx->bufsize -= SizeRound(len);
	ctx->buflens[idx] = 0;

	return buf;
}

void *Thread::ContextGetParameter(Context *ctx, int idx, int len)
{
	int offset = 0, i;

	LASSERT(ctx->buflens[idx] == len);

	for (i = 0; i < idx; i++)
		offset += SizeRound(ctx->buflens[i]);

	return (void *) (ctx->buf + offset);
}


