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
#include "elevator.h"

#define ELV_HASH_ENTRIES (1 << params.disk.ElvHashShift)
#define ELV_HASH_FN(off) (hash_long(off, params.disk.ElvHashShift))
#define ELV_ON_HASH(rq) (!hlist_unhashed(&(rq)->hash))
#define rq_hash_key(rq) ((rq)->off + (rq)->count)
//#define rq_end_pos(rq) ((rq)->off + (rq)->count)

#define REQ_RW 0
#define REQ_SORTED 1
#define REQ_SOFTBARRIER 2
#define	REQ_HARDBARRIER 4
#define REQ_STARTED 8
#define REQ_NOMERGE 16

Elevator::Elevator()
    : Device()
{
	inflight = sorted = 0;
	count[READ] = count[WRITE] = 0;
	straved[READ] = straved[WRITE] = 0;
	elvpriv = 0;
	sorted = 0;
	last = boundary = NULL;
	endpos = 0;
	totBytes = 0;
	__dbg = params.debug.Elv;
	maxsize = params.disk.ElvMaxReqSize;
	unplugThreshold = params.disk.ElvUnplugThreshold;
	Init();
}

Elevator::~Elevator()
{
	free(hash);
}

int Elevator::Init()
{
	hash = (hlist_head *) malloc(sizeof(*hash) * ELV_HASH_ENTRIES);
	if (hash == NULL)
		return -ENOMEM;
	for (int i = 0; i < ELV_HASH_ENTRIES; i++)
		INIT_HLIST_HEAD(&hash[i]);
	return 0;
}

void Elevator::Attach(Disk *d)
{
	disk = d;
}

obd_count Elevator::GetPendingNum()
{
	return ((obd_count)count[READ] + (obd_count)count[WRITE] - inflight);
}

request *Elevator::elv_rqhash_find(obd_off off)
{
    hlist_head *hash_list = &hash[ELV_HASH_FN(off)];
	hlist_node *entry, *next;
	request *rq;

	hlist_for_each_entry_safe(rq, entry, next, hash_list, hash) {
		assert(rq && ELV_ON_HASH(rq));

		if (rq_hash_key(rq) == off)
			return rq;
	}
	return NULL;
}

void Elevator::elv_rqhash_del(request *rq)
{
	if (ELV_ON_HASH(rq))
		hlist_del_init(&rq->hash);
}

void Elevator::elv_rqhash_add(request *rq)
{
	assert(!ELV_ON_HASH(rq));
	hlist_add_head(&rq->hash, &hash[ELV_HASH_FN(rq_hash_key(rq))]);
}

void Elevator::elv_rqhash_reposition(request *rq)
{
	hlist_del_init(&rq->hash);
	elv_rqhash_add(rq);
}

/*
 * Rbtree support functions for inserting/lookup/removalof requests
 * in sorted RB tree.
 */
request *Elevator::elv_rb_add(Rbtree *tree, request *rq)
{
	rb_node **p = &tree->root.rb_node;
	rb_node *parent = NULL;
	request *__rq;

	while (*p) {
		parent = *p;
		__rq = rb_entry(parent, struct request, rbnode);

		if (rq->off < __rq->off)
			p = &(*p)->rb_left;
		else if (rq->off > __rq->off)
			p = &(*p)->rb_right;
		else
			return __rq;
	}

	tree->rb_link_node(&rq->rbnode, parent, p);
	tree->rb_insert_color(&rq->rbnode);
	return NULL;
}

void Elevator::elv_rb_del(Rbtree *tree, request *rq)
{
	assert(!RB_EMPTY_NODE(&rq->rbnode));
	tree->rb_erase(&rq->rbnode);
	tree->rb_clear_node(&rq->rbnode);
}

request *Elevator::elv_rb_find(Rbtree *tree, obd_off off)
{
	rb_node *n = tree->root.rb_node;
	request *rq;

	while (n) {
		rq = rb_entry(n, struct request, rbnode);

		if (off < rq->off)
			n = n->rb_left;
		else if (off > rq->off)
			n = n->rb_right;
		else
			return rq;
	}

	return NULL;
}

