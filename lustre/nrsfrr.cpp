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

#include "nrsfrr.h"
#include "nrsrbtree.h"

#define FRR_QUANTUM 8
#define FIX_TIME 2000000ULL

#define FRR_DL_ONLY (params.nrs.Deadline == NRS_ALGO_DL_ONLY)
#define FRR_DL_OFF (params.nrs.Deadline == NRS_ALGO_DL_OFF)
#define FRR_DL_2L (params.nrs.Deadline == NRS_ALGO_DL_2L)

#define FRR_ABSTRACT_NRS 0

float NrsFRR::coef = 1.5;
cfs_time_t NrsFRR::min_st = 500000000ULL; /* 500 ms */
cfs_time_t NrsFRR::delta = 0;//params.nrs.DDLD; /* 5 ms */
obd_count NrsFRR::nr_expired;
obd_count NrsFRR::nr_dyn_expired;
obd_count NrsFRR::nr_mdr_expired;

#define DT(t)	((t) * params.disk.TimeUnit)

NrsFRR::NrsFRR()
 : Scheduler()
{
	qnr = 0;
	wq = NULL;
	num_serviced = num_queued = 0;
	io_serviced = io_queued = 0;
	bw = params.disk.ReadBandwidth * 1 / 2;
	rbQ.Init(CompareByDeadline, Message::GetKey);
}

NrsFRR::NrsFRR(Server *s)
 : Scheduler()
{
	srv = s;
	qnr = 0;
	wq = NULL;
	num_serviced = num_queued = 0;
	io_serviced = io_queued = 0;
	bw = params.disk.ReadBandwidth / 2;
	rbQ.Init(CompareByDeadline, Message::GetKey);
	if (FRR_DL_ONLY) {
		Q1 = new NrsFifo;
		Q2 = new NrsFifo;
	}
}

NrsFRR::~NrsFRR()
{
	if (FRR_DL_ONLY) {
		delete Q1;
		delete Q2;
	}
	LASSERT(size == 0 && mQ.Empty() && dQ.Empty() && Q.Empty());
}

int NrsFRR::CompareByOffset(void *a, void *b)
{
	IO *o1, *o2;

	o1 = (IO *)((Message *)a)->req;
	o2 = (IO *)((Message *)b)->req;

	LASSERT(o1->fid == o2->fid);
	BUG_ON(o1->off == o2->off);
	return (o1->off > o2->off) ? 1: -1;
}

int NrsFRR::CompareByDeadline(void *a, void *b)
{
	IO *o1, *o2;

	o1 = (IO *)((Message *)a)->req;
	o2 = (IO *)((Message *)b)->req;

	return (o1->deadline > o2->deadline) ? 1 : -1;
}

void NrsFRR::IoEnqueue(io_queue *q, Message *msg)
{
	q->nr++;

#ifdef FRR_ABSTRACT_NRS
	q->s->Enqueue(msg);
#else
	q->rs.Enqueue(msg);
#endif
}

Message *NrsFRR::IoDequeue(io_queue *q)
{
	q->nr--;
	LASSERTF(q->nr >= 0,
		 NOW"wq@%p, nr %d size %llu empty: %d\n", 
		 Event::Clock(), q, q->nr, size, q->rs.Empty());

#ifdef FRR_ABSTRACT_NRS
	return (Message *)(q->s->Dequeue());
#else
	return (Message *)(q->rs.Dequeue());
#endif
}

void NrsFRR::AddIoQueue(io_queue *q)
{
}

io_queue *NrsFRR::GetWorkQueue()
{
	if (wq)
		return wq;

	Print(NOW"------------ new wq ------------\n", Event::Clock());
	wq = (io_queue *)Q.Dequeue();
	if (wq == NULL)
		return NULL;

	wq->flags &= ~IO_QUEUE_QUEUED;
	wq->flags |= IO_QUEUE_WORK;
	wq->quantum = FRR_QUANTUM;
	return wq;
}

io_queue *NrsFRR::GetIoQueue(obd_id fid, int cmd)
{
	return (srv->GetIoQueue(fid, cmd));
}

cfs_time_t NrsFRR::GetDynamicMaxDeadline()
{
	Message *msg;
	IO *io;

	if (dQ.Empty())
		return 0;

	msg = (Message *)(dQ.Last());
	LASSERT(msg != NULL);
	io = (IO *) msg->req;
	LASSERT(io != NULL);
	return ((IO *)(msg->req))->deadline;
}

