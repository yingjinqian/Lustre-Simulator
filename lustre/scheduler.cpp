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
#include "scheduler.h"

Scheduler::Scheduler()
{
	size = 0;
	__dbg = params.debug.Nrs;
    throttling = false;
}


Scheduler::~Scheduler()
{}

void Scheduler::Print(const char *fmt...)
{
	if (__dbg) {
		va_list args;
	
		va_start(args,fmt);
		vfprintf(stdout,fmt, args);
		va_end(args);
	}
}

obd_count Scheduler::Size()
{
	return size;
}

bool Scheduler::Empty()
{
	return size == 0;
}

bool Scheduler::Throttling()
{
    return throttling;
}

//#define NRS_ALGO_FIFO 1
//#define NRS_ALGO_BINHEAP 2
//#define NRS_ALGO_RBTREE 3
//#define NRS_ALGO_EPOCH 4
//#define NRS_ALGO_FRR 5
//#define NRS_ALGO_BYOBJID 6
//#define NRS_ALGO_BYDEADLINE 7
//#define NRS_ALGO_BYKEY 8
//#define NRS_ALGO_FCFS 9
//#define NRS_ALGO_TBF    10

const char *Scheduler::NrsAlgoName(int algo)
{
	switch (algo) {
	case NRS_ALGO_FCFS:
	case NRS_ALGO_FIFO:
		return "FCFS";
	case NRS_ALGO_FRR:
		return "File Object Round Robin";
	case NRS_ALGO_BINHEAP:
		return "BINHEAP";
	case NRS_ALGO_RBTREE:
		return "RBTREE";
	case NRS_ALGO_EPOCH:
		return "EPOCH";
	case NRS_ALGO_BYOBJID:
		return "Greedy by Object ID";
	case NRS_ALGO_BYDEADLINE:
		return "By RPC Deadline";
	case NRS_ALGO_BYKEY:
		return "BY Key";
    case NRS_ALGO_TBF:
        return "TBF";
	default:
		return "UNKNOWN";
	}
}
