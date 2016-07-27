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
#include "raid0device.h"
#include "disk.h"

int Raid0Device::num;

#define do_div(a,b)                     \
        ({                              \
                unsigned long remainder;\
                remainder = (a) % (b);  \
                (a) = (a) / (b);        \
                (remainder);            \
        })

Raid0Device::Raid0Device()
 : BlockDevice()
{
	id = num++;
	sprintf(name, "md-raid0@%d", id);
	__dbg = params.debug.Raid0;
	devcnt = params.disk.raid0.DiskCount;
	chunk = params.disk.raid0.ChunkSize;
}


Raid0Device::~Raid0Device()
{
	for (int i = 0; i < set.size(); i++) {
		delete set[i];
	}
}

obd_count Raid0Device::AllocBlock(obd_count size)
{
	obd_off off = curoff;
	
	curoff += size;
	return off;
}

int Raid0Device::Completion(ioreq *req)
{
	ioreq *p = req->next;

	//printf(NOW "raid finished the request %llu:%llu.\n",
	//	now, p->off, p->count);
	if (--p->ref == 0 && p->completion) {
		p->completion(p);
	}
	delete req;
	return 0;
}

int Raid0Device::StripeNumber(obd_off offset)
{
	unsigned long swidth;
	obd_off stripe_off;

	swidth = devcnt * chunk;
	/* do_div(a, b) returns a % b, and a = a / b */
	stripe_off = do_div(offset, swidth);

	return (stripe_off / chunk);
}

#define OBD_OBJECT_EOF 0xffffffffffffffffULL
int Raid0Device::StripeOffset(int stripeno, obd_off roff, obd_off *doff)
{
	unsigned long swidth, stripe_off, this_stripe;
	int ret = 0;

	swidth = chunk * devcnt;

	/* do_div(a, b) returns a % b, and a = a / b */
	stripe_off = do_div(roff, swidth);

	this_stripe = stripeno * chunk;
	if (stripe_off < this_stripe) {
		stripe_off = 0;
		ret = -1;
	} else {
		stripe_off -= this_stripe;
		if (stripe_off >= chunk) {
			stripe_off = chunk;
			ret = 1;
		}
	}
	*doff = roff * chunk + stripe_off;
	return ret;
}

int Raid0Device::StripeIntersect(int stripeno, obd_off start, obd_off count, obd_off *dstart, obd_off *dcount)
{
	int start_side, end_side;

	assert(start != OBD_OBJECT_EOF && count != OBD_OBJECT_EOF);
	start_side = StripeOffset(stripeno, start, dstart);
	end_side = StripeOffset(stripeno, start + count, dcount);
	
	if (start_side != 0 && end_side != 0 && *dstart == *dcount) {
		return 0;
	}

	*dcount -= *dstart;

	return 1;
}

int Raid0Device::SubmitIoreq(ioreq *req)
{
	obd_count left;
	ioreq *sub;
	int stripe, rc;

	left = req->count;
	stripe = StripeNumber(req->off);

	Print(NOW "%s handles ioreq: %llu:%llu.\n",
			now, name, req->off, req->count);
	while (left > 0) {
		assert(req->ref == 0);
		req->ref++;
		sub = new ioreq;
		sub->io	= NULL;
		sub->cmd = req->cmd;
		
		rc = StripeIntersect(stripe, req->off, req->count, &sub->off, &sub->count);
		assert(rc == 1);

		sub->ref = 0;
		sub->next = req;
		sub->completion = Completion;

		Print(NOW "%s submit sub ioreq: stripe@%d %llu:%llu.\n",
			now, name, stripe, sub->off, sub->count);

		set[stripe]->SubmitIoreq(sub);

		stripe = (stripe + 1) % devcnt;
		left -= sub->count;
	}

	return 0;
}

void Raid0Device::SetDevCount(int cnt)
{
	devcnt = cnt;
}

void Raid0Device::SetStripeSize(obd_count sz)
{
	chunk = sz;
}

void Raid0Device::Start()
{
	BlockDevice *blk;

	Print(NOW"%s is starting...\n", now, name);
	
	for (int i = 0; i < devcnt; i++) {
		blk = new Disk;
		set.push_back(blk);	
		set[i]->Start();
	}
}


void Raid0Device::SelfTest()
{
#define NUM 5
	Raid0Device raid;
	ioreq req[NUM];
	int i;

	printf(NOW "Raid0 Device Driver Test.\n", now);
	
	for (i = 0; i < NUM; i++) {
		req[i].cmd = WRITE;
		req[i].next = NULL;
		req[i].completion = NULL;
		req[i].ref = 0;
		req[i].io = NULL;
	}

	req[0].off = 1048576 * 0;
	req[1].off = 1048576 * 9;
	req[2].off = 1048576 * 27;
	req[3].off = 1048576 * 81;
	req[4].off = 1048576 * 256;
	//req[5].off = 1048576 * 0;
	//req[0].off = 1048576 * 0;
	//req[0].off = 1048576 * 0;
	//req[0].off = 1048576 * 0;
	//req[0].off = 1048576 * 0;
	//req[0].off = 1048576 * 0;

	req[0].count = 1048576 * 2;
	req[1].count = 1048576 * 3;
	req[2].count = 1048576 * 5;
	req[3].count = 1048576 * 7;
	req[4].count = 1048576 * 13;
	//req[0].count = 1048576 * 18;
	
	raid.SetDevCount(2);
	raid.SetStripeSize(1048576 * 4);
	raid.Start();
	
	for (i = 0; i < NUM; i++)
		raid.SubmitIoreq(&req[i]);

	Event::Schedule();
}
