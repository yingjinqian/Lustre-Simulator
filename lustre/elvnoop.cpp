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
#include "elvnoop.h"

ElvNoop::ElvNoop()
    : Elevator()
{}


ElvNoop::~ElvNoop()
{}

int ElvNoop::Dispatch(int force)
{
	request *rq;
	
	if (queue.Empty())
		return 0;
	
	rq = (request *)queue.suc;
	rq->Remove();
	ElvDispatchSort(rq);
	return 1;
}

void ElvNoop::AddRequest(request *rq)
{
	rq->Insert(&queue);
}

int ElvNoop::QueueEmpty()
{
	return queue.Empty();
}

request *ElvNoop::ElvFormerRequest(request *rq)
{
	if (rq->pred == &queue)
		return NULL;
	return (request *)rq->pred;
}

request *ElvNoop::ElvLatterRequest(request *rq)
{
	if (rq->suc == &queue)
		return NULL;
	return (request *)rq->suc;
}

void ElvNoop::ElvMergeRequests(request *rq, request *next)
{
	next->Remove();
	Elevator::ElvMergeRequests(rq, next);
}

