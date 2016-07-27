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
#ifndef MDT_H
#define MDT_H

#include <server.h>

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/
class MDT;
typedef vector<MDT *> mdtvec_t;
class MDT : public Server
{
	static int num;
	static mdtvec_t set;
	
	int id;	

	obd_count nrpc;
	Stat *srpc;
	
	enum {
		mdt_OpenFileState,
		mdt_OpenCompleteState,
	};

	void InitStat();
	void StatRpc(Message *msg, int i);
	void OpenStateMachine(ThreadLocalData *tld);
	static void Handle(void *arg);
public:
	MDT();
	~MDT();

	static int GetCount();
	static int GetNid(int i);
	void Start();
};

#endif