cfs_time_t NrsFRR::GetExpireTime(IO *io)
{
	return (io->flags & IO_FLG_DYNAMIC_DEADLINE) ? (cfs_time_t)((io_queued + io_serviced) / bw * coef) : 0;
}

void NrsFRR::SetIoDeadline(Message *msg)
{
	IO *io = (IO *)msg->req;
	cfs_time_t current;

	LASSERT(FRR_DL_2L && (io->flags & IO_FLG_DEADLINE));

	current = Event::Clock();
	if (io->flags & IO_FLG_DYNAMIC_DEADLINE) {
		cfs_time_t max_dl;

		LASSERT(io->deadline == 0);
		io->deadline = (cfs_time_t) (DT((io_queued + io_serviced) / bw) * coef) + current;

		max_dl = GetDynamicMaxDeadline();
		if (max_dl > io->deadline) {
			io->deadline = max_dl + delta;
		}
		dQ.Enqueue(msg);
	} else {
		LASSERT(io->deadline != 0 && io->deadline == params.nrs.MDLV);
		io->deadline += current;
		mQ.Enqueue(msg);
	}
}

void NrsFRR::IoDeadlineEnqueue(Message *msg)
{
	if (FRR_DL_OFF)
		return;

	if (FRR_DL_ONLY) {
		IO *io = (IO *)msg->req;

		if (io->flags & IO_FLG_DEADLINE_1)
			Q1->Enqueue(msg);
		else {
			LASSERT(io->flags & IO_FLG_DEADLINE_2);
			Q2->Enqueue(msg);
		}
		return;
	}

	SetIoDeadline(msg);

}

Message *NrsFRR::DequeueA(Scheduler *s, Message *msg)
{
	IO *io = (IO *) msg->req;

	LASSERT(s->Dequeue() == msg);
	nr_expired++;
	if (io->flags & IO_FLG_MANDATORY_DEADLINE)
		nr_mdr_expired++;
	else if (io->flags & IO_FLG_DYNAMIC_DEADLINE)
		nr_dyn_expired++;

	/*Print*/printf(NOW"msg@%p fid %llu deadline %llu elapsed, num expired %llu:d%llu:m%llu.\n", 
		Event::Clock(), msg, io->fid, io->deadline, nr_expired, nr_dyn_expired, nr_mdr_expired);
	return msg;
}

Message *NrsFRR::Dequeue2QDeadline(Scheduler *s1, Scheduler *s2, int tc)
{
	Message *m1, *m2;
	IO *o1, *o2;
	cfs_time_t current;

	m1 = (Message *) s1->First();
	m2 = (Message *) s2->First();
	current = Event::Clock();

	if (m1 == NULL) {
		if (!tc || m2 == NULL)
			return m2;

		/*
		 * Check whether the deadline of the request is elapsed.
		 */
		o2 = (IO *) m2->req;
		LASSERT(o2->flags & IO_FLG_DEADLINE);
		if (o2->deadline <= current)
			return DequeueA(s2, m2);
	} else {
		if (m2 == NULL) {
			if (!tc)
				return DequeueA(s1, m1);

			o1 = (IO *)m1->req;
			if (o1->deadline <= current)
				return DequeueA(s1, m1);
		}  else {
			o1 = (IO *)m1->req;
			o2 = (IO *)m2->req;

			if (o1->deadline > o2->deadline) {
				if (!tc || (tc && o2->deadline <= current))
					return DequeueA(s2, m2);
			} else if (!tc || (tc && o2->deadline <= current)) 
				return DequeueA(s1, m1);
		}
	}

	return NULL;
}

Message *NrsFRR::IoDeadlineDequeue()
{
	IO *o1 = NULL, *o2 = NULL;
	Message *m1, *m2;
	cfs_time_t current;

	if (FRR_DL_OFF)
		return NULL;

	/*
	 * with flags FRR_DL_ONLY, the requests are only sorted 
	 * according to the deadline indicated by clients.
	 */
	if (FRR_DL_ONLY)
		return Dequeue2QDeadline(Q1, Q2, 0);

	return Dequeue2QDeadline(&dQ, &mQ, 1);
}

void NrsFRR::RemoveFromDeadlineQueue(Message *msg)
{
	IO *io = (IO *)msg->req;

	if (!FRR_DL_2L)
		return;

	if (io->flags & IO_FLG_MANDATORY_DEADLINE) {
		mQ.Erase(msg);
	} else {
		LASSERT(io->flags & IO_FLG_DYNAMIC_DEADLINE);
		dQ.Erase(msg);
	}
}

