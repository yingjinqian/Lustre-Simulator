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
#include "network.h"

Network *Network::instance;
Stat *Network::stat;

Network::Network()
 : NetDevice()
{
	inflight = 0;
	nodes = 0;
}


Network::~Network()
{}

void Network::TxDone(void *arg)
{
	Packet *pkt = (Packet *) arg;

	//printf(NOW"txdone for pkt.\n", now);
	UpdateState(pkt->src, NET_PKT_TXDONE, pkt);
	pkt->InitEvent(NetTraversed, pkt);
	pkt->RunAfter(NetLatency());
}

void Network::NetTraversed(void *arg)
{
	Packet *pkt = (Packet *) arg;
	NetDevice *dst;

	//printf(NOW"net_travered done.\n", now);
	dst = GetNetDevice(pkt->dst);
	dst->Recv(pkt);
}

void Network::Recv(Packet *pkt)
{
	if (inflight < capacity) {
		inflight++;
		pkt->InitEvent(TxDone, pkt);
		pkt->RunAfter(TxTime(pkt->size));
	} else {
		//printf(NOW"network is congested.\n", now);
		blkQ.Enqueue(pkt);
		UpdateState(pkt->src, NET_PKT_BLK, pkt);
	}
}

void Network::Notify(int state, Packet *pkt)
{
	NetDevice *nd;

	assert(state == NET_PKT_RXDONE && pkt == NULL);
	inflight--;
	pkt = (Packet *)blkQ.Dequeue();
	if (pkt == NULL)
		return;

	//printf(NOW"retransmit the blocked packet.\n", now);
	UpdateState(pkt->src, NET_PKT_RETX, pkt);
}

void Network::Connect()
{
	nodes++;
	//capacity = nodes * params.network.N * params.SizeUnit / params.network.PacketSize;
	capacity = nodes * params.network.N * 128;
	//capacity= 1024 * 256;
}

void Network::Start()
{
	Print("Start the network.\n");
	instance = this;
}

void Network::Setup()
{
	instance = new Network;
	stat = new Stat("network");
	instance->Start();
}

void Network::Cleanup()
{
	delete instance;
	delete stat;
	instance = NULL;
	stat = NULL;
}
