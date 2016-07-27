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
#include "nrsprio.h"

#define DT(t)	((t) * params.disk.TimeUnit)

#define PRIO_STRICT 1
#define PRIO_QUANTUM_ONLY 2
#define PRIO_DEADLINE_ONLY 4
#define PRIO_DQPS 8

#define QUANTUM_L0 1
#define QUANTUM_L1 10

int NrsPrio::flavor = /*PRIO_QUANTUM_ONLY;*/ PRIO_STRICT;
float NrsPrio::coef;
cfs_duration_t NrsPrio::delta;

#define IsPQEmpty(p) ((p->pq)->Empty())

NrsPrio::NrsPrio()
 : Scheduler()
{
	num_serviced = num_queued = 0;
	io_serviced = io_queued = 0;
	bw = params.disk.ReadBandwidth * 1 / 2;

	InitPrioQueues();
}


NrsPrio::~NrsPrio()
{
	for (int i = 0; i < PRIO_QUEUE_NUM; i++) {
		LASSERT(Q[i].Empty());
	}
}

void NrsPrio::InitPrioQueues()
{
	idx = PRIO_QUEUE_NUM - 1;
	wq = NULL;

	Q[0].prio = 0;
	Q[0].flags = PRIO_FLG_DYNAMIC_WT;
	Q[0].quantum = QUANTUM_L0;
	Q[0].wt = 0;

	Q[1].prio = 1;
	Q[1].flags = PRIO_FLG_QUANTUM;
	Q[1].quantum = QUANTUM_L1;
	Q[1].wt = 5000000000ULL;
}

cfs_time_t NrsPrio::GetDynamicMaxDeadline()
{
	NrsFifo *pq = &Q[0].pq;
	Message *msg;
	IO *io;

	if (pq->Empty())
		return 0;

	msg = (Message *)(pq->Last());
	LASSERT(msg != NULL);
	io = (IO *) msg->req;
	LASSERT(io != NULL);
	return ((IO *)(msg->req))->deadline;
}

cfs_duration_t NrsPrio::GetDynamicWaitime()
{
	return (cfs_time_t)((io_queued + io_serviced) / bw * coef);
}

cfs_time_t NrsPrio::GetDynamicDeadline()
{
	cfs_time_t max_dl, dl;

	dl = (cfs_time_t) (DT((io_queued + io_serviced) / bw) * coef) + Event::Clock();

	max_dl = GetDynamicMaxDeadline();
	if (max_dl > dl) {
		dl = max_dl + delta;
	}

	return dl;
}

int NrsPrio::Enqueue(void *e)
{
	Message *msg = (Message *)e;
	IO *io = (IO *)msg->req;
	prio_queue *q;

	Print(NOW"Enqueue a new I/O request %p [%llu-%llu]with priority %d\n",
		Event::Clock(), msg, io->off, io->count, io->prio);

	LASSERT(0 <= io->prio && io->prio < PRIO_QUEUE_NUM);
	q = &Q[io->prio];
	if (q->flags & PRIO_FLG_DYNAMIC_WT)
		io->deadline = GetDynamicDeadline();
	else
		io->deadline = Event::Clock() + q->wt;

	q->pq.Enqueue(msg);

	/* I/O stat */
	num_queued++;
	io_queued += io->count;
	size++;

	return 0;
}

Message *NrsPrio::GetMessageFromPQ(prio_queue *q)
{
	return q->Dequeue();
}

Message *NrsPrio::StrictPrioDequeue()
{
	for (int i = PRIO_QUEUE_NUM - 1; i >= 0; i--) {
		if (!Q[i].Empty())
			return GetMessageFromPQ(&Q[i]);
	}

	return NULL;
}

/**
 * Similar to WRR (Weighted Round Robin Scheudling algorithm)
 */
Message *NrsPrio::QuantumPrioDequeue()
{
	Message *msg = NULL;

repeat:
	if (wq == NULL) {
		wq = &Q[idx % PRIO_QUEUE_NUM];
	}

	if (wq != NULL) {
		if (wq->Empty()) {
			wq->quantum = wq->prio ? QUANTUM_L1 : QUANTUM_L0;
			idx++;
			goto repeat;
		}

		msg = (Message *)wq->Dequeue();
		wq->quantum--;
		if (wq->quantum == 0) {
			wq = NULL;
			idx = (idx + 1) % PRIO_QUEUE_NUM;
		}
	}

	LASSERT(msg != NULL);
	return msg;
}

Message *NrsPrio::DeadlineCheckDequeue()
{
	Message *msg;
	IO *io;

	for (int i = PRIO_QUEUE_NUM - 1; i >= 0; i--) {
		msg = Q[i].First();
		io = (IO *) msg->req;

		if (io->deadline >= Event::Clock())
			return msg;
	}

	return NULL;
}

Message *NrsPrio::DLPrioDequeue()
{
	Message *msg;

	msg = DeadlineCheckDequeue();
	if (msg != NULL)
		return msg;

	return StrictPrioDequeue();
}

Message *NrsPrio::DQPSDequeue()
{
	Message *msg;

	msg = DeadlineCheckDequeue();
	if (msg != NULL)
		return msg;

	msg = QuantumPrioDequeue();

	LASSERT(msg != NULL);
	return msg;
}

void *NrsPrio::Dequeue()
{
	Message *msg;
	IO *io;

	if (Empty())
		return NULL;

	switch (flavor) {
	case PRIO_STRICT:
		msg = StrictPrioDequeue();
		break;
	case PRIO_QUANTUM_ONLY:
		msg = QuantumPrioDequeue();
		break;
	case PRIO_DEADLINE_ONLY:
		msg = DLPrioDequeue();
		break;
	case PRIO_DQPS:
		msg = DQPSDequeue();
		break;
	}

	io = (IO *)msg->req;
	num_queued--;
	num_serviced++;
	io_queued -= io->count;
	io_serviced += io->count;

	LASSERT(num_queued >= 0 && io_queued >= 0);
	size--;

	Print(NOW"Dequeue a new I/O request with priority %d\n",
		Event::Clock(), io->prio);
	return msg;
}

void NrsPrio::Finish(void *e)
{
	Message *msg = (Message *)e;
	IO *io = (IO *)msg->req;

	Print(NOW"Finish I/O request %p.\n", Event::Clock(), msg);
	num_serviced--;
	io_serviced -= io->count;

	LASSERT(io_serviced >= 0);
}

char *NrsPrio::GetFlavor()
{
	switch (flavor) {
	case PRIO_STRICT:
		return "Strict priority scheduler";
	case PRIO_QUANTUM_ONLY:
		return "Quantum based priority scheduler";
	case PRIO_DEADLINE_ONLY:
		return "Priority scheduler only with deadline";
	case PRIO_DQPS:
		return "Quantum base priority scheduler with deadline";
	}
	return "NULL";
}