int NrsFRR::Enqueue(void *e)
{
	Message *msg = (Message *)e;
	IO *io = (IO *)msg->req;
	io_queue *q;

	if (FRR_DL_ONLY)
		goto __deadline;

	Print(NOW"Get IO queue fid@%llu.\n", Event::Clock(), io->fid);
	q = srv->GetIoQueue(io->fid, io->cmd);

	if (!(q->flags & IO_QUEUE_INITED)) {
	#ifdef FRR_ABSTRACT_NRS
		q->s = new NrsRbtree(CompareByOffset);
	#else
		q->rs.SetCompareFunc(CompareByOffset);
	#endif
		q->flags |= IO_QUEUE_INITED;
	}

	IoEnqueue(q, msg);

	if (!(q->flags & IO_QUEUE_QUEUED) && q != wq) {
		Q.Enqueue(q);
		q->flags |= IO_QUEUE_QUEUED;
	}

	/*
	 * Update I/O tally information.
	 */
__deadline:
	num_queued++;
	io_queued += io->count;
	IoDeadlineEnqueue(msg);

	Print(NOW"Enqueue: msg@%p queue@%p fid@%llu cmd@%d off@%llu, deadline@%llu.\n",
		Event::Clock(), msg, q, io->fid, io->cmd, io->off, io->deadline);

	size++;
	return 0;
}

int NrsFRR::Requeue(void *e)
{
	LASSERTF(0, "Not Impelmented!\n");
	return 0;
}

void NrsFRR::Erase(void *e)
{
	LASSERTF(0,"Not Implemented!\n");
	return;
}

void *NrsFRR::Dequeue()
{
	Message *msg;
	IO *io;

	/* check Deadline first */
	msg = IoDeadlineDequeue();
	if (FRR_DL_ONLY)
		return msg;

	/*
	 * The request is expired.
	 */
	if (msg != NULL) {
		io_queue *q;
		
		io = (IO *)msg->req;
		//Print(NOW"Get IO queue fid@%llu.\n", Event::Clock(), io->fid);
		q = srv->GetIoQueue(io->fid, io->cmd);
		LASSERT(q->nr > 0);
	#ifdef FRR_ABSTRACT_NRS
		q->s->Erase(msg);
	#else
		q->rs.Erase(msg);
	#endif
		q->nr--;
		if (q->nr == 0) {
			if (wq == q)
				wq = NULL;
			if (q->flags & IO_QUEUE_QUEUED)
				Q.Erase(q);
		}
		goto __stat;
	}

	if ((wq = GetWorkQueue()) == NULL)
		return NULL;

	msg = IoDequeue(wq);
	LASSERTF(msg != NULL,
		 NOW"wq@%p nr@%d\n", Event::Clock(), wq, wq->nr);
	io = (IO *)msg->req;

	Print(NOW"Dequeue: wq@%p nr@%d fid@%llu cmd@%d off@%llu.\n",
		Event::Clock(), wq, wq->nr, io->fid, io->cmd, io->off);

	/*
	 * Quantum is measured by number of reqeusts serived per round. 
	 */
	wq->quantum--;
	if (wq->nr == 0) {
		Print(NOW"---------- nr == 0 ---------\n", Event::Clock());
		wq = NULL;
	} else if (wq->quantum == 0) {
		Print(NOW"---------- quantum == 0 ----------\n", Event::Clock());
		Q.Enqueue(wq);
		wq->flags &= ~IO_QUEUE_WORK;
		wq->flags |= IO_QUEUE_QUEUED;
		wq = NULL;
	}

	/*
	 * remove the I/O request from the deadline queue.
	 */
	RemoveFromDeadlineQueue(msg);

__stat:
	num_queued--;
	num_serviced++;
	io_queued -= io->count;
	io_serviced += io->count;

	LASSERT(num_queued >= 0 && io_queued >= 0);

	size--;
	if (size == 0 && FRR_DL_2L) {
		LASSERT(mQ.Empty());
		LASSERT(dQ.Empty());
		LASSERT(num_queued == 0);
	}
	return msg;
}

void NrsFRR::Finish(void *e)
{
	Message *msg = (Message *)e;
	IO *io = (IO *)msg->req;

	Print(NOW"Finish I/O request %p.\n", Event::Clock(), msg);
	num_serviced--;
	io_serviced -= io->count;

	LASSERT(io_serviced >= 0);
}

obd_count NrsFRR::GetExpiredNum()
{
	return nr_expired;
}

obd_count NrsFRR::GetMandatoryExpiredNum()
{
	return  nr_mdr_expired;
}

obd_count NrsFRR::GetDynamicalExpiredNum()
{
	return nr_dyn_expired;
}
