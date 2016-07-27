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
#ifndef OST_H
#define OST_H

#include <server.h>
#include <osd.h>

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/
class OST;
typedef vector<OST *> ostvec_t;
class OST : public Server
{
	int id;
	static int num;
	static ostvec_t set;

	OSD osd;
	
	/* Statistics */
	Stat *srpc;
	Stat *swr;
	Stat *srd;
	Stat *sen; /*entry */
	Stat *sex; /* exit */

	cfs_time_t stime;

	obd_count norpc;
	obd_count nowr;
	obd_count nord;

	enum {
		ost_WriteFirstState,
		ost_WriteStartState,
		ost_WriteDiskIOState,
		ost_WriteCompletionState,
		ost_WriteLastState,
	};
	enum {
		ost_ReadFirstState,
		ost_ReadFileIOState,
		ost_ReadSendPutState,
		ost_ReadRecvPutAckState,
		ost_ReadCompletionState,
		ost_ReadLastState,
	};
	enum {
		ost_PingStartState,
		ost_PingCompletionState,
		ost_PingLastState,
	};

	void PingStateMachine(ThreadLocalData *tld);
	void WriteStateMachine(ThreadLocalData *tld);
	void ReadStateMachine(ThreadLocalData *tld);

	void InitStat();
	void StatNrs(Message *msg, int delta);
	void Enqueue(Message *msg);
	//void SetScheduler(int algo);

	static void Handle(void *arg);
public:
	OST();
	~OST();

	static int GetCount();
	static int GetNid(int i);

	obd_count GetQueuedRPCNum();
	io_queue *GetIoQueue(obd_id fid, int rw);
	void Start();
};

#endif
