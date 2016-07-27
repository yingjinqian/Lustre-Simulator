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
#ifndef DISK_H
#define DISK_H

#include <blockdevice.h>
#include <timer.h>
#include <hash.h>

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/

struct request;
typedef int (*end_req_t)(request *); 
/* Disk request */
struct request : public List
{
	int cmd;
	obd_off off;
	obd_count count;
	cfs_time_t time;
	int flags;
	int priority;
	int ref;
	hlist_node hash;
	rb_node rbnode;
	List batch;
	void *data;
	end_req_t completion;
};

class Elevator;
class Disk : public BlockDevice
{
	int id;
	int pluged;
	obd_off headpos;
	Elevator *elv;	

	int ready;
	List Queue; /*ready Disk I/O request */
	Timer timer;
	cfs_duration_t delay;
	request *curq; /* Current request in progress. */
	
#define MIN_REQ_SIZE  4096 /* 4k */
#define MAX_REQ_SIZE 134217728 /* 128M */
#define LOG_BASE 12
#define LOG_MAX 30
	unsigned int seeknr;
	unsigned int log[2][LOG_MAX];
	Stat *rqsz;
	Stat *perf;
	cfs_time_t lastTime;
	obd_count totBytes; /* bytes in time unit */
	
	static	int num;
	static int RequestCompletion(request *rq);
	static void DiskUnplugWork(void *data);
	static void Process(void *data);

	void SubmitRq(request *rq);
	void RecordRequestSize();
	void StatDisk(request *rq);
	void DiskStateMachine();
	void Request();
	request *GetNextRequest();
	void EndRequest(request *rq);
	ioreq *GetNextIoreq(request *rq);
	void InitElevator();
	enum {
		disk_IdleState,
		disk_WorkState,
		disk_ReqFinishedState,
		disk_LastState,
	} diskState;
	
	void InitStat();

	// benchmark
#define DISK_LATENCY 0
#define DISK_RANDPERF 1
#define DISK_SEQPERF 2
	struct dctl {
		int dt;
		int state;
		int factor;
		int repeat;
		int finished;
		obd_count rsz;
		obd_count maxsz;
		obd_count	totcnt;
		cfs_duration_t stime;
		request *rq;
		Disk *d;
		Thread *t;
		Stat *stat;
	};
	enum {
		disk_LatencyFirstState,
		disk_LatencyStartState,
		disk_LatencyLaunchState,
		disk_LatencyFinishState,
		disk_LatencyLastState,
	};
	static void LatencyStateMachine(void *arg);
	static void LaunchIO(dctl *ctl);
	static void RecordLatency(dctl *ctl);
	static void DiskLatencyTest();
	static void DiskPerfTest(int dt);
	static void DiskRandomTest();
	static void DiskSequentTest();

public:
	Disk();
	~Disk();

	virtual obd_off AllocBlock(obd_count size);
	virtual int SubmitIoreq(ioreq *req);

	void Plug();
	void Unplug();
	int RemovePlug();
	int IsPlug();
	void SetPlugDelay(cfs_duration_t d);
	void Start();

	static void SelfBenchmark();
};

#endif
