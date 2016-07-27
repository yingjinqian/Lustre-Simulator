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
#ifndef NETWORK_H
#define NETWORK_H

#include <netdevice.h>
#include <nrsfifo.h>
/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/
class Network : public NetDevice
{
	obd_count inflight;
	obd_count capacity;
	obd_count nodes;
	NrsFifo blkQ;
	static Network *instance;
	static Stat *stat;
public:
 	Network();

  ~Network();

	static void TxDone(void *arg);
	static void NetTraversed(void *arg);
	
	void Recv(Packet *pkt);
	void Notify(int state, Packet *pkt);
	void Connect();
	void Start();
	static void Setup();
	static void Cleanup();
	static Network * Instance() {
		return instance;
	}
};

#endif
