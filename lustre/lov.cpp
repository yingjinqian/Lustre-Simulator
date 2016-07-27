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
#include "lov.h"
#include "ost.h"
#include "osc.h"

LOV::LOV()
 : DataDevice()
{
	num = 0;
	memset(&layout, 0, sizeof(layout));
}


LOV::~LOV()
{}

obd_count LOV::StripeOffset(Layout *lyt, obd_count off)
{
	obd_count ssize = lyt->stripe_size;
	obd_count swidth = ssize * lyt->stripe_count;
	obd_count stripe_off;

	//assert(lyt->StripeAlign(off));
	stripe_off = do_div(off, swidth);
	return off * ssize + stripe_off;
}

int LOV::ReadWrite(IO *io)
{
	FileObject *obj = (FileObject *) io->obj;
	Layout *lyt = obj->GetLayout();
	obd_count off, left;
	int idx, rc = 0;

	//assert(lyt->StripeAlign(io->off) && lyt->StripeAlign(io->off + io->count));
	assert(lyt->Check(io->count));
	idx = (io->off / lyt->stripe_size) % lyt->stripe_count;
	off = io->off;
	left = io->count;

	while (left > 0) {
		DataDevice *dt;
		ChunkObject *sobj;
		IO *sio;

		sobj = (ChunkObject *)&obj->cobj[idx];
		sio = new IO;
		io->Refer(sio);
		sio->count = min(left, lyt->stripe_size);
		sio->parent = io;
		sio->data = NULL;
		sio->obj = sobj;
		sio->off = StripeOffset(lyt, off);
		sio->ref = 0;
		sio->waiter = NULL;
		left -= sio->count;
		off += sio->count;
		
		dt = lov[sobj->ost_idx];
		if (io->cmd == READ)
			rc = dt->Read(sio);
		else
			rc = dt->Write(sio);

		if (rc)
			return rc;

		idx = (idx + 1) % lyt->stripe_count;
	}

	return rc;
}

int LOV::Read(IO *io)
{
	return ReadWrite(io);
}

int LOV::Write(IO *io)
{
	return ReadWrite(io);
}

int LOV::Create(Object *obj)
{
	FileObject *file = (FileObject *)obj;
	int start, i, nid;
	
	assert(params.io.StripeCount <= num);

	nid = site->GetNid();
	if (!(file->flags & FILE_HAS_LAYOUT))
		file->SetLayout(&layout);

	file->cobj = new ChunkObject [file->StripeCount()];
	if (params.io.Mode == ACM_FPP) {
		file->SetId(nid);
	} else {
		file->SetId(0);
	}

	//FIXME: FFP and shared access mode.
	//start = nid % file->StripeCount();
	start = nid % num;
	for (i = 0; i < file->StripeCount(); i++) {
		DataDevice *dt;

		assert(start >= 0 && start < num);
		dt = lov[start];
		dt->Create(&file->cobj[i]);
		start = (start + 1) % num;
	}
	
	return 0;
}

int LOV::Destroy(Object *obj)
{
	FileObject *file = (FileObject *) obj;

	//delete file;

	return 0;
}

void LOV::Attach(Node *s)
{
	DataDevice *dt;

	site = s;
	num = OST::GetCount();
	assert(num == params.cluster.OstCount);
	for (int i = 0; i < num; i++) {
		dt = new OSC(i, OST::GetNid(i));
		lov.push_back(dt);
		dt->Attach(s);
	}	
}

void LOV::Cleanup()
{
	for (int i = 0; i < num; i++) {
		lov[i]->Cleanup();
		delete lov[i];
	}
}

int LOV::Disconnect()
{
	for (int i = 0; i < num; i++)
		lov[i]->Disconnect();
	return 0;
}

void LOV::Setup()
{
	sprintf(name, "LOV-%s", site->GetDeviceName());
	assert(num > 0);
	for(int i = 0; i < num; i++) {
		lov[i]->Setup();
	}

	layout.stripe_pattern = params.io.StripePattern;
	layout.stripe_size = params.io.StripeSize ? : 1048576;
	layout.stripe_count = (params.io.StripeCount ? : (num - 1)) ? : 1;
}
