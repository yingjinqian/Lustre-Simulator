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
#include "peer.h"

uint32_t Peer::num;

Peer::Peer()
 : Node()
{
	type = 0;
	id = num++;
	sprintf(name, "peer%d", id);
	nr = 0;
	tgts = NULL;
	ctl = NULL;
	msg = NULL;
	state = 0;
	finished = 0;
	__dbg = 1;
}


Peer::~Peer()
{
}

void Peer::InitPeer(int t, int n, Node *targets, nctl *c)
{
	type = t;
	nr = n;
	tgts = targets;
	ctl = c;
}

void Peer::RecordPerformance()
{
	cfs_duration_t ticks;
	obd_count bw;
	
	assert(ctl != NULL && ctl->mt == MSG_NICPERF);

	ctl->finished++;
	ctl->totcnt += ctl->msz;
	ticks = now - ctl->stime;
	if (ticks >= params.TimeUnit) {
		bw = ctl->totcnt * params.TimeUnit / params.SizeUnit / ticks;
		ctl->stat->Record("%llu.%09llu %llu\n",
			now / params.TimeUnit, now % params.TimeUnit, bw);
		ctl->stime = now;
		ctl->totcnt = 0;
	}
	if (ctl->finished >= ctl->repeat) {
		bw = ctl->msz * ctl->repeat * params.TimeUnit / params.SizeUnit / now;
		ctl->stat->Record("# Summary: total time %llu.%09llu, bandwidth %llu.\n",
			now / params.TimeUnit, now % params.TimeUnit, bw);
	}
}

void Peer::Recv(Message *m)
{
	if (type == READER || type == WRITER) {
		t.Signal();
		return;
	}

	if (type == SERVER) {
		ProcessNetReadWrite(m);
		return;
	}

	if (ctl->mt == MSG_NICPERF) {
		RecordPerformance();
		delete m;	
		return;
	}

	if (ctl->mt == MSG_BANDWIDTH) {
		delete m;
		return;
	}
	
	assert(ctl->mt == MSG_LATENCY);
	if (m != msg/*(type & RECEIVER)*/) {
		Print(NOW"%s SEND the returned trip message.\n",
					now, name);
		//m->ReverseChannel();
		m->SetLength(ctl->msz);
		Send(m);
	} else if (m == msg/*type == SENDER*/) {
		printf(NOW"%s FINI %dth round trip for msz %llu.\n",
				now, name, ctl->finished + 1, ctl->msz);
		t.Signal();
	}
}

void Peer::RecordLatency()
{
	cfs_duration_t rrt;

	rrt = (now - ctl->stime) / ctl->repeat;
	ctl->stat->Record("%llu %llu.%03llu\n",
		 ctl->msz, rrt / 1000, rrt % 1000);
}

void Peer::LatencyLauncher()
{
	switch (state) {
	case peer_SendStartingState:
		ctl->stime = now;
	case peer_SendMessageState:
		Print(NOW"%s SEND message (sz: %llu).\n",
				now, name, ctl->msz);
		msg->Init(MSG_LATENCY, nid, tgts->nid);
		msg->SetLength(ctl->msz);
		Send(msg);
		state = peer_SendFiniRoundTripState;
		break;
	case peer_SendFiniRoundTripState:
		if (++ctl->finished < ctl->repeat) {
			state = peer_SendMessageState;
			LatencyLauncher();
			break;
		}

		RecordLatency();
		ctl->msz *= ctl->factor;
		if (ctl->msz <= ctl->maxsz) {
			ctl->finished = 0;
			state = peer_SendStartingState;
			LatencyLauncher();
			break;
		}
	case peer_SendLastState:
		delete msg;
		break;
	}
}

void Peer::LatencyStateMachine(void *arg)
{
	Peer *peer = (Peer *) arg;

	peer->LatencyLauncher();
}

void Peer::BandwidthLaunch()
{
	Message *msg;

	assert(type == SENDER);
	for (int i = 0; i < ctl->repeat; i++) {
		for (int j = 0; j < ctl->dnr; j++) {
			msg = new Message(ctl->mt, nid, tgts[j].nid);
			msg->SetLength(ctl->msz);
			Send(msg);
		}
	}		
}

void Peer::WriteNodisk()
{
	IO *io = &msg->io;
	
	switch(state) {
	case peer_WriteNdStartState:
		msg->Init(MSG_WRITE, nid, tgts->nid);
		msg->SetIO(nid, WRITE, 0, ctl->wmsz);
		msg->req = io;
		msg->SetWorker(&t);
		Print(NOW"%s SEND write REQ@%d FID@%llu (%llu:%llu) to %s\n",
			now, name, msg->GetId(), io->fid, io->off, io->count, GetNodeName(msg->dst));
		msg->SetLength(1024);
		Send(msg);
		state = peer_WriteNdRecvGetState;
		break;
	case peer_WriteNdRecvGetState:
		Print(NOW"%s RECV bulk GET REQ@%d, tranfering bulk data.\n",
			now, name, msg->GetId());
		msg->SetLength(io->count);
		Send(msg);
		state = peer_WriteNdCompletionState;
		break;
	case peer_WriteNdCompletionState:
		Print(NOW"%s FINI write REQ@%d (%llu:%llu).\n",
			now, name, msg->GetId(), io->off, io->count);

		state = peer_WriteNdStartState;
		if (ctl->repeat && (finished++ > ctl->repeat))
			state = peer_WriteNdLastState;

		WriteNodisk();
		break;
	case peer_WriteNdLastState:
		Print(NOW"%s FINI All test.\n",
			now, name);
		delete msg;
	}
}

