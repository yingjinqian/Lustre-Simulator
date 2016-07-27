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
#include "nrsfifo.h"

NrsFifo::NrsFifo()
 : Scheduler()
{}


NrsFifo::~NrsFifo()
{}

int NrsFifo::Enqueue(void *e)
{
	size++;
	Q.push_front(e);
	return 0;
}

int NrsFifo::Requeue(void *e)
{
	size++;
	Q.push_back(e);
	return 0;
}

void NrsFifo::Erase(void *e)
{
	size--;
	Q.remove(e);
}

void *NrsFifo::Dequeue()
{
	void *e;
	
	if (Q.empty())
		return NULL;
	
	e = Q.back();
	Q.pop_back();
	
	size--;
	return e;
}

void *NrsFifo::First()
{
	void *e;

	if (Q.empty())
		return NULL;

	e = Q.back();
	return e;
}

void *NrsFifo::Last()
{
	void *e;

	if (Q.empty())
		return NULL;

	e = Q.front();
	return e;
}
