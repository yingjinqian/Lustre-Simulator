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
#include "timer.h"

Timer::Timer()
    : Thread()
{}


Timer::~Timer()
{}

void Timer::SetupTimer(fun_t fn, void *data)
{
	CreateThread(fn, data);
}

void Timer::DelTimer()
{
	if (run)
		RemoveFromRunQ();
}

void Timer::ModTimer(cfs_time_t expire)
{
	if (run) {
		if (expire == runTime)
			return;
		RemoveFromRunQ();
	}
	RunAt(expire);
}

cfs_time_t Timer::GetExpireTime()
{
	return runTime;
}
