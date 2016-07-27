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
#include "nic.h"
#include "peer.h"
#include "network.h"

/* For network test without disk I/O */
#include "ost.h"

#define MSG_LOG_BASE 10
#define MIN_MSG_SIZE  (1 << MSG_LOG_BASE)
#define MAX_MSG_SIZE 33554432 /* 32M */
#define MAX_MSG_LOG 20

#define IN  0
#define OUT 1

NIC::NIC()
		: NetDevice()
{
	txm = NULL;
	txp = rxp = cnp = NULL;
	st[0] = st[1] = NULL;
	log[0] = log[1] = NULL;
	__dbg = params.debug.NIC;
	ibytes = obytes = 0;
}


NIC::~NIC()
{
	if (__stat) {
		RecordMessageSize();
		for (int i = 0; i < 2; i++) {
			delete st[i];
			delete [] log[i];
		}
	}
}

void NIC::Connect()
{
	target = Network::Instance();
	target->Connect();
}

void NIC::Send(Message *msg)
{
	StatMessageSize(OUT, msg);
	msgQ.Enqueue(msg);
	if (sendState <= nic_SendIdleState) {
		if (sendState == nic_SendIdleState)
			sth.Signal();
		sendState = nic_SendPostMessageState;
	}
}

void NIC::Recv(Packet *pkt)
{
	inQ.Enqueue(pkt);
	if (recvState <= nic_RecvIdleState) {
		if (recvState == nic_RecvIdleState)
			rth.Signal();
		recvState = nic_RecvRxPacketState;
	}
}

void NIC::Notify(int state, Packet *pkt)
{
	switch (state) {
	case NET_PKT_TXDONE:
		assert(pkt == txp && sendState == nic_SendTxPacketState);
		if (txp->state & NET_PKT_MSG)
			sendState = nic_SendPostMessageState;
		else
			sendState = nic_SendPostPacketState;

		StatOutBandwidth(pkt->size);
		break;
	case NET_PKT_RETX:
		assert(pkt == txp && pkt == cnp &&
		       sendState == nic_SendCongestState);
		/* retransmit the congested packet. */
		sendState = nic_SendTxPacketState;
		break;
	case NET_PKT_BLK:
		assert(pkt == txp && sendState == nic_SendTxPacketState);
		sendState = nic_SendCongestState;
		cnp = pkt;
		return;
	}

	SendStateMachine();
}

void NIC::SendStateMachine()
{
	switch (sendState) {
	case nic_SendPostMessageState:
		txm = (Message *) msgQ.Dequeue();
		if (txm == NULL) {
			sendState = nic_SendIdleState;
			break;
		}
	case nic_SendPostPacketState:
		txp = txm->GetPacket();
		assert(txp != NULL);
		sendState = nic_SendTxPacketState;
	case nic_SendTxPacketState:
		NetDevice::Send(txp);
		break;
	case nic_SendStartState:
		sendState = nic_SendIdleState;
	case nic_SendIdleState:
	case nic_SendCongestState:
	case nic_SendLastState:
		break;
	}
}

void NIC::RecvStateMachine()
{
	switch (recvState) {
	case nic_RecvRxPacketState:
		rxp = (Packet *)inQ.Dequeue();
		if (rxp == NULL) {
			recvState = nic_RecvIdleState;
			break;
		}

		rth.RunAfter(max((cfs_duration_t)1, TxTime(rxp->size)));
		recvState = nic_RecvRxDoneState;
		break;
	case nic_RecvRxDoneState:
		target->Notify(NET_PKT_RXDONE, NULL);
		if (rxp->state & NET_PKT_MSG) {
			Message *m = (Message *) rxp->data;

			//Print(NOW"%s receives the msg@%d.\n", now, name, m->GetId());
			/* Notify the sender when the tranfer finished. */
			if (m->nt)
				m->Notify();

			m->ReverseChannel();
			if (m->GetType() == MSG_LATENCY)
			{
				//m->phase++;
				Recv(m);
			} else
				DeliverMessage(m);

			StatMessageSize(IN, m);
		}

		StatInBandwidth(rxp->size);
		delete rxp;
		recvState = nic_RecvRxPacketState;
		RecvStateMachine();
		break;
	case nic_RecvStartState:
		recvState = nic_RecvIdleState;
	case nic_RecvIdleState:
	case nic_RecvLastState:
		break;
	}
}

void NIC::ProcessSend(void *arg)
{
	NIC *nic = (NIC *) arg;

	nic->SendStateMachine();
}

void NIC::ProcessRecv(void *arg)
{
	NIC *nic = (NIC *) arg;

	nic->RecvStateMachine();
}

void NIC::MessageInterrupt(void *arg)
{
	Message *msg = (Message *) arg;
	NIC *nic = (NIC *) msg->data;

	//msg->phase++;
	nic->Recv(msg);
}

