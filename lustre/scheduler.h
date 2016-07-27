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
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <lustre.h>

class Processor;
/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/
#define UNUSED(x) (void)x
class Scheduler
{
protected:
	int __dbg:1;
    int algo;
	obd_count	size;
    bool throttling;

	void Print(const char *fmt...);
public:
	Scheduler();
    virtual ~Scheduler();

	virtual int Enqueue(void *e) = 0;
	virtual void *Dequeue() = 0;
    virtual void Erase(void *e) { UNUSED(e); return; };
    virtual int Requeue(void *e) { UNUSED(e); return 0; };
	virtual void *First() { return NULL; }
    virtual void *Last() { return NULL; }
    virtual void Finish(void *e) { UNUSED(e); }
    virtual void Attach(Processor *p) { UNUSED(p);return; };

	obd_count Size();
    bool Empty();
    bool Throttling();

    int GetAlgo() { return algo; };
    static const char *NrsAlgoName(int algo);
};

#endif
