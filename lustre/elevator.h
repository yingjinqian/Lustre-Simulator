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
#ifndef ELEVATOR_H
#define ELEVATOR_H

#include <event.h>
#include <disk.h>

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/

class Elevator : public Device 
{
	hlist_head *hash;
	Disk *disk;
	
protected:

#define ELEVATOR_NO_MERGE 0
#define ELEVATOR_FRONT_MERGE 1
#define ELEVATOR_BACK_MERGE 2

#define ELEVATOR_INSERT_FRONT 1
#define ELEVATOR_INSERT_BACK 2
#define ELEVATOR_INSERT_SORT 3
#define ELEVATOR_INSERT_REQUEUE 4

#define rq_end_pos(rq) ((rq)->off + (rq)->off)
#define rb_entry_rq(node) rb_entry((node), struct request, rbnode)
#define rq_fifo_time(rq) ((rq)->time)
#define rq_set_fifo_time(rq, exp) ((rq)->time = exp)
#define rq_fifo_clear(rq) do { \
	rq->Remove(); \
	rq_set_fifo_time(rq, 0); \
	} while (0)

	char name[MAX_NAME_LEN];
	List Queue;
	int count[2];
	int straved[2];
	int elvpriv;
	obd_count totBytes;
	obd_count sorted;
	obd_count inflight;
	obd_count unplugThreshold;
	obd_count maxsize;
	obd_off endpos;
	request *boundary;
	request *last;
	
	request *elv_rqhash_find(obd_off off);
	void elv_rqhash_del(request *rq);
	void elv_rqhash_add(request *rq);
	void elv_rqhash_reposition(request *rq);

	request *elv_rb_add(Rbtree *tree, request *rq);
	request *elv_rb_find(Rbtree *tree, obd_off off);
	void elv_rb_del(Rbtree *tree, request *rq);
	
	void elv_dispatch_add_tail(request *rq);

	//int ElvMerge(request **rq, ioreq *req);
	int ElvMergeOK(request *rq, ioreq *req);
	int ElvQueueEmpty();
	void ElvAddRequest(request *rq);
	void ElvInsertRequest(request *rq, int where);
	void ElvDispatchSort(request *rq);
	request *ElvNextRequest();
	//void ElvMergedRequest(request *rq, int ret);
	//void ElvMergeRequests(request *rq, request *next);
	void SpliceIoreq(List *h1, List *h2);
	int AttemptBackMerge(request *rq);
	int AttemptFrontMerge(request *rq);
	int AttemptMerge(request *rq, request *next);

	/* ll_back/front_merge_fn */
	int ll_front_merge_fn(request *rq, ioreq *req);
	int ll_back_merge_fn(request *rq, ioreq *req);

	static int RequestCompletion(request *rq);

	virtual int ElvMerge(request **rq, ioreq *req);
	virtual int Dispatch(int force) = 0;
	virtual void AddRequest(request *rq) = 0;
	virtual int QueueEmpty() = 0;
	virtual request *ElvFormerRequest(request *rq) = 0;
	virtual request *ElvLatterRequest(request *rq) = 0;
	virtual void ElvMergeRequests(request *rq, request *next);
	virtual void ElvMergedRequest(request *rq, int type);

public:
	Elevator();
	~Elevator();

	int Init();
	void Attach(Disk *disk);
	void PutRequest(request *rq);
	void Enqueue(ioreq *req);
	request *Dequeue();
	obd_count GetPendingNum();
};

#endif
