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
#ifndef NRSPRIO_H
#define NRSPRIO_H

#include <scheduler.h>
#include <nrsfifo.h>
#include <server.h>

/**
	Quantum based priority Scheduling with deadline.
	@author yingjin.qian <yingjin.qian@sun.com>
*/
class NrsPrio : public Scheduler
{
	static int flavor;
	static float coef;
	static cfs_duration_t delta;

	obd_count bw;
	obd_count num_serviced;
	obd_count num_queued;
	obd_count io_queued;
	obd_count io_serviced;

#define PRIO_FLG_DYNAMIC_WT 1
#define PRIO_FLG_QUANTUM 2

	struct prio_queue {
		int flags;
		int prio;
		int quantum;
		NrsFifo pq;
		cfs_duration_t wt;

		int Empty() { return pq.Empty(); }
		Message *First() { return (Message *)pq.First(); }
		Message *Dequeue() { return (Message *)pq.Dequeue(); }
		void Enqueue(void *e) { pq.Enqueue(e); }
	};

#define PRIO_QUEUE_NUM 2
	prio_queue Q[PRIO_QUEUE_NUM];
	prio_queue *wq;
	int idx;

	void InitPrioQueues();
	cfs_duration_t GetDynamicWaitime();
	cfs_time_t GetDynamicMaxDeadline();
	cfs_time_t GetDynamicDeadline();

	Message *DeadlineCheckDequeue();
	Message *GetMessageFromPQ(prio_queue *q);
	Message *StrictPrioDequeue();
	Message *QuantumPrioDequeue();
	Message *DLPrioDequeue();
	Message *DQPSDequeue();

public:
	NrsPrio();
	~NrsPrio();
	int Enqueue(void *e);
	void *Dequeue();
	void Finish(void *e);
    //void Erase(void *e) { return; }
    //int Requeue(void *e){ return 0; }

	static char *GetFlavor();
};

#endif
