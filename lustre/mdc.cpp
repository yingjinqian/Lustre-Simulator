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
#include "mdc.h"
#include "mdt.h"

MDC::MDC()
 : MetadataDevice()
{
	id = tnid = 0;
	__dbg = params.debug.MDC;
}

MDC::MDC(int i, int tid)
 : MetadataDevice()
{
	id = i;
	tnid = tid;
	__dbg = params.debug.MDC;
}

MDC::~MDC()
{}

int MDC::Open(Object *obj)
{
	Message *msg = new Message;
	ThreadLocalData *tld = new ThreadLocalData;
	Thread *t = new Thread;

	msg->Init(MSG_OPEN, nid, tnid);
	msg->req = obj;
	memset(tld, 0, sizeof(*tld));
	tld->m = msg;
	tld->n = site;
	tld->t = t;
	t->CreateThread(site->GetHandler(), tld);
	Print(NOW "%s submits OPEN request FID@%llu to %s\n",
		now, name,  obj->GetId(), NetDevice::GetNodeName(tnid));
	t->Run();
	return 0;	
}

void MDC::Setup()
{
	sprintf(name, "MDC%d@%s", id, site->GetDeviceName());
	if (!params.cluster.CMD)
		tnid = MDT::GetNid(0);
	nid = site->GetNid();
}
