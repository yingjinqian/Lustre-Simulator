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
#include "ost.h"

int OST::num;
ostvec_t OST::set;

OST::OST()
 : Server()
{
	id = num++;
	set.push_back(this);
	__dbg = params.debug.OST;
	sprintf(name, "OST%d", id);

	norpc = nowr = nord = 0;
	stime = 0;
	thnr = params.cluster.OstThreadCount;
	srpc = swr = srd = sen = sex = NULL;
}


OST::~OST()
{
	if (srpc)
		delete srpc;
	if (swr)
		delete swr;
	if (srd)
		delete srd;
	if (sen)
		delete sen;
	if (sex)
		delete sex;
}

void OST::StatNrs(Message *msg, int delta)
{
	int type;
	Stat *cur;
	IO *io = (IO *) msg->req;

	if (!params.stat.OSTNrs || !srpc)
		return;

	type = msg->GetType();
	norpc += delta;
	srpc->Record("%llu.%09llu	%llu	%d\n",
		now / params.TimeUnit, now % params.TimeUnit, norpc, 0);
	if (type == MSG_READ) {
		nord += delta;
		srd->Record("%llu.%09llu	%llu	%d\n",
			now / params.TimeUnit, now % params.TimeUnit, nord, type);
	} else if (type == MSG_WRITE) {
		nowr += delta;
		swr->Record("%llu.%09llu	%llu	%d\n",
			now / params.TimeUnit, now % params.TimeUnit, nowr, type);
	}

	if (delta == -1)
		cur = sex;
	else if (delta == 1)
		cur = sen;
	cur->Record("%llu.%09llu	%llu	%llu	c%d	%d	%llu@%llu:%llu\n",
			now / params.TimeUnit, now % params.TimeUnit,
			norpc, msg->GetKey(), msg->cid, type, io->fid, io->off, io->count);
}

void OST::Enqueue(Message *msg)
{
	Server::Enqueue(msg);

	StatNrs(msg, 1);
}

void OST::PingStateMachine(ThreadLocalData *tld)
{
	Processor *p = tld->p;
	Message *msg = tld->m;
	Node *site = tld->n;
	Thread *t = tld->t;
	
	assert(msg->GetType() == MSG_PING);
	
	switch (tld->state) {
	case ost_PingStartState: {
		printf(NOW"%s RUN PID@%d to handle REQ@%d\n", 
			now, site->GetDeviceName(), tld->id, msg->GetId());
		t->RunAfter(params.handle.PingHandleTicks);

		tld->state = ost_PingCompletionState;
		break;
	}
	case ost_PingCompletionState:
		printf(NOW "%s Handled REQ@%d, SEND reply.\n", 
			now, site->GetDeviceName(), msg->GetId());
		msg->SetLength(1024);
		site->Send(msg);
	case ost_PingLastState:
		/* Free the thread into thread pool. */
		p->TaskCompletion(tld);
		break;
	}
}

void OST::WriteStateMachine(ThreadLocalData *tld)
{
	Processor *p = tld->p;
	Message *msg = tld->m;
	IO *io = (IO *)msg->req;
	Thread *t = tld->t;
	
	switch (tld->state) {
	case ost_WriteFirstState:
		if (params.cluster.Scale) {
			t->RunAfter(5000000);
			tld->state = ost_WriteStartState;
			break;
		}
	case ost_WriteStartState: {
		Print(NOW "%s RUN PID@%d to handle REQ@%d:%p (io, fid:%llu).\n",
			now, name, tld->id, msg->GetId(), msg, io->fid);
		Print(NOW "%s SEND bulk GET REQ@%d.\n",
			now, name, msg->GetId());
		msg->SetWorker(t);
		msg->SetLength(1024);
		Send(msg);
		if (params.test.NetworkNodisk)
			tld->state = ost_WriteCompletionState;
		else 
			tld->state = ost_WriteDiskIOState;
		/*Wait until the completion of bulk data transimission.*/ 
		break;
	}
	/* Finish bulk date tranfer, start the Disk IO.*/
	case ost_WriteDiskIOState: {
		cfs_duration_t	ticks;
		
		/* Simple Disk Model. */
		Print(NOW "%s FINI bulk data tranfer,"
			"Starting disk IO for REQ@%d. (pid@%d)\n",
			now, name, msg->GetId(), tld->id);
		//msg->SetTag("diskio");
		tld->state = ost_WriteCompletionState;
		osd.Write(io);
		break;
	}
	case ost_WriteCompletionState: {
		Print(NOW "%s Finished Disk write for REQ@%d,"
			"Sending reply to client.\n",
			now, name, msg->GetId());
		//msg->ReverseChannel();
		msg->SetLength(1024);
		msg->phase = RPC_PHASE_COMPLETE;
		Send(msg);
		p->TaskCompletion(tld);
	}
	}
}

