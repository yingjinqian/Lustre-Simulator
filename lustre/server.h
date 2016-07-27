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
#ifndef SERVER_H
#define SERVER_H

#include <ptlrpc.h>
#include <obd.h>

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/

#define RPC_ENQUEUE	1
#define RPC_DEQUEUE	0
class Server : public Ptlrpc
{
protected:
	int algo;
	int thnr;
    Scheduler *nrs; // Network Request Scheduler
    Processor cpu;  // Thread Pool

	virtual void Enqueue(Message *msg);
    virtual void StatRpc(Message *msg, int in) { UNUSED(msg); UNUSED(in); return; }
	virtual obd_count GetQueuedRPCNum() { return nrs->Size(); }
	static int TaskCompare(void *a, void *b);
	static int CompareByObjid(void *a, void *b);
	static int CompareByDeadline(void *a, void *b);
public:
	Server();
    virtual ~Server();

	static void Handle(void *arg);
	virtual io_queue *GetIoQueue(obd_id fid, int rw);
	virtual void SetScheduler(int algo);
	virtual void Recv(Message *msg);
	virtual void Start();

};

#endif
