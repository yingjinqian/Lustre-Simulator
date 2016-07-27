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
#ifndef NRSEPOCH_H
#define NRSEPOCH_H

#include <scheduler.h>
#include <timer.h>
#include <vector>
/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/
/* Epoch algorithm based on time slice. */
class NrsEpoch : public Scheduler
{
	struct queue_class {
		int c;
		cfs_duration_t quota; /* Time slice */
		cfs_duration_t start;
		Scheduler *q;
	};

	typedef vector<queue_class *> nrs_qvec_t;
	nrs_qvec_t qvec;
	int num;
	int cur;
	int on; /* whether the timer is tirggered. */
	Timer timer;
	Scheduler *nrs; /* the queue current serviced */

	int GetQueueClass(void *e);
	void SwitchQueue();
	void StartTimer();
	static void TimeSliceExpire(void *arg);

public:
	NrsEpoch();
	~NrsEpoch();

	void Init(int algo, compare_f c);
	int Enqueue(void *e);
	void *Dequeue();
	void Erase(void *e);
	int Requeue(void *e);

};

#endif
