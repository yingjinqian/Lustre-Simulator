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
#include "message.h"
#include "ptlrpc.h"

cfs_time_t RPC::startime;
uint32_t RPC::rto;
uint32_t RPC::numrpc;
cfs_duration_t RPC::tot_rtt;
uint32_t RPC::numrto;
cfs_duration_t RPC::tot_toval;
Stat *RPC::st = NULL;
Stat *RPC::est = NULL;
cfs_duration_t RPC::max_servtime;
cfs_duration_t RPC::max_worktime;
cfs_duration_t RPC::max_rtttime;
cfs_duration_t RPC::max_netlatency;
cfs_time_t RPC::wktbegin = 10000000; /* 10 ms */
cfs_duration_t RPC::wktstep = 20000000; /* 20 ms */
cfs_time_t RPC::snaptime = 50 * params.TimeUnit;
unsigned int RPC::rttbegin = 30;
unsigned int RPC::rttstep = 10;
unsigned int RPC::rtt_hist[MAX_RPC_SLOTS];
unsigned int RPC::wkt_hist[MAX_RPC_SLOTS];

RPC::RPC()
{
	memset(&rbnode, 0, sizeof(rbnode));
	rbnode.rb_entry = this;

	sendTime = 0;
	arrivalTime = 0;
	handleTime = 0;
	workTime = 0;
	servTime = 0;
	deadline = 0;
	srvdl = 0;
	estServTime = 0;
	rtt = 0;
	erpcnt = 0;
	imp = NULL;
	timedout = 0;
	erp_timed = 0;
	poll_end = 0;
	msg = NULL;
	pm = NULL;
	orig = NULL;
	data = NULL;

	
}

RPC::~RPC()
{}

int RPC::GetPhase()
{
	LASSERT(msg != NULL);
	return msg->phase;
}

void RPC::SetPhase(int phase)
{
	LASSERT(msg != NULL);
	msg->phase = phase;
}

void RPC::Copy(RPC *rpc)
{
	sendTime = rpc->sendTime;
	arrivalTime = rpc->arrivalTime;
	handleTime = rpc->handleTime;
	workTime = rpc->workTime;
	servTime = rpc->servTime;
	estServTime = rpc->estServTime;
	rtt = rpc->rtt;
	imp = rpc->imp;
	orig = rpc;
}

int RPC::GetId()
{
	return msg->GetId();
}

cfs_time_t RPC::GetMaxRTT()
{
	return max_rtttime;
}

cfs_time_t RPC::GetMaxServTime()
{
	return max_servtime;
}

cfs_duration_t RPC::GetAvgRTT()
{
	return (tot_rtt / numrpc);
}

void RPC::InitRPCTimeout(const char *str)
{	
	if (params.stat.Timeout && params.ptlrpc.ToON) {
		st = new Stat(str);
		if (params.ptlrpc.AtON) {
			est = new Stat("EST.st");
			est->Record("# time	EST	ST	AST	VAN	AET\n");
		}
	}
}

void RPC::FiniRPCTimeout()
{
	if (st) {
		int i;
		cfs_time_t t;

		st->Record("# Timeout Summary:\n"
			   "# At algo: %s\n"
			   "# Total run time: %llu\n"
			   "# Total RPC No.: %lu\n"
			   "# Timedout RPC No.: %lu\n"
			   "# Max. RPC service time: %llu s\n"
			   "# Max. RPC execute time: %llu ms\n"
			   "# Max. RPC net latency: %llu ms\n"
			   "# Max. RPC RTT: %llu s\n"
			   "# Timeout Rate: %lu%\n",
			   Ptlrpc::GetAtSrvAlgo(),
			   Event::Clock() / params.TimeUnit,
			   numrpc, numrto, max_servtime / params.TimeUnit, 
			   max_worktime / 1000000, max_netlatency / 1000000,
			   max_rtttime / params.TimeUnit, 
			   numrto * 100 / numrpc);

		#define PCT(val) ((val) * 100 / numrpc)
		st->Record("\n# RPC RTT statistics:\n"
			   "# [start, end]:\tcount\t pct%\n");
		if (rtt_hist[0] != 0)
			st->Record("# [0s, %lus]:\t%lu\t%lu%\n", rttbegin, rtt_hist[0], PCT(rtt_hist[0]));
		for (i = 1, t = rttbegin; i < MAX_RPC_SLOTS - 1; i++, t += rttstep) {
			if (rtt_hist[i] != 0) {
				st->Record("# [%llus, %llus]:\t%lu\t%lu%\n",
					   t, t + rttstep, rtt_hist[i], 
				           PCT(rtt_hist[i]));
			}
		}

		if (rtt_hist[MAX_RPC_SLOTS - 1] != 0)
			st->Record("# [%lus, -->]:\t%lu\t%lu%\n",
				   t, rtt_hist[MAX_RPC_SLOTS - 1],
				   PCT(rtt_hist[MAX_RPC_SLOTS - 1]));

		st->Record("\n# RPC work time statistics:\n");
		if (wkt_hist[0] != 0)
			st->Record("# [0ms, %llums]:\t%lu\t%lu%\n",
				   wktbegin / 1000000, wkt_hist[0], PCT(wkt_hist[0]));
		for (i = 1, t = wktbegin; i < MAX_RPC_SLOTS - 1; i++, t += wktstep) {
			if (wkt_hist[i] != 0) {
				st->Record("# [%llums, %llums]:\t%lu\t%lu%\n",
					   t/ 1000000, (t + wktstep) / 1000000, 
					   wkt_hist[i], PCT(wkt_hist[i]));
			}
		}
		if (wkt_hist[MAX_RPC_SLOTS - 1] != 0)
			st->Record("# [%llus, -->]:\t%lu\t%lu%\n",
				   t / 1000000, wkt_hist[MAX_RPC_SLOTS - 1],
				   PCT(wkt_hist[MAX_RPC_SLOTS - 1]));

		delete st;
	}
	if (est)
		delete est;
}

