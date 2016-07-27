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
#ifndef NIC_H
#define NIC_H

#include <netdevice.h>
#include <nrsfifo.h>

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/
class NIC : public NetDevice
{
	enum {
		nic_SendStartState,
		nic_SendIdleState,
		nic_SendPostMessageState,
		nic_SendPostPacketState,
		nic_SendTxPacketState,
		nic_SendCongestState,
		nic_SendLastState,
	} sendState;
	NrsFifo msgQ;
	NrsFifo outQ;
	Message *txm;
	Packet *txp;
	Thread sth;

	enum {
		nic_RecvStartState,
		nic_RecvIdleState,
		nic_RecvRxPacketState,
		nic_RecvRxDoneState,
		nic_RecvLastState,
	} recvState;
	NrsFifo inQ;
	Packet *rxp;
	Thread rth;

	Packet *cnp; /* congested packet */

	void Recv(Packet *pkt);
	//void Send(Packet *pkt);

	void Notify(int state, Packet *pkt);
	void Connect(); /* connect to the netowrk */
	
	void DeliverMessage(Message *msg);
	static void MessageInterrupt(void *arg);

	void SendStateMachine();
	void RecvStateMachine();
	static void ProcessSend(void *arg);
	static void ProcessRecv(void *arg);

/* Benchmark */
	Stat *st[2];
	uint32_t *log[2];
	obd_count ibytes;
	obd_count obytes;
	cfs_time_t itime;
	cfs_time_t otime;

	static void LatencyTest();
	static void BandwidthTest();
	static void PerformanceTest();
	static void OutgoingTest();
	static void NetworkTest(); /*complicated case */
	static void NetNodiskTest();

	char *GetSendState(int state);
	char *GetRecvState(int state);
	
	void InitStat();
	void StatInBandwidth(obd_count count);
	void StatOutBandwidth(obd_count count);
	void StatMessageSize(int in, Message *msg);
	void RecordMessageSize();

public:
	NIC();
	~NIC();

	void Send(Message *msg);
	virtual void Recv(Message *msg) = 0;
	virtual void Start();

	static void SelfBenchmark();
};

#endif
