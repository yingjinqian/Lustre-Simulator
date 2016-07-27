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
#include "osc.h"
#include "client.h"
#include "congestcontroller.h"

OSC::OSC()
 : DataDevice()
{
	id = tnid = 0;
	rpcs_in_flight = 0;
	r_in_flight = 0;
	w_in_flight = 0;
	max_rpcs_in_flight = params.cluster.MaxRpcsInflight;
	r_in_queue = 0;
	w_in_queue = 0;
	pmsg = NULL;
	counter = 0;
	rr_idx = 0;
	__dbg = params.debug.OSC;
    processor = NULL;
}

OSC::OSC(int i, int tid)
 : DataDevice()
{
	id = i;
	rpcs_in_flight = 0;
	r_in_flight = 0;
	w_in_flight = 0;
	r_in_queue = 0;
	w_in_queue = 0;
	max_rpcs_in_flight = params.cluster.MaxRpcsInflight;
	tnid = tid;
	pmsg = NULL;
	counter = 0;
	rr_idx = 0;
	__dbg = params.debug.OSC;
    processor = NULL;
}

OSC::~OSC()
{
    if (processor)
        delete processor;
}

int OSC::RpcsInflight()
{
	return r_in_flight + w_in_flight;
}

void OSC::CheckRpcs()
{
}

obd_count OSC::GetKey(Message *msg)
{
	IO *io = (IO *)msg->req;
	obd_count key;
	int clnr;
/*
	if (!params.nrs.ByKeyVaule)
		return 0;
*/
	if (params.nrs.algo != NRS_ALGO_BYKEY)
		return 0;

	if (params.io.Mode == ACM_FPP)
		clnr = params.cluster.ClientCount;
	else
		//clnr = msg->GetType() == MSG_WRITE ? params.io.WriterCount : params.io.ReaderCount;
		clnr = params.io.WriterCount;
	counter = io->off / 1048576;
	//if (io->cmd == WRITE) {
	//if (params.io.ModeFPP)
	/* Formula 1: */
		//key = cid + counter * clnr;
	
	/* Formula 2: */
		//key = counter;
	
	/* Formula 3: */
	{
		IO *pio = io->parent;
		key = cid + (io->off - pio->off) / 1048576 * clnr;
	}

	return key;
}

int OSC::ReadWrite(IO *io)
{
	Message *msg = new Message;

	if (params.io.Mode == ACM_FPP)
		io->fid = nid;
	else
		io->fid = 0;

	msg->Init(io->cmd ? MSG_WRITE: MSG_READ, nid, tnid);
	msg->SetImport(&imp);
	msg->req = io;
	io->data = msg;
	msg->key = GetKey(msg);
	msg->cid = cid;
	msg->sid = id;
	Print(NOW "%s submits %s request (fid@%llu, %llu:%llu) to %s\n",
		now, name, io->cmd ? "write" : "read", io->fid, 
		io->off, io->count, NetDevice::GetNodeName(tnid));
    processor->AddNewTask(msg);
	return 0;
}

int OSC::Read(IO *io)
{
	return ReadWrite(io);
}

int OSC::Write(IO *io)
{
	return ReadWrite(io);
}

int OSC::Create(Object *object)
{
	ChunkObject *obj = (ChunkObject *) object;

	obj->kms = 0;
	obj->ost_gen = 0;
	obj->ost_idx = id;

	if (params.io.Mode == ACM_FPP)
		obj->SetId(site->GetNid());
	else
		obj->SetId(0);

	return 0;
}

void OSC::PingDaemon(void *arg)
{
	OSC *osc = (OSC *) arg;

	osc->PingStateMachine();
}

void OSC::PingStateMachine()
{
	if (over)
		state = osc_PingLastState;

	switch(state) {
	case osc_PingStartState:
		pmsg = new Message;
	case osc_PingSendState:
		Print(NOW "%s SEND ping REQ@%d\n",
			now, /*site->GetDeviceName()*/name, pmsg->GetId());
		pmsg->Init(MSG_PING, nid, tnid);
		pmsg->SetWorker(&ping);
		pmsg->msz = DEFAULT_MSG_SIZE;
		site->Send(pmsg);
		state = osc_PingRecvReplyState;
		break;
	case osc_PingRecvReplyState:
		Print(NOW "%s RECV reply for ping REQ@%d\n",
			now, /*site->GetDeviceName()*/name, pmsg->GetId());
	case osc_PingIntervalSleepState:
		ping.Sleep(params.handle.IntervalPerPing + 
			SignRand(params.handle.IntervalPerPing * params.TimeUnit / 10));
		state = osc_PingSendState;
		break;
	case osc_PingLastState:
		delete pmsg;
		break;
	}
}

int OSC::Disconnect()
{
	Client *cli = (Client *)site;

	cli->DisconnectImport(&imp);
	return 0;
}

void OSC::Cleanup()
{
	if (pmsg)
		delete pmsg;
}

void OSC::Setup()
{
	Client *cli = (Client *)site;

	sprintf(name, "OSC@%d@%s", id, site->GetDeviceName());
	nid = site->GetNid();
	cid = cli->GetId();

	cli->ConnectImport(&imp);

	if (params.cluster.PingON) {
		ping.CreateThread(PingDaemon, this);
		ping.RunAfter(2);
	}

	/* 8 I/O threads. */
    if (params.cc.FIX) {
        processor = new Processor;
        processor->Attach(site, max_rpcs_in_flight, &rwQ);
        processor->Start();
    } else {
        processor = new CongestController;
        processor->Attach(site, params.cc.RCC);
        processor->Start();
    }
}