void Elevator::elv_dispatch_add_tail(request *rq)
{
	if (last == rq)
		last = NULL;

	elv_rqhash_del(rq);
	sorted--;
	endpos = rq_end_pos(rq);
	boundary = rq;
	rq->Insert(&Queue);
}

int Elevator::ll_front_merge_fn(request *rq, ioreq *req)
{
	if (rq->count + req->count > maxsize) {
		if (rq == last)
			last = NULL;
		return 0;
	}
	return 1;
}

int Elevator::ll_back_merge_fn(request *rq, ioreq *req)
{
	if (rq->count + req->count > maxsize) {
		if (rq == last)
			last = NULL;
		return 0;
	}
	return 1;
}

int Elevator::ElvMergeOK(request *rq, ioreq *req)
{
	if (rq->cmd != req->cmd)
		return 0;
	
	/*Other kind checks */
	return 1;
}

int Elevator::ElvMerge(request **rq, ioreq *req)
{
	request *__rq;
	int rc = ELEVATOR_NO_MERGE;
	
	if (last) {
		if (last->off + last->count == req->off)
			rc = ELEVATOR_BACK_MERGE;
		else if (last->off - req->count == req->off)
			rc = ELEVATOR_FRONT_MERGE;
		
		if (rc != ELEVATOR_NO_MERGE) {
			*rq = last;
			return rc;
		}
	}
	__rq = elv_rqhash_find(req->off);
	if (__rq  && ElvMergeOK(__rq, req)) {
		*rq = __rq;
		return ELEVATOR_BACK_MERGE;
	}
	
	return rc;
}

int Elevator::ElvQueueEmpty()
{
	if (!Queue.Empty())
		return 0;
	
	return QueueEmpty();
}

void Elevator::ElvInsertRequest(request *rq, int where)
{
	switch (where) {
	case ELEVATOR_INSERT_FRONT:
		rq->flags |= REQ_SOFTBARRIER;
		rq->Insert(&Queue);
		break;
	case ELEVATOR_INSERT_BACK:
		rq->flags |= REQ_SOFTBARRIER;
		rq->InsertTail(&Queue);
		/*TODO: drain the queue. */
		break;
	case ELEVATOR_INSERT_SORT:
		rq->flags |= REQ_SORTED;
		elv_rqhash_add(rq);
		if (!last)
			last = rq;
			
		AddRequest(rq);
		break;
	case ELEVATOR_INSERT_REQUEUE:
		break;
	}
	
/*
	printf(NOW "Add new request %lu %lu %llu %llu\n", 
		now, count[0], count[1], inflight, GetPendingNum());
*/
	if (disk->IsPlug() && GetPendingNum() >= unplugThreshold) {
		Print(NOW "%llu exceed the max number of outstanding requests\n", 
			now, GetPendingNum());
		disk->Unplug();
	}
}

void Elevator::ElvAddRequest(request *rq)
{
	ElvInsertRequest(rq, ELEVATOR_INSERT_SORT);	
}

void Elevator::ElvDispatchSort(request *rq)
{
	List *pos;
	request *__rq;
	
	if (last = rq)
		last = NULL;
	
	elv_rqhash_del(rq);
	sorted--;
	for (pos = Queue.pred; pos != &Queue; pos = pos->pred) {
		__rq = (request *)pos;
		if (rq->cmd != __rq->cmd)
			break;
		
		if (__rq->flags & (REQ_SOFTBARRIER | REQ_HARDBARRIER | REQ_STARTED))
			break;
		
		if (rq->off >= endpos) {
			if (__rq->off < endpos)
				continue;
		} else {
			if (__rq->off >= endpos)
				break;	
		}
	}
	
	rq->Insert(pos);
}

void Elevator::PutRequest(request *rq)
{
	inflight--;
	count[rq->cmd]--;
	assert(count[rq->cmd] >= 0);
}

int Elevator::RequestCompletion(request *rq)
{
	Elevator *elv = (Elevator *)rq->data;

	elv->PutRequest(rq);
	delete rq;
	return 0;
}