uint32_t Message::num;

Message::Message()
 : Packet()
{
	memset(&rbn, 0, sizeof(rbn));
	rbn.rb_entry = this;

	id  = num++;
	rpc.msg = this;
	Init(MSG_NONE, -1, -1);
}

Message::Message(int t, int s, int d)
	: Packet(s, d)
{
	memset(&rbn, 0, sizeof(rbn));
	rbn.rb_entry = this;

	id = num++;
	rpc.msg = this;
	Init(t, s, d);
}

Message::~Message()
{}

void Message::Init(int t, int s, int d)
{
	type = t;
	src = s;
	dst = d;
	r = 0;
	nt = 0;
	left = 0;
	slice = 0;
	waiter = NULL;
	worker[0] = worker[1] = NULL;

	memset(&io, 0, sizeof(io));
	io.data = this;

	req = NULL;
	key = 0;
	flags = 0;
	sid = cid = 0;

	InitRPC();
}

void Message::InitRPC()
{
	phase = RPC_PHASE_NEW;
	rpc.timedout = 0;
	rpc.erp_timed = 0;
	rpc.sendTime = 0;
	rpc.arrivalTime = 0;
	rpc.workTime = 0;
	rpc.deadline = 0;
	rpc.srvdl = 0;
	rpc.handleTime = 0;
	rpc.servTime = 0;
	rpc.rtt = 0;
	rpc.estServTime = 0;
	rpc.erpcnt = 0;
	rpc.imp = NULL;
	rpc.orig = NULL;
}

char *Message::GetPhase()
{
	switch(phase) {
	case RPC_PHASE_NEW:
		return "RPC_PHASE_NEW";
	case RPC_PHASE_RPC:
		return "RPC_PHASE_RPC";
	case RPC_PHASE_BULK:
		return "RPC_PHASE_BULK";
	case RPC_PHASE_INTERPRET:
		return "RPC_PHASE_INTERPRET";
	case RPC_PHASE_COMPLETE:
		return "RPC_PHASE_COMPLETE";
	case RPC_PHASE_EARLY_REPLY:
		return "RPC_PHASE_EARLY_REPLY";
	case RPC_PHASE_POLLING:
		return "RPC_PHASE_POLLING";
	case RPC_PHASE_POLL_INTERVAL:
		return "RPC_PHASE_POLL_INTERVAL";
	default:
		return "RPC_PHASE_UNKNOW";
	}
}

char *Message::GetTypeStr()
{
	switch(type) {
	case MSG_READ:
		return "MSG_READ";
	case MSG_WRITE:
		return "MSG_WRITE";
	case MSG_PING:
		return "MSG_PING";
	}

	return "MSG_UNKNOWN";
}

RPC *Message::GetRPC()
{
	return &rpc;
}

void Message::SetImport(obd_import *imp)
{
	rpc.imp = imp;
}

void Message::SetLength(obd_count len)
{
	msz = left = len;
}

void Message::SetRecvTime(cfs_time_t t)
{
	runTime = t;
	if (params.nrs.algo == NRS_ALGO_FIFO ||
	    params.nrs.algo == NRS_ALGO_FCFS)
		key = t;
}

cfs_time_t Message::GetRecvTime()
{
	return runTime;
}

obd_count Message::GetKey()
{
	return key;
}

int Message::GetType()
{
	return type;
}

int Message::GetId()
{
	return id;
}

cfs_time_t Message::GetSendTime()
{
	return rpc.sendTime;
}

Packet *Message::GetPacket()
{
	Packet *pkt;
	
	pkt = new Packet;
	if (r) {
		pkt->src = dst;
		pkt->dst = src;
	} else {
		pkt->src = src;
		pkt->dst = dst;
	}
	pkt->size = min(params.network.PacketSize, left);
	left -= pkt->size;
	if (left == 0) {
		pkt->state |= NET_PKT_MSG;
	}
	pkt->data = this;
	pkt->id = slice++;

	return pkt;
}

void Message::SetIO(obd_id id, int cmd, obd_off offset, obd_count len)
{
	io.fid = id;
	io.off = offset;
	io.count = len;
	io.cmd = cmd;
}

obd_count Message::GetIOCount()
{
	return io.count;
}

void Message::SetWorker(Thread *t)
{
	worker[r] = t;
}

void Message::ReverseChannel()
{
	r = !r;
}

void Message::Notify()
{
	worker[r]->Signal();
}

void Message::Copy(Message *msg)
{
	src = msg->src;
	dst = msg->dst;
	type = msg->type;
	r = msg->r;
}

cfs_duration_t Message::GetRTT()
{
	return (now - rpc.sendTime);
}


cfs_duration_t Message::GetServTime()
{
	return rpc.servTime;
}

cfs_time_t Message::GetArrivalTime()
{
	return rpc.arrivalTime;
}

rb_node *Message::GetKey(void *e)
{
	return &(((Message *)e)->rbn);
}

int Message::GetRCC()
{
	return rpc.rcc;
}
