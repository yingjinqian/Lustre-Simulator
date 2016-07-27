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
#ifndef OSC_H
#define OSC_H

#include <obd.h>
#include "processor.h"

/**
	Implement the distributed I/O congestion control algorithm in OSC layer.
	@author yingjin.qian <yingjin.qian@sun.com>
*/
class OSC : public DataDevice
{
	int id;   /* Corresponded Server ID */
	int tnid; /* Target NID */
	int nid;
	int cid; /* Client ID */
	int rpcs_in_flight;
	int r_in_flight;
	int w_in_flight;
	int r_in_queue;
	int w_in_queue;
	int max_rpcs_in_flight;

	Thread ping;

    NrsFifo rwQ;
    Processor *processor;
	int state;

	obd_import imp;

	obd_count counter;
	obd_count rr_idx; /* round-robin index */

	enum {
		osc_PingStartState,
		osc_PingSendState,
		osc_PingRecvReplyState,
		osc_PingIntervalSleepState,
		osc_PingLastState,
	};

	Message *pmsg;
	void PingStateMachine();
	static void PingDaemon(void *arg);

	void CheckRpcs();
	int RpcsInflight();
	int ReadWrite(IO *io);
	obd_count GetKey(Message *msg);
public:
	OSC();
	OSC(int id, int nid);
	~OSC();

	void Setup();
	void Cleanup();
	int Disconnect();
	int Read(IO *io);
	int Write(IO *io);
	int Create(Object *obj);
};

#endif