void NIC::DeliverMessage(Message *msg)
{
	cfs_time_t time;

	msg->data = this;
	msg->InitEvent(MessageInterrupt, msg);
	msg->RunAfter(time = InterruptLatency());
	//Print(NOW"%s Deliver msg@%d after interrupt latency (at %llu).\n",
	//      now, name, msg->GetId(), time + now);
}

void NIC::StatInBandwidth(obd_count count)
{
	if (__stat) {
		cfs_duration_t ticks;

		ticks = now - itime;
		ibytes += count;
		if (ticks >= params.TimeUnit) {
			obd_count bw;
			obd_count unit;

			unit = 1024;
			bw = ibytes * params.TimeUnit / ticks;
			st[IN]->Record("%llu.%09llu %llu.%03llu\n",
				now / params.TimeUnit, now % params.TimeUnit, 
				bw / unit, (bw % unit) * 1000 / unit);
			ibytes = 0;
			itime = now;
		}
	}
}

void NIC::StatOutBandwidth(obd_count count)
{
	if (__stat) {
		cfs_duration_t ticks;

		ticks = now - otime;
		obytes += count;
		if (ticks >= params.TimeUnit) {
			obd_count bw;
			obd_count unit;

			unit = 1024; //params.SizeUnit;
			bw = obytes * params.TimeUnit / unit / ticks;
			st[OUT]->Record("%llu.%09llu %llu\n",
				now / params.TimeUnit, now % params.TimeUnit, bw);
			obytes = 0;
			otime = now;
		}
	}
}

void NIC::StatMessageSize(int i, Message *msg)
{
	if (!__stat)
		return;

	unsigned int val;
	obd_count msz = msg->msz;

	for (val = 0; ((1 << val) < msz) && (val < MAX_MSG_SIZE); val++)
		;
	if (val < MSG_LOG_BASE)
		val = MSG_LOG_BASE;

	assert(val - MSG_LOG_BASE <= MAX_MSG_LOG &&
	       val - MSG_LOG_BASE >= 0);
	log[i][val - MSG_LOG_BASE]++;
}

void NIC::RecordMessageSize()
{
#define pct(a,b) ((b) ? (((a) * 100) / b) : 0)

	for (int i = 0; i < 2; i++) {
		int j, max = 0;
		unsigned int tot = 0;
		obd_count msz = MIN_MSG_SIZE;

		for (j = 0; j < MAX_MSG_LOG; j++) {
			if (log[i][j] > 0) {
				max = j;
				tot += log[i][j];
			}
		}

		st[i]->Record("# nic@%d message size stat (tot %lu):\n"
		              "# fmt (msz[kb],count,pct%)\n", nid, tot);
		for (j = 0; j <= max; j++, msz *= 2) {
			if (log[i][j] > 0)
				st[i]->Record("# %llu	%lu	%lu\n",
				              msz / 1024, log[i][j], pct(log[i][j], tot));
		}
	}
}

/*
 * Latency is measured by sending a packet that
 * is returned to the sender and the round-trip time
 * is considered as the latency.
 */
void NIC::LatencyTest()
{
	nctl ctl;
	Peer s, d;

	memset(&ctl, 0, sizeof(ctl));
	ctl.mt = MSG_LATENCY;
	ctl.snr = ctl.dnr = 1;
	ctl.repeat = 30;
	ctl.factor = 2;
	ctl.msz = 1;
	ctl.maxsz = 4 * 1048576; /* 1M */
	ctl.stat = new Stat("nic.latency");
	s.InitPeer(SENDER, 1, &d, &ctl);
	d.InitPeer(RECEIVER, 0, NULL, &ctl);
	s.Start();
	d.Start();

	Event::Schedule();

	delete ctl.stat;
}

void NIC::BandwidthTest()
{
	nctl ctl;
	Peer *src, *dst;


	memset(&ctl, 0, sizeof(ctl));
	ctl.mt = MSG_BANDWIDTH;
	ctl.snr = 60;
	ctl.dnr = 1;
	ctl.msz = 1048576;
	ctl.repeat = 1024;
	src = new Peer [ctl.snr];
	dst = new Peer;

	dst->InitPeer(RECEIVER, 0, NULL, &ctl);
	dst->Start();
	for (int i = 0; i < ctl.snr; i ++) {
		src[i].InitPeer(SENDER, 1, dst, &ctl);
		src[i].Start();
	}

	Event::Schedule();

	delete [] src;
	delete dst;
}

