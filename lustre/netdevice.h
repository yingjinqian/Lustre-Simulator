/***************************************************************************
 *   Copyright (C) 2008 by qian                                            *
 *                                                                         *
 *   Storage Team in NUDT                                                  *
 *   Yingjin Qian <yingjin.qian@sun.com>                                   *
 *                                                                         *
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
#ifndef NETDEVICE_H
#define NETDEVICE_H

#include <device.h>
#include <vector>
#include <message.h>
/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/
	
class NetDevice : public Device
{
	static uint32_t num;
	static vector<NetDevice *> ndvec;

protected:
	NetDevice *target;

	static void UpdateState(int nid, int state, Packet *pkt);
	static cfs_duration_t TxTime(obd_count size);
	static cfs_duration_t	NetLatency();
	static cfs_duration_t InterruptLatency();
public:
	int nid;

public:
	NetDevice();
	~NetDevice();

	virtual void Send(Packet *pkt);
	virtual void Recv(Packet *pkt);
	virtual void Notify(int state, Packet *pkt) = 0;
	virtual void Start() = 0;
	virtual void Connect() = 0;

	uint32_t GetNid();
	static NetDevice *GetNetDevice(int nid);
	static char *GetNodeName(int nid);
};

#endif
