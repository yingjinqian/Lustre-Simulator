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
#ifndef NRSFRR_H
#define NRSFRR_H

#include <nrsfifo.h>
#include <nrsrbtree.h>
#include <server.h>

/**
	@author yingjin.qian <yingjin.qian@sun.com>
	File Object based Round robin algorithm
	Performance improvement;
	fairness and responsitity;
*/

class NrsFRR : public Scheduler
{
	Server *srv;
	int qnr;
	NrsRbtree rbQ;
	NrsFifo mQ; /* Mandatory deadline, the indicated service time are same. */
	NrsFifo dQ; /* dynamical deadline */

	/* FRR Global queue */
	NrsFifo Q;
	io_queue *wq;

	NrsFifo *Q1;
	NrsFifo *Q2;

	/*
	 * Tally information.
	 */
	static float coef;
	static cfs_duration_t min_st; /* queued time */
	static cfs_duration_t delta;
	static obd_count nr_expired;
	static obd_count nr_dyn_expired;
	static obd_count nr_mdr_expired;

	obd_count bw;
	obd_count num_serviced;
	obd_count num_queued;
	obd_count io_queued;
	obd_count io_serviced;
	#define TIME_SLOT 10
	cfs_time_t iotime[TIME_SLOT];
	static int CompareByOffset(void *a, void *b);
	static int CompareByDeadline(void *a, void *b);

	void IoEnqueue(io_queue *q, Message *msg);
	Message *IoDequeue(io_queue *q);
	void AddIoQueue(io_queue *q);
	io_queue *GetWorkQueue();
	io_queue *GetIoQueue(obd_id fid, int rw);
	cfs_time_t GetExpireTime(IO *io);
	void IoDeadlineEnqueue(Message *msg);
	void SetIoDeadline(Message *msg);
	void RemoveFromDeadlineQueue(Message *msg);
	Message *IoDeadlineDequeue();
	Message *Dequeue2QDeadline(Scheduler *s1, Scheduler *s2, int tc);
	Message *DequeueA(Scheduler *s, Message *msg);
	cfs_time_t GetDynamicMaxDeadline();

public:
	NrsFRR();
	NrsFRR(Server *s);
	~NrsFRR();

	int Enqueue(void *e);
	void *Dequeue();
	void Erase(void *e);
	int Requeue(void *e);
	void Finish(void *e);

	static obd_count GetExpiredNum();
	static obd_count GetMandatoryExpiredNum();
	static obd_count GetDynamicalExpiredNum();
};

#endif