void NIC::PerformanceTest()
{
	nctl ctl;
	Peer s, d;

	memset(&ctl, 0, sizeof(ctl));
	ctl.mt = MSG_NICPERF;
	ctl.snr = 1;
	ctl.dnr = 1;
	ctl.msz = 1048576;
	ctl.repeat = 2048;  /* 2G data */
	ctl.stat = new Stat("nic.perf");
	s.InitPeer(SENDER, 1, &d, &ctl);
	d.InitPeer(RECEIVER, 0, NULL, &ctl);
	s.Start();
	d.Start();

	Event::Schedule();

	delete ctl.stat;
}

void NIC::OutgoingTest()
{
	nctl ctl;
	Peer *src, *dst;

	memset(&ctl, 0, sizeof(ctl));
	ctl.mt = MSG_BANDWIDTH;
	ctl.snr = 1;
	ctl.dnr = 6;
	ctl.msz = 1048576;
	ctl.repeat = 1024;
	src = new Peer;
	dst = new Peer [ctl.dnr];

	src->InitPeer(SENDER, ctl.dnr, dst, &ctl);
	src->Start();
	for (int i = 0; i < ctl.dnr; i++) {
		dst[i].InitPeer(RECEIVER, 0, NULL, &ctl);
		dst[i].Start();
	}

	Event::Schedule();

	delete src;
	delete [] dst;
}

void NIC::NetworkTest()
{
	int i;
	nctl ctl;
	Peer *rw;
	Peer *srv;

	memset(&ctl, 0, sizeof(ctl));
	ctl.mt = MSG_NETWORK;
	ctl.wtnr = 10;
	ctl.rdnr = 10;
	ctl.srvnr = 1;
	ctl.wmsz = 1048576;
	ctl.rmsz = 4094;//1048576;

	srv = new Peer [ctl.srvnr];
	for (i = 0; i < ctl.srvnr; i++) {
		srv[i].InitPeer(SERVER, 0, NULL, &ctl);
		srv[i].Start();
	}

	rw = new Peer [ctl.rdnr + ctl.wtnr];
	for (i = 0; i < ctl.rdnr; i++) {
		rw[i].InitPeer(READER, 1, srv, &ctl);
		rw[i].Start();
	}
	for ( ; i < ctl.rdnr + ctl.wtnr; i++) {
		rw[i].InitPeer(WRITER, 1, srv, &ctl);
		rw[i].Start();
	}

	Thread::SetRunTicks(params.test.NetTestTicks);
	Thread::Schedule();

	delete [] srv;
	delete [] rw;
}

void NIC::NetNodiskTest()
{
	int i;
	nctl ctl;
	Peer *rw;
	OST *srv;

	memset(&ctl, 0, sizeof(ctl));
	ctl.mt = MSG_NETWORK;
	ctl.wtnr = 1000;
	ctl.rdnr = 1000;
	ctl.srvnr = 1;
	ctl.repeat = 0;
	ctl.wmsz = 1048576;
	ctl.rmsz = 1048576;
	ctl.nodisk = 1;

	assert(params.test.NetworkNodisk);

	srv = new OST;
	srv->Start();

	rw = new Peer [ctl.rdnr + ctl.wtnr];
	for (i = 0; i < ctl.rdnr; i++) {
		rw[i].InitPeer(READER, 1, srv, &ctl);
		rw[i].Start();
	}
	for ( ; i < ctl.rdnr + ctl.wtnr; i++) {
		rw[i].InitPeer(WRITER, 1, srv, &ctl);
		rw[i].Start();
	}

	Thread::SetRunTicks(params.test.NetTestTicks);
	Thread::Schedule();

	delete [] rw;

}

void NIC::SelfBenchmark()
{
	printf("Start to benchmark the NIC...\n");

	Network::Setup();
	if (params.test.NetLatency)
		LatencyTest();
	if (params.test.NetBandwidth)
		BandwidthTest();
	if (params.test.NicPerformance)
		PerformanceTest();
	if (params.test.Network)
		NetworkTest();
	if (params.test.NetworkNodisk)
		NetNodiskTest();
	Network::Cleanup();
}

void NIC::InitStat()
{
	if (__stat) {
		char str[MAX_NAME_LEN];

		sprintf(str, "%s.nic.in", name, nid);
		st[0] = new Stat(str);
		sprintf(str, "%s.nic.out", name, nid);
		st[1] = new Stat(str);

		for (int i = 0; i < 2; i++) {
			log[i] = new uint32_t [MAX_MSG_LOG];
			memset(log[i], 0, sizeof(uint32_t) * MAX_MSG_LOG);
		}
	}
}

void NIC::Start()
{
	Print(NOW"Start the NIC@%d.\n", now, nid);

	InitStat();

	sendState = nic_SendStartState;
	recvState = nic_RecvStartState;

	sth.CreateThread(ProcessSend, this);
	sth.RunAfter(1);

	rth.CreateThread(ProcessRecv, this);
	rth.RunAfter(1);

	/*Connect to the network */
	Connect();
}