void Elevator::SpliceIoreq(List *h1, List *h2)
{
	List *t1, *t2, *t3;

	assert(!h1->Empty() && !h2->Empty());

	t1 = h2->suc;
	t2 = h2->pred;
	h2->Remove();
	h1->pred->suc = t1;
	t1->pred = h1->pred;
	h1->pred = t2;
	t2->suc = h1;
	
	
}

void Elevator::ElvMergeRequests(request *rq, request *next)
{
	elv_rqhash_reposition(rq);
	elv_rqhash_del(next);
	sorted--;
	last = rq;
}

int Elevator::AttemptMerge(request *rq, request *next)
{
	/*
	 * not contiguous
	 */
	if (rq->off + rq->count != next->off)
		return 0;
	
	if (rq->cmd != next->cmd)
		return 0;

	if (rq->count + next->count > maxsize)
		return 0;

	/* If we are allowed to merge, then append the ioreq list. */
	rq->count += next->count;
	SpliceIoreq(&rq->batch, &next->batch);
	ElvMergeRequests(rq, next);

	count[next->cmd]--;
	delete next;
	return 1;
}

int Elevator::AttemptBackMerge(request *rq)
{
	request *next = ElvLatterRequest(rq);

	if (next)
		return AttemptMerge(rq, next);

	return 0;
}

int Elevator::AttemptFrontMerge(request *rq)
{
	request *prev = ElvFormerRequest(rq);

	if (prev)
		return AttemptMerge(prev, rq);

	return 0;
}

void Elevator::ElvMergedRequest(request *rq, int type)
{
	if (type == ELEVATOR_BACK_MERGE)
		elv_rqhash_reposition(rq);

	last = rq;
}

void Elevator::Enqueue(ioreq *req)
{
	request *rq;
	int rc, sync = 0;
	
	rc = ElvMerge(&rq, req);
	switch (rc) {
	case ELEVATOR_BACK_MERGE:
		if (!ll_back_merge_fn(rq, req))
			break;
		req->Insert(&rq->batch);
		Print(NOW "Elv Back Merge ioreq %llu:%llu -> request %llu:%llu.\n",
			now, req->off, req->count, rq->off, rq->count);
		rq->count += req->count;
		
		/* Merge with the latter request. */
		if (!AttemptBackMerge(rq))
			ElvMergedRequest(rq, rc);
		goto out;
	case ELEVATOR_FRONT_MERGE:
		if (!ll_front_merge_fn(rq, req))
			break;
		Print(NOW "Elv Front Merge ioreq %llu:%llu <- request %llu:%llu.\n",
			now, req->off, req->count, rq->off, rq->count);
		req->InsertTail(&rq->batch);
		//req->Insert(&rq->batch);
		rq->count += req->count;
		rq->off = req->off;
		
		/* Merge with the fromer request.*/
		if (!AttemptFrontMerge(rq))
			ElvMergedRequest(rq, rc);
		goto out;
	}
	
	rq = new request;
	count[req->cmd]++;
	rq->cmd = req->cmd;
	rq->off = req->off;
	rq->count = req->count;
	rq->flags = 0;
	//rq->startTime = now;
	rq->priority = 0;
	INIT_HLIST_NODE(&rq->hash);
	//RB_CLEAR_NODE(&rq->rbnode);
	rq->data = this;
	rq->completion = RequestCompletion;
	req->Insert(&rq->batch);
	
	if (ElvQueueEmpty())
		disk->Plug();
	
	ElvAddRequest(rq);
	
out:
	if (sync)
		disk->Unplug();
}

request *Elevator::ElvNextRequest()
{
	request *rq;

	while (1) {
		while (!Queue.Empty()) {
			rq = (request *)(Queue.suc);
			rq->Remove();
			return rq;
		}
		
		if (!Dispatch(0))
			return NULL;
	}
}

request *Elevator::Dequeue()
{
	request *rq;

	while ((rq = ElvNextRequest()) != NULL) {
		if (!(rq->flags & REQ_STARTED)) {
			rq->flags |= REQ_STARTED;
		}
		
		if (!boundary || boundary == rq) {
			endpos = rq_end_pos(rq);
			boundary = NULL;
		}
		
		/*TODO: Other lower layer prepares the request. */
		break;
	}
	if (rq)
		inflight++;
	return rq;
}