void Peer::Writer()
{
	if (ctl->nodisk) {
		WriteNodisk();
		return;
	}

	switch (state) {
	case peer_WriteStartingState:
		assert(type == WRITER);
		msg = new Message;
		state = peer_WriteBulkDataState;
	case peer_WriteBulkDataState:
		Print(NOW"%s SEND WRITE_NET request.\n", now, name);
		msg->Init(MSG_WRITE_NET, nid, tgts->nid);
		msg->SetLength(ctl->wmsz);
		Send(msg);
		break;
	case peer_WriteFinishState:
		break;
	case peer_WriteLastState:
		delete msg;
		break;
	}
}

void Peer::WriteStateMachine(void *arg)
{
	Peer *peer = (Peer *) arg;
	
	peer->Writer();
}

void Peer::ReadNodisk()
{
	IO *io = &msg->io;

	switch (state) {
	case peer_ReadNdStartState:
		msg->Init(MSG_READ, nid, tgts->nid);
		msg->SetIO(nid, READ, 0, ctl->rmsz);
		msg->req = &msg->io;
		msg->SetWorker(&t);
		Print(NOW"%s SEND read REQ@%d FID@%llu %llu:%llu to %s.\n",
			now, name, msg->GetId(), io->fid, io->off, io->count, GetNodeName(msg->dst));
		msg->SetLength(1024);
		Send(msg);
		state = peer_ReadNdRecvPutState;
		break;
	case peer_ReadNdRecvPutState:
		Print(NOW "%s RECV PUT REQ@%d, SEND PUT ACK.\n",
			now, name, msg->GetId());
		msg->SetLength(1024);
		Send(msg);
		state = peer_ReadNdCompletionState;
		break;
	case peer_ReadNdCompletionState:
		Print(NOW "%s FINI read REQ@%d (%llu:%llu).\n", 
			now, name, msg->GetId(), io->off, io->count);
		state = peer_ReadNdStartState;
		if (ctl->repeat && (finished++ > ctl->repeat))
			state = peer_ReadNdLastState; 
		ReadNodisk();
		break;
	case peer_ReadNdLastState:
		delete msg;
		break;
	}
}

void Peer::Reader()
{
	if (ctl->nodisk) {
		ReadNodisk();
		return;
	}

	switch (state) {
	case peer_ReadStartingState:
		assert(type == READER);
		msg = new Message;
		state = peer_ReadBulkDataState;
	case peer_ReadBulkDataState:
		Print(NOW"%s SEND READ_NET request.\n", now, name);
		msg->Init(MSG_READ_NET, nid, tgts->nid);
		msg->SetIO(nid, READ, 0, ctl->rmsz);
		msg->SetLength(1024);
		Send(msg);
		break;
	case peer_ReadFinishState:
		break;
	case peer_ReadLastState:
		delete msg;
		break;
	}
}

void Peer::ReadStateMachine(void *arg)
{
	Peer *peer = (Peer *) arg;

	peer->Reader();
}

void Peer::ProcessNetReadWrite(Message *msg)
{
	msg->ReverseChannel();

	switch (msg->GetType()) {
	case MSG_READ_NET:
		msg->SetLength(msg->GetIOCount());
		Print(NOW"%s Recv the READ_NET request  "
			"and SEND the read bulk data (sz: %llu).\n",
			now, name, msg->msz);
		
		Send(msg);
		break;
	case MSG_WRITE_NET:
		Print(NOW"%s Recv the WRITE_NET msg (sz: %llu) and SEND the reply.\n",
			now, name, msg->msz);
		msg->SetLength(1024);
		Send(msg);
		break;
	}
}

void Peer::Start()
{
	__stat = (ctl->mt == MSG_BANDWIDTH ||
            ctl->mt == MSG_NICPERF || 
           /* type == READER || type == WRITER ||*/
            type == SERVER);

	Print(NOW"%s is starting...\n", now, name);
	
	NIC::Start();

	assert(ctl != NULL);

	if (type == RECEIVER || type == SERVER)
		return;

	if (type == WRITER){
		assert(nr == 1);
		msg = new Message;
		t.CreateThread(WriteStateMachine, this);
		t.RunAfter(2);
		return;
	}
	
	if (type == READER) {
		assert(nr == 1);
		msg = new Message;
		t.CreateThread(ReadStateMachine, this);
		t.RunAfter(2);
		return;
	}

	// SENDER
	if (ctl->mt == MSG_LATENCY) {
		msg = new Message;
		t.CreateThread(LatencyStateMachine, this);
		t.RunAfter(2);
	} else if (ctl->mt == MSG_BANDWIDTH ||
		        ctl->mt == MSG_NICPERF) {
			BandwidthLaunch();
	}	
}
