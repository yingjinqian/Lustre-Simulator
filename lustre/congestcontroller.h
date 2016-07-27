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
#ifndef CONGESTCONTROLLER_H
#define CONGESTCONTROLLER_H

#include <processor.h>
#include <nrsfifo.h>
#include "client.h"
#include "stat.h"


/**
	Client-side I/O congestion controller
	@author yingjin.qian <yingjin.qian@sun.com>
*/

struct CSP {
	int depth;
	int rcc;
	int uc;
	int state;
	int congested;
	NrsFifo Q;
};

class CongestController : public Processor
{
	static int depth_stat;

	int site;
	CSP csp;

	Client *cl;
	void StatRCC();

public:
	CongestController();

	~CongestController();

	void Attach(Node *site, int thnr);
	virtual int TaskCompletion(ThreadLocalData *tld);
	virtual void RunOneTask(Thread *t);
	int AddNewTask(Message *msg);
	Stat *GetDepthStat();
	void AddDepthStat();
	void CheckIoCongestion();
};

#endif
