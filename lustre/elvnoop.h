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
#ifndef ELVNOOP_H
#define ELVNOOP_H

#include <elevator.h>

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/

/* No-op elevator algorithm */
class ElvNoop : public Elevator
{
	List	queue;
	
public:
  ElvNoop();

  ~ElvNoop();
	//int Merge(request **rq, ioreq *req);
	int Dispatch(int force);
	void AddRequest(request *rq);
	int QueueEmpty();
	request *ElvFormerRequest(request *rq);
	request *ElvLatterRequest(request *rq);
	void ElvMergeRequests(request *rq, request *next);
};

#endif
