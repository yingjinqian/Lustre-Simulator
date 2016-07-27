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
#ifndef OBD_H
#define OBD_H

#include <node.h>
#include <vector>
#include <nrsrbtree.h>

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/

#define IO_QUEUE_INITED 1
#define IO_QUEUE_QUEUED 2
#define IO_QUEUE_WORK   4
#define IO_QUEUE_RBTREE 8

struct io_queue {
	int nr;
	int flags;
	int quantum;
	NrsRbtree rs;
	Scheduler *s;
};

class OBD : public Device
{
protected:
	Node *site;
	OBD *_up;
	OBD *_down;

public:
	OBD();
	~OBD();
	
	virtual void Attach(Node *s) { site = s; }
	virtual void Setup() {};
	virtual void Cleanup() {};
	virtual int Connect() { return 0; }
	virtual int Disconnect() { return 0; }
};

class DataDevice : public OBD
{
public:
	DataDevice();
    virtual ~DataDevice();

	virtual int Read(IO *io) = 0;
	virtual int Write(IO *io) = 0;
    virtual int Create(Object *obj) { UNUSED(obj); return 0; }
    virtual int Destroy(Object *obj) { UNUSED(obj); return 0; }
};

typedef vector<DataDevice *> dtvec_t;

class MetadataDevice : public OBD
{
public:
    MetadataDevice();
    virtual ~MetadataDevice();

	virtual int Open(Object *obj) = 0;
	//virtual int Close(Object *obj) { return 0; }
};

#endif
