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
#ifndef ELVDEADLINE_H
#define ELVDEADLINE_H

#include <elevator.h>

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/

/* Deadline I/O scheduler */
class ElvDeadline : public Elevator
{
	Rbtree sort_list[2];
	List fifo_list[2];
	request *next_rq[2];
	int fifo_expire[2];
	unsigned int batching; /* number of sequential requests made */
	unsigned int starved; /* times reads have starved writes */
	int front_merges;
	int writes_starved;
	int fifo_batch;
	obd_off headpos;

	void deadline_del_rq_rb(request *rq);
	void deadline_add_rq_rb(request *rq);
	void deadline_move_request(request *rq);
	void deadline_move_to_dispatch(request *rq);
	void deadline_remove_request(request *rq);
	int deadline_check_fifo(int rw);

public:
	ElvDeadline();
	~ElvDeadline();

	int ElvMerge(request **rq, ioreq *req);
	int Dispatch(int force);
	void AddRequest(request *rq);
	int QueueEmpty();
	request *ElvFormerRequest(request *rq);
	request *ElvLatterRequest(request *rq);
	void ElvMergeRequests(request *rq, request *next);
	void ElvMergedRequest(request *rq, int type);
};

#endif
