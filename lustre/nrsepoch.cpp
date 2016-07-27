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
#include "nrsepoch.h"
#include "nrsfifo.h"
#include "message.h"

#define QUEUE_NUM 2

NrsEpoch::NrsEpoch()
 : Scheduler()
{
	num = params.cluster.ClientSet;
	cur = 0;
	Init(0, NULL);
}


NrsEpoch::~NrsEpoch()
{}

void NrsEpoch::StartTimer()
{
	if (!on) {
		cfs_duration_t used;
		assert(size > 0);
		used = Event::Clock() - qvec[cur]->start;
		if (used > qvec[cur]->quota)
			SwitchQueue();
		else {
			timer.ModTimer(Event::Clock() + qvec[cur]->quota - used);
		}
	}
}

void NrsEpoch::SwitchQueue()
{
	timer.DelTimer();
	cur = (cur + 1) % num;
	Print(NOW "Switch to queue@%d.........................\n",
		Event::Clock(), cur);
	nrs = qvec[cur]->q;
	qvec[cur]->start = Event::Clock();
	if (size > 0) {
		timer.ModTimer(Event::Clock() + qvec[cur]->quota);
		on = 1;
	}
}

void NrsEpoch::TimeSliceExpire(void *arg)
{
	NrsEpoch *ep = (NrsEpoch *) arg;

	ep->SwitchQueue();
}

/* Two client classes:
 * In each epoch (10 sec),
 * class A uses 7 sec
 * class B uses 3 sec
 */

void NrsEpoch::Init(int algo, compare_f c)
{
	int i;
	queue_class *qc;
	cfs_duration_t ts[num];

	ts[0] = 700000000ULL;
	ts[1] = 300000000ULL;
	//ts[2] = 2000000000ULL;

	for (i = 0; i < num; i++) {
		qc = new queue_class;
		
		qc->c = i;
		qc->quota = ts[i];
		qc->start = Event::Clock();
		qc->q = new NrsFifo;
		qvec.push_back(qc);
	}

	nrs = qvec[cur]->q;
	timer.SetupTimer(TimeSliceExpire, this);
	on = 0;
}



int NrsEpoch::GetQueueClass(void *e)
{
	Message *msg = (Message *) e;

	return msg->cid / (params.cluster.ClientCount / params.cluster.ClientSet) % num;
}

int NrsEpoch::Enqueue(void *e)
{
	int c;
	
	c = GetQueueClass(e);
	assert(c < num);
	qvec[c]->q->Enqueue(e);
	
	size++;
	if (size == 1)
		StartTimer();
	return 0;
}

void *NrsEpoch::Dequeue()
{
	void *e = NULL;
	int idx;

	if (size == 0)
		return NULL;

	e = nrs->Dequeue();
	if (e != NULL)
		goto out;

	/* choose one request from other queues
	 * if the current one in use is empty.
	 */
	idx = (cur + 1) % num;
	while (idx != cur) {
		e = qvec[idx]->q->Dequeue();
		if (e != NULL)
			goto out;

		idx = (idx + 1) % num;
	}

out:
	size--;
	return e;
}

void NrsEpoch::Erase(void *e)
{
	int c;

	c = GetQueueClass(e);
	qvec[c]->q->Erase(e);
	size--;
}

int NrsEpoch::Requeue(void *e)
{
	int c;

	c = GetQueueClass(e);
	size++;
	return qvec[c]->q->Requeue(e);
}

