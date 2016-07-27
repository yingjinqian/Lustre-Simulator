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
#include "elvdeadline.h"

#define RQ_RB_ROOT(rq)	(&sort_list[rq->cmd])

ElvDeadline::ElvDeadline()
 : Elevator()
{
	headpos = 0;
	/* TODO: set reasonable read/write expire duration */
	fifo_expire[READ] = 0;
	fifo_expire[WRITE] = 0;
	writes_starved = 2;
	/* number of sequential requests treated as one */
	fifo_batch = 16;
	batching = 0;
	starved = 0;
	front_merges = 1;
}


ElvDeadline::~ElvDeadline()
{}

void ElvDeadline::deadline_add_rq_rb(request *rq)
{
	Rbtree *tree = RQ_RB_ROOT(rq);
	request *__alias;

retry:
	__alias = elv_rb_add(tree, rq);
	if (__alias) {
		/* TODO: move to dispatch queue */
		assert(0);
		deadline_move_request(__alias);
		goto retry;
	}
}

void ElvDeadline::deadline_del_rq_rb(request *rq)
{
	if (next_rq[rq->cmd] == rq) {
		rb_node *next = Rbtree::rb_next(&rq->rbnode);

		next_rq[rq->cmd] = NULL;
		if (next)
			next_rq[rq->cmd] = rb_entry_rq(next);
	}

	elv_rb_del(RQ_RB_ROOT(rq), rq);
}

void ElvDeadline::deadline_remove_request(request *rq)
{
	rq_fifo_clear(rq);
	deadline_del_rq_rb(rq);
}

void ElvDeadline::deadline_move_request(request *rq)
{
	int rw = rq->cmd;
	rb_node *next = Rbtree::rb_next(&rq->rbnode);

	next_rq[READ] = NULL;
	next_rq[WRITE] = NULL;
	
	if (next)
		next_rq[rw] = rb_entry_rq(next);

	headpos = rq->off + rq->count;

	/*
	 * take it off the sort and fifo list, 
	 * move to dispatch queue.
	 */
	deadline_move_to_dispatch(rq);
}

void ElvDeadline::deadline_move_to_dispatch(request *rq)
{
	deadline_remove_request(rq);
	elv_dispatch_add_tail(rq);
}

int ElvDeadline::deadline_check_fifo(int rw)
{
	request *rq = (request *) fifo_list[rw].suc;

	if (Event::Clock() >= rq_fifo_time(rq))
		return 1;

	return 0;
}

int ElvDeadline::ElvMerge(request **rq, ioreq *req)
{
	request *__rq;
	int rc;

	rc = Elevator::ElvMerge(rq, req);
	if (rc != ELEVATOR_NO_MERGE)
		return rc;

	if (front_merges) {
		obd_off off = req->off + req->count;

		__rq = elv_rb_find(&sort_list[req->cmd], off);
		if (__rq) {
			assert(off == __rq->off);
			
			if (ElvMergeOK(__rq, req)) {
				rc = ELEVATOR_FRONT_MERGE;
				goto out;
			}
		}
	}

	return ELEVATOR_NO_MERGE;
out:
	*rq = __rq;
	return 0;
}

int ElvDeadline::Dispatch(int force)
{
	int reads = !fifo_list[READ].Empty();
	int writes = !fifo_list[WRITE].Empty();
	request *rq;
	int rw;

	/* Not consider the writes starved the reads.*/
	if (writes)
		rw = WRITE;
	else if (reads)
		rw = READ;
	else
		return 0;

	goto dispatch_simple;

	/*
	 * batches are currently reads XOR writes
	 */
	if (next_rq[WRITE])
		rq = next_rq[WRITE];
	else
		rq = next_rq[READ];
	
	if (rq) {
			/* we have a "next request" */
			if (headpos != rq->off)
				/* end the batch on a non sequential request */
				batching += fifo_batch;
			if (batching < fifo_batch)
				goto dispatch_request;
	}

	/*
	 * at this point we are not running a batch, select the appropriate
	 * data direction (read/write)
	 */
	if (reads) {
		if (writes && (starved++ >= writes_starved))
			goto dispatch_writes;

		rw = READ;
		goto dispatch_find_request;
	}
	
	/*
	 * there are either no reads or writes have been starved
	 */
	if (writes) {
dispatch_writes:
		starved = 0;
		rw = WRITE;
		goto dispatch_find_request;
	}

	return 0;

dispatch_find_request:
	/*
	 * we are not running a batch, find best request for selected rw
	 */
	if (deadline_check_fifo(rw)) {
		/* an expired request exists - satisfy it */
		batching = 0;
		rq = (request *) fifo_list[rw].suc;
	} else if (next_rq[rw]) {
		/*
		 * The last req was the same dir and we have a next request in
		 * sort order. No expired requests so continue on from here.
		 */
		rq = next_rq[rw];
	} else {
dispatch_simple:
		rb_node *node;
		/*
		 * The last req was the other direction or we have run out of
		 * higher-offed requests. Go back to the lowest off resquest 
		 * (1 way elevator) and start a new batch.
		 */
		batching = 0;
		node = sort_list[rw].rb_first();
		if (node)
			rq = rb_entry_rq(node);
	}

dispatch_request:
	/*
	 * rq is the selected appropriate requests.
	 */
	batching++;
	deadline_move_request(rq);
	
	return 1;
}

request *ElvDeadline::ElvFormerRequest(request *rq)
{
	rb_node *prev = Rbtree::rb_prev(&rq->rbnode);

	if (prev)
		return rb_entry_rq(prev);

	return NULL;
}

request *ElvDeadline::ElvLatterRequest(request *rq)
{
	rb_node *next = Rbtree::rb_next(&rq->rbnode);
	
	if (next)
		return rb_entry_rq(next);

	return NULL;
}

void ElvDeadline::ElvMergeRequests(request *rq, request *next)
{
	/* TODO: if next expires before rq, assign its expire time to 
	 * rq and mobe into next position (next will be deleted) in fifo
	 */
/*	if (!rq->Empty() && !next->Empty()) {
		assert(0);
	}*/
	deadline_remove_request(next);
	Elevator::ElvMergeRequests(rq, next);
	
}

void ElvDeadline::AddRequest(request *rq)
{
	deadline_add_rq_rb(rq);
	
	/*
	 * TODO:Set expire time (only used for reads) and add to fifo list
	 */
	rq_set_fifo_time(rq, Event::Clock() + fifo_expire[rq->cmd]);
	rq->Insert(&fifo_list[rq->cmd]);
}

int ElvDeadline::QueueEmpty()
{
	return fifo_list[READ].Empty() && fifo_list[WRITE].Empty();
}

void ElvDeadline::ElvMergedRequest(request *rq, int type)
{
	if (type == ELEVATOR_FRONT_MERGE) {
		elv_rb_del(RQ_RB_ROOT(rq), rq);
		deadline_add_rq_rb(rq);
	}

	Elevator::ElvMergedRequest(rq, type);
}
