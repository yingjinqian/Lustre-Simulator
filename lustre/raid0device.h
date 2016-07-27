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
#ifndef RAID0DEVICE_H
#define RAID0DEVICE_H

#include <blockdevice.h>

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/

class Raid0Device : public BlockDevice
{
	static int num;

	int id;
	int devcnt;
	unsigned long chunk;
	blkdevec_t set;

	static int Completion(ioreq *req);
	int StripeNumber(obd_off offset);
	int StripeOffset(int stripeno, obd_off roff, obd_off *doff);
	int StripeIntersect(int stripeno, obd_off stat, obd_off end, obd_off *dstart, obd_off *dend);
public:
	Raid0Device();

	~Raid0Device();

	virtual obd_count AllocBlock(obd_count size);
	virtual int SubmitIoreq(ioreq *req);

	void SetDevCount(int cnt);
	void SetStripeSize(obd_count sz);
	void Start();

	static void SelfTest();
};

#endif
