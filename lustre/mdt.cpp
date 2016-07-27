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
#include "mdt.h"
int MDT::num;
mdtvec_t MDT::set;

MDT::MDT()
 : Server()
{
	id = num++;
	set.push_back(this);
	__dbg = params.debug.MDT;
	sprintf(name, "MDT%d", id);
	thnr = params.cluster.MdtThreadCount;
	nrpc = 0;
	srpc = NULL;
}


MDT::~MDT()
{
	if (srpc)
		delete srpc;
}

void MDT::InitStat()
{
	char s[MAX_NAME_LEN];

	__stat = params.stat.NIC;

	if (!params.stat.MDTNrs)
		return;

	sprintf(s, "%s.nrs.rpc", name);
	srpc = new Stat(s);
}

void MDT::StatRpc(Message *msg, int i)
{
	if (i == RPC_ENQUEUE) {
		nrpc++;
	} else if (i == RPC_DEQUEUE) {
		assert(nrpc > 0);
		//msg->phase++;
		nrpc--;
	}

	if (srpc == NULL) {
		assert(!params.stat.MDTNrs);
		return;
	}

	srpc->Record("%llu.%09llu	%llu	%s	%d\n",
		now / params.TimeUnit, now % params.TimeUnit, nrpc,
		i == RPC_ENQUEUE ? "->" : "<-", 0);
}

void MDT::OpenStateMachine(ThreadLocalData *tld)
{
	Processor *p = tld->p;
	Message *msg = tld->m;
	FileObject *obj = (FileObject *)msg->req;
	Thread *t = tld->t;

	switch (tld->state) {
	case mdt_OpenFileState:
		/* It will take 5ms to open/create a new file. Very simple... */
		t->RunAfter(params.io.OpenTicks + SignRand(params.io.OpenTicks / 10)); 
		tld->state = mdt_OpenCompleteState;
		break;
	case mdt_OpenCompleteState:
		Print(NOW "%s Finished OPEN for OBJ@%llu.\n",
			now, name, obj->GetId());
		msg->SetLength(1024);
		Send(msg);
		p->TaskCompletion(tld);
		msg->phase = RPC_PHASE_COMPLETE;
		break;
	}
}

void MDT::Handle(void *arg)
{
	ThreadLocalData *tld = (ThreadLocalData *) arg;
	MDT *mdt = (MDT *) tld->n;
	Message *msg = tld->m;
	int type = msg->GetType();

	if (msg->phase == RPC_PHASE_NEW) {
		mdt->StatRpc(msg, RPC_DEQUEUE);
		SetRPCHandleTime(msg, now);
	}

	msg->phase = RPC_PHASE_RPC;

	switch(type) {
	case MSG_OPEN:
		mdt->OpenStateMachine(tld);
		break;
	}
}

int MDT::GetCount()
{
	return num;
}

int MDT::GetNid(int i)
{
	return set[i]->nid;
}

void MDT::Start()
{
	Print(NOW"%s is starting...\n", now, name);
	InitStat();
	Server::Start();
	handler = Handle;
}