void OST::ReadStateMachine(ThreadLocalData *tld)
{
	Processor *p = tld->p;
	Message *msg = tld->m;
	IO *io = (IO *) msg->req;
	Thread *t = tld->t;

	switch (tld->state) {
	case ost_ReadFirstState:
		if (params.cluster.Scale) {
			t->RunAfter(1000000000);
			tld->state = ost_ReadFileIOState;
			break;
		}
	case ost_ReadFileIOState:{
		cfs_duration_t ticks;
			
		Print(NOW "%s RUN PID@%d to handle read REQ@%d (io, fid:%llu).\n",
			now, name, tld->id, msg->GetId(), io->fid);
		Print(NOW "%s BEGIN Disk I/O for REQ@%d.\n",
			now, name, msg->GetId());
		msg->SetWorker(t);
		//msg->phase++;
		if (!params.test.NetworkNodisk) {
			tld->state = ost_ReadSendPutState;
			osd.Read(io);
			break;
		}
	}
	case ost_ReadSendPutState: {
		Print(NOW "%s FINI Disk I/O and SEND PUT REQ@%d.\n",
			now, name, msg->GetId());
		msg->SetLength(1024);
		Send(msg);
		tld->state = ost_ReadRecvPutAckState;
		break;
	}
	case ost_ReadRecvPutAckState:
		Print(NOW "%s RECV PUT Ack for REQ@%d,"
			"Tranfering bulk data.\n",
			now, name, msg->GetId());
		msg->SetLength(io->count);
		/*Notify Server When complete the bulk data tranfer. */
		msg->nt = 1;
		msg->phase = RPC_PHASE_BULK;
		Send(msg);
		tld->state = ost_ReadCompletionState;
		break;
	case ost_ReadCompletionState:
		/* Wait until the bulk tranfer completion,
		 * release used resource.*/
		/* FIXME: read reply. */
		Print(NOW "%s Release resource: PID@%d.\n",
			now, name, tld->id); 
		p->TaskCompletion(tld);
		break;
	}
}

void OST::InitStat()
{
	char s[MAX_NAME_LEN];
	
	/* NIC Statictics */
	if (params.cluster.OstCount > 1 && !((id == 0) ||
	    (id == params.cluster.OstCount / 2) ||
	    (id == params.cluster.OstCount - 1)))
		return;

	__stat = params.stat.NIC;
	if (!params.stat.OSTNrs)
		return;

	sprintf(s, "%s.nrs.rpc", name);
	srpc = new Stat(s);
	sprintf(s, "%s.nrs.write", name);
	swr = new Stat(s);
	sprintf(s, "%s.nrs.read", name);
	srd = new Stat(s);
	sprintf(s, "%s.nrs.en", name);
	sen = new Stat(s);
	sprintf(s, "%s.nrs.ex", name);
	sex = new Stat(s);
}

obd_count OST::GetQueuedRPCNum()
{
	assert(norpc == nrs->Size());
	return nrs->Size();
}

io_queue *OST::GetIoQueue(obd_id fid, int rw)
{
	return osd.GetIoQueue(fid, rw);
}

/*
void OST::SetScheduler(int algo)
{

}
*/

void OST::Handle(void *arg)
{
	ThreadLocalData *tld = (ThreadLocalData *) arg;
	OST *ost = (OST *) tld->n;
	Message *msg = tld->m;
	int type = msg->GetType();

	if (msg->phase == RPC_PHASE_NEW) {
		ost->StatNrs(msg, -1);
		SetRPCHandleTime(msg, now);
	}
	
	msg->phase = RPC_PHASE_RPC;

	switch(type) {
	case MSG_PING:
		ost->PingStateMachine(tld);
		break;
	case MSG_READ:
		ost->ReadStateMachine(tld);
		break;
	case MSG_WRITE:
		ost->WriteStateMachine(tld);
		break;
	}
}

int OST::GetCount()
{
	return num;
}

int OST::GetNid(int i)
{
	return set[i]->nid;
}

void OST::Start()
{
    Print(NOW"%s is starting - ThreadNum@%d...\n", now, name, thnr);

	algo = params.nrs.algo;

	InitStat();
	Server::Start();
	handler = Handle;
	osd.Attach(this);
	osd.Setup();
}
