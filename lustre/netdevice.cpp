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
#include "netdevice.h"

uint32_t NetDevice::num;
vector<NetDevice *> NetDevice::ndvec;

NetDevice::NetDevice()
 : Device()
{
	target = NULL;
	nid = num++;
	ndvec.push_back(this);
}


NetDevice::~NetDevice()
{}

void NetDevice::Recv(Packet *pkt)
{
	delete pkt;
}

void NetDevice::Send(Packet *pkt)
{
	if (target)
		target->Recv(pkt);
}

void NetDevice::UpdateState(int nid, int state, Packet *pkt)
{
	NetDevice *nd;

	nd = GetNetDevice(nid);
	nd->Notify(state, pkt);
}

cfs_duration_t NetDevice::TxTime(obd_count size)
{
	return (size / params.network.Bandwidth /*+ SignRand(size / params.network.Bandwidth / 10)*/);
}

cfs_duration_t NetDevice::NetLatency()
{
	return (params.network.NetLatency + SignRand(params.network.NetLatency / 10));
}

cfs_duration_t NetDevice::InterruptLatency()
{
	return (params.network.InterruptLatency + SignRand(params.network.InterruptLatency / 10));
}

uint32_t NetDevice::GetNid()
{
	return nid;
}

NetDevice *NetDevice::GetNetDevice(int nid)
{
	assert (nid > 0 &&  nid < num);
	return ndvec[nid];
}

char *NetDevice::GetNodeName(int nid)
{
	assert (nid > 0 &&  nid < num);
	return ndvec[nid]->name;
}
