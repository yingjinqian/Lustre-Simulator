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
#include "server.h"
#include "nrsfifo.h"
#include "nrsrbtree.h"
#include "nrsepoch.h"
#include "nrsfrr.h"
#include "nrsprio.h"
#include "nrstbf.h"

Server::Server()
 : Ptlrpc()
{
	__type = NODE_SERVER;
	thnr = 1;
	algo = 0;
	nrs = NULL;
}

Server::~Server()
{
	if (nrs) {
        delete nrs;
	}
}

void Server::Enqueue(Message *msg)
{
	cpu.AddNewTask(msg);
	StatRpc(msg, RPC_ENQUEUE);
}

void Server::Recv(Message *msg)
{
	Ptlrpc::Recv(msg);

	/* filter the POLL and PING message */
	if (msg->GetType() == MSG_POLL ||
	    msg->GetType() == MSG_PING)
		return;

	msg->SetRecvTime(now);

	if (msg->phase == RPC_PHASE_NEW)
		Enqueue(msg);
	else /* Wake up the waiting thread */
		msg->Notify();
}

io_queue *Server::GetIoQueue(obd_id fid, int rw)
{
	return NULL;
}

int Server::TaskCompare(void *a, void *b)
{
	return (((Message *)a)->GetKey() > ((Message *)b)->GetKey() ? 1 : -1);
}

int Server::CompareByObjid(void *a, void *b)
{
	IO *o1, *o2;
	
	o1 = (IO *)((Message *)a)->req;
	o2 = (IO *)((Message *)b)->req;

	if (o1->fid > o2->fid)
		return 1;
	if (o1->fid < o2->fid)
		return -1;

	BUG_ON(o1->off == o2->off);
	return (o1->off > o2->off) ? 1: -1;
}

int Server::CompareByDeadline(void *a, void *b)
{
	RPC *r1, *r2;

	r1 = ((Message *)a)->GetRPC();
	r2 = ((Message *)b)->GetRPC();

	return (r1->deadline < r2->deadline) ? 1: -1;
}

void Server::SetScheduler(int a)
{
	if (nrs)
		delete nrs;

	algo = a ? : NRS_ALGO_FIFO;

	if (algo == NRS_ALGO_FIFO || algo == NRS_ALGO_FCFS) {
		nrs = new NrsFifo;
	} else if (algo == NRS_ALGO_BYOBJID) {
		nrs = new NrsRbtree(CompareByObjid);
	} else if (algo == NRS_ALGO_BYDEADLINE) {
		nrs = new NrsRbtree(CompareByDeadline);
	} else if (algo == NRS_ALGO_RBTREE) {
		nrs = new NrsRbtree(TaskCompare);
	} else if (algo == NRS_ALGO_FRR) {
		nrs = new NrsFRR(this);
	} else if (algo == NRS_ALGO_EPOCH) {
		nrs = new NrsEpoch;
	} else if (algo == NRS_ALGO_PRIO) {
		nrs = new NrsPrio;
    } else if (algo == NRS_ALGO_TBF) {
        nrs = new NrsTbf;
    }
}

void Server::Start()
{
	Ptlrpc::Start();

	SetScheduler(algo);

    assert(nrs != NULL && thnr != 0);
	cpu.Attach(this, thnr, nrs);
	cpu.Start();
}


