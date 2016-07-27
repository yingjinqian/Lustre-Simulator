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
#ifndef BLOCKDEVICE_H
#define BLOCKDEVICE_H

#include <vector>
#include "device.h"
#include "server.h"
/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/

#define BLK_PLAIN 0
#define BLK_RAID0 1
#define BLK_RAID1 2

struct ioreq;
typedef int (*end_io_t)(ioreq *);
/* Request submitted from the upper layer. */
struct ioreq : public List
{
	int cmd;
	int ref;
	int flags;
	obd_off off;
	obd_count count;
	ioreq *next;
	IO *io;
	end_io_t completion;
};

class BlockDevice : public Device
{
protected:
	obd_off curoff;
	Server *site;
public:
	int type;
public:
	BlockDevice();

	virtual ~BlockDevice() = 0;
	
	virtual obd_off AllocBlock(obd_count size) = 0;
	virtual int SubmitIoreq(ioreq *req) = 0;
	virtual void Attach(Server *s) { site = s; }
	virtual void Start() = 0;
};

typedef vector<BlockDevice *> blkdevec_t;
#endif
