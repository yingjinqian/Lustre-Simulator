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
#include "congestcontroller.h"
#include "message.h"


int CongestController::depth_stat = 1;

CongestController::CongestController()
 : Processor()
{
	csp.depth = csp.rcc = csp.uc = 0;
	csp.congested = 0;
	csp.state = 0;
}


CongestController::~CongestController()
{
}

void CongestController::Attach(Node *site, int thnr)
{
	cl = (Client *)site;
	Processor::Attach(site, thnr, &csp.Q);
}

void CongestController::StatRCC()
{
	Stat *st = Client::GetRCCStat();

	st->Record("%llu.%09llu\t%d\t%d\t%d\t%d\t%d\n",
		now / params.TimeUnit,
		now % params.TimeUnit,
		csp.uc, csp.rcc, idle, num, rnum);
}

int CongestController::TaskCompletion(ThreadLocalData *tld)
{
	Message *msg = tld->m;
	RPC *rpc;

	/*if (!params.cc.ON || !params.cc.CLON)
		return 0;*/
 
	Print(NOW "Finish a Request %p\n", now, tld->m);
	rpc = msg->GetRPC();
	csp.Q.Finish(tld->m);

	tld->f = NULL;
	tld->m = NULL;
	tld->v = NULL;
	tld->state = 0;

	//csp.depth--;
	csp.uc--;
	LASSERT(csp.uc >= 0);

 
	/*LASSERTF(!run, "rcc %d returned rcc %d num %d rnum %d %s\n",
		csp.rcc, rpc->rcc, num, rnum, GetPoolState());*/
	if (!params.cc.FIX) {
		csp.rcc = rpc->rcc;
		AdjustThreadPool(csp.rcc, 0);
		LASSERT(csp.rcc == num);
		StatRCC();
	}

	/* After shrink the thread number of the pool...*/
	if (rnum > num) {
		delete tld->t;
		delete tld;
		rnum--;
	} else {
		tld->t->Insert(&idleQ);
		idle++;
	}

	if (!run && idle > 0 && nrs && (poolState == pool_FullLoadState)) {
		LASSERT(!run);
		Signal();
		poolState = pool_RunState;
	}

	return 0;
}

int CongestController::AddNewTask(Message *msg)
{
	Print(NOW"Add a new Reuqest %p.\n", now, msg);

	csp.depth++;
	CheckIoCongestion();

	return Processor::AddNewTask(msg);
}

void CongestController::RunOneTask(Thread *t)
{
	RPC *rpc;
	ThreadLocalData *tld = t->GetTLD();

	Print(NOW"Run One task t:%p\n", now, t);

	rpc = tld->m->GetRPC();

	csp.uc++;
	csp.depth--;
	CheckIoCongestion();

	rpc->dc = csp.depth;
	rpc->rcc = csp.rcc;

	t->Run();
}

Stat *CongestController::GetDepthStat()
{
	return (Client::GetStat(0));
}

void CongestController::AddDepthStat()
{
	Stat *st;

	if (!depth_stat)
		return;

	st = GetDepthStat();
	st->Record("%llu.%09llu\t%d\n", 
		now / params.TimeUnit,
		now % params.TimeUnit,
		csp.depth);


}

void CongestController::CheckIoCongestion()
{
	if (csp.depth >= params.cc.CQmax && !csp.congested) {
		csp.state = cl->SetIoCongestFlag();
		csp.congested = 1;
	} else if (csp.congested && csp.depth < params.cc.CQmax) {
		cl->ClearIoCongestFlag(csp.state);
		csp.congested = 0;
	}

	if (params.cc.CLON)
		AddDepthStat();
}
