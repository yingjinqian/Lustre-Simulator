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
#include "cluster.h"
#include "network.h"
#include "client.h"
#include "ost.h"
#include "mdt.h"

Cluster::Cluster()
 : Device()
{
	__dbg = 1;
}

Cluster::Cluster(const char *s)
	: Device()
{
	__dbg = 1;
	strncpy(name, s, MAX_NAME_LEN);
}

Cluster::~Cluster()
{}

void Cluster::Start()
{
	int i, setnr, cnr;
	OST *ost;
	MDT *mdt;
	Client *cl;
	setctl *ctl;

	Print(NOW "%s is Starting...\n", now, name);

	Network::Setup();

	RPC::InitRPCTimeout("Timeout");
	//Client::InitDeadline();
	//Client::InitPriority();
	Client::InitClient();

	/*
	 * setup the OSSes.
	 */
	ost = new OST [params.cluster.OstCount];
	for (i = 0; i < params.cluster.OstCount; i++) {
		ost[i].Start();
	}

	/*
	 * setup MDS
	 */
	if (params.cluster.MDT) {
		mdt = new MDT;
		mdt->Start();
	}

	/*
	 * setup clients.
	 * Now we only support the cluster with two client classes.
	 * (experimental)
	 */
	setnr = params.cluster.ClientSet;

	if (setnr > 1)
		assert(params.nrs.algo == NRS_ALGO_EPOCH);

	ctl = new setctl[setnr];
	memset(ctl, 0, setnr * sizeof(*ctl));
	for (i = 0; i < setnr; i++) {
		ctl[i].c = i;
		ctl[i].sotime = START_TIME;
		ctl[i].swtime = START_TIME;
		ctl[i].srtime = START_TIME;
		ctl[i].sctime = START_TIME;
		ctl[i].writetime = 0;
		ctl[i].readtime = 0;
		ctl[i].opentime = 0;
		ctl[i].closetime = 0;
		sprintf(name, "client.set%d", i);
		ctl[i].stat = new Stat(name);
	}

	if (params.io.Mode == ACM_SHARE) {
		assert(setnr == 1);
		ctl->fid = 0;
        ctl->fsz = params.io.AgvFileSizeSF; /* 32G */
		ctl->wtnr = params.io.WriterCount;
		ctl->rdnr = params.io.ReaderCount;
		cnr = max(ctl->wtnr, ctl->rdnr);
	} else {
		ctl->wtnr = ctl->rdnr = cnr = params.cluster.ClientCount;
	}

	cl = new Client [cnr];
	for (i = 0; i < cnr; i++) {
		int idx;

		idx = cl[i].GetId() / (cnr / params.cluster.ClientSet) % setnr;
		cl[i].Init(&ctl[idx]);
		cl[i].Start();
	}

	/*
	 * run the simulator.
	 */
	Event::SetRunTicks(params.MaxRunTicks);
	Event::Schedule();

	/* 
	 * Record the aggregate bandwidth.
	 */
	Client::RecordAggregateBandwidth();
	//Client::FiniDeadline();
	//Client::FiniPriority();
	Client::FiniClient();

	/*
	 * cleanup the clients.
	 */
	for (int i = 0; i < setnr; i++) {
		delete ctl[i].stat;
	}
	
	delete [] cl;

	if (params.cluster.MDT) {
		delete mdt;
	}

	delete [] ost;
	delete [] ctl;

	RPC::FiniRPCTimeout();
	Network::Cleanup();
}
