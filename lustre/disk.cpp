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
#include "disk.h"
#include "elvnoop.h"
#include "elvdeadline.h"

#define DT(t)	((t) * params.disk.TimeUnit)

int Disk::num;

Disk::Disk()
    : BlockDevice()
{
	id = num++;
	pluged = 0;
	headpos = 0;
	ready = 0;
	delay = params.disk.UnplugDelay;
	__dbg = params.debug.Disk;
	diskState = disk_IdleState;
	curq = NULL;
	elv = NULL;
	lastTime = 0;
	seeknr = 0;
	totBytes = 0;

	for (int i = 0; i < 2; i++) 
		memset(log[i], 0, sizeof(unsigned int) * LOG_MAX);
}


Disk::~Disk()
{
	Unplug();

	if (params.stat.DiskReqSize && rqsz) {
		RecordRequestSize();
		delete rqsz;
	}
	if (params.stat.DiskBandwidth && perf)
		delete perf;

	while (curq = GetNextRequest()) {
		ioreq *req;
		List	*pos, *head = &curq->batch;

		while ((req = GetNextIoreq(curq)) != NULL)
			delete req;
		
		if (curq->completion)
			curq->completion(curq);
	}

	if (elv)
		delete elv;
}

void Disk::InitStat()
{
	char name[MAX_NAME_LEN];

	perf = rqsz = NULL;

	if (params.stat.DiskMaxStat && (id > params.stat.DiskMaxStat))
		return;

	if (params.stat.DiskReqSize) {
		sprintf(name, "Disk%d.rqsz", id);
		rqsz = new Stat(name);
	}

	if (params.stat.DiskBandwidth) {
		sprintf(name, "Disk%d.bw", id);
		perf = new Stat(name);
		perf->Record("# disk unplug delay: %llu.%llums\n"
			"fmt [time(sec), bw(MB/s), seek count, pending count]\n",
			delay / 1000000, delay % 1000000 / 100000);
	}
}

void Disk::StatDisk(request *rq)
{
	if (params.stat.DiskBandwidth && perf) {
		cfs_duration_t ticks;

		totBytes += rq->count;
		ticks = now - lastTime;
		if (ticks >= params.TimeUnit) {
			obd_count bw;

			bw = totBytes * params.TimeUnit / params.SizeUnit / ticks;
			perf->Record("%llu.%09llu	%llu	%lu	%llu\n", 
				now / params.TimeUnit, now % params.TimeUnit,
				bw, seeknr, elv->GetPendingNum());
			totBytes = 0;
			lastTime = now;
			seeknr = 0;
		}
	}

	if (params.stat.DiskReqSize && rqsz) {
		unsigned int val;
		
		for (val = 0; ((1 << val) < rq->count) && (val < MAX_REQ_SIZE); val++)
			;
		if (val < LOG_BASE)
			val = LOG_BASE;

		assert(val - LOG_BASE <= LOG_MAX);
		log[rq->cmd][val - LOG_BASE]++;
	}
}

#define pct(a,b) ((b) ? (((a) * 100) / b) : 0)
void Disk::RecordRequestSize()
{
	for (int i = 0; i < 2; i++) {
		int j, max = 0;
		unsigned int tot = 0;
		obd_count rsz = MIN_REQ_SIZE;

		for (j = 0; j < LOG_MAX; j++) {
			if (log[i][j] > 0) {
				max = j;
				tot += log[i][j];
			}
		}
		
		rqsz->Record("# %s request size stat (tot %lu):\n"
			"# fmt (rw, rsz[kb], count, pct%)\n",
			i == READ ? "READ" : "WRITE", tot);
		for (j = 0; j <= max; j++, rsz *= 2) {
			if (log[i][j] > 0)
				rqsz->Record("%d	%llu	%lu	%lu\n",
					i, rsz / 1024, log[i][j], pct(log[i][j], tot));
		}
	}
}

obd_off Disk::AllocBlock(obd_count size)
{
	obd_off off = curoff;
	
	curoff += size;
	return off;
}

int Disk::SubmitIoreq(ioreq *req)
{
	int rc = 0;
	
	elv->Enqueue(req);
	return rc;
}

void Disk::SubmitRq(request *rq)
{
	rq->Insert(&Queue);
	if (diskState == disk_IdleState) {
		Signal();
		diskState = disk_WorkState;
	}
}

void Disk::Request()
{
	request *rq;
	while ((rq = elv->Dequeue()) != NULL)
		SubmitRq(rq);
}

void Disk::Plug()
{
	if (!pluged) {
		pluged = 1;
		timer.ModTimer(now + delay);
	}
}

void Disk::Unplug()
{
	if (!RemovePlug())
		return;
	
	Request();
}

int Disk::RemovePlug()
{
	if (!pluged)
		return 0;
	
	timer.DelTimer();
	pluged = 0;
	return 1;
}

int Disk::IsPlug()
{
	return pluged;
}

void Disk::DiskUnplugWork(void *data)
{
	Disk *disk = (Disk *)data;
	
	disk->Unplug();
}

request *Disk::GetNextRequest()
{
	request *rq;
	
	if (Queue.Empty())
		return NULL;
	
	rq = (request *)Queue.suc;
	rq->Remove();
	
	return rq;
}

ioreq *Disk::GetNextIoreq(request *rq)
{
	List *head = &rq->batch;
	ioreq *req;
	
	if (head->Empty())
		return NULL;
	
	req = (ioreq *)head->suc;
	req->Remove();
	
	return req;
}

int Disk::RequestCompletion(request *rq)
{
	dctl *ctl = (dctl *) rq->data;

	if (ctl->dt == DISK_LATENCY) {
		ctl->t->Signal();
	} else { /* Performance test */
		if (++ctl->finished >= ctl->repeat) {
			obd_count bw;

            bw = ctl->totcnt * params.TimeUnit / 1024 / now;
            ctl->stat->Record("%lluKB	%lluKB/s\n", ctl->rsz / 1024, bw);
		}
		delete rq;
	}
	return 0;
}

void Disk::EndRequest(request *rq)
{
	ioreq *req;
	
	while ((req = GetNextIoreq(rq)) != NULL) {
		if (req->completion)
			req->completion(req);
	}
	StatDisk(rq);

	if (rq->completion)
		rq->completion(rq);
}

void Disk::DiskStateMachine()
{
	cfs_duration_t ticks = 0;
	
	switch (diskState) {
	case disk_WorkState:
		curq = GetNextRequest();
		if (curq == NULL) {
			diskState = disk_IdleState;
			break;
		}
		
		ticks = params.disk.Latency + SignRand(params.disk.LatencyRandom);
		if (headpos != curq->off) {
			seeknr++;
			ticks = params.disk.SeekTicks + SignRand(params.disk.SeekRandom);
		}
		
		Print(NOW "Disk@%d Handles request %s %llu:%llu.\n",
			now, id, curq->cmd ? "w" : "r", curq->off, curq->count);
						
		if (curq->cmd == READ)
			ticks += DT(curq->count / params.disk.ReadBandwidth);
		else 
			ticks += DT(curq->count / params.disk.WriteBandwidth);
		
		/* Handle the disk request. */
		RunAfter(ticks);
		diskState = disk_ReqFinishedState;
		break;
	case disk_ReqFinishedState:
		headpos = curq->off + curq->count;
		EndRequest(curq);
		if (IsPlug()) {
			diskState = disk_IdleState;
			break;
		}
		diskState = disk_WorkState;
		DiskStateMachine();
		break;
	case disk_IdleState:
		break;
	}
}

void Disk::Process(void *data)
{
	Disk *disk = (Disk *)data;
	
	disk->DiskStateMachine();
}

void Disk::RecordLatency(dctl *ctl)
{
	cfs_duration_t latency;

	latency = (now - ctl->stime) / ctl->repeat;
	ctl->stat->Record("%llu %llu.%06llu\n",
		ctl->rsz, latency / 1000000, latency % 1000000);
}

void Disk::LatencyStateMachine(void *arg)
{
	dctl *ctl = (dctl *) arg;
	request *rq = ctl->rq;

	switch (ctl->state) {
	case disk_LatencyFirstState:
		rq = new request;
		rq->cmd = WRITE;
		rq->data = ctl;
		rq->completion = RequestCompletion;
		ctl->rq = rq;

		ctl->stat->Record("# Disk latency:\n"
			"#fmt: (rsz[kb], latency[ms])\n");
	case disk_LatencyStartState:
		if (ctl->rsz > ctl->maxsz) {
			ctl->state = disk_LatencyLastState;
			LatencyStateMachine(ctl);
			break;
		}
		ctl->stime = now;
		ctl->finished = 0;
		rq->count = ctl->rsz;
	case disk_LatencyLaunchState:
		/* add the effect of disk seek time */
		rq->off = Rand(100000);
		ctl->d->SubmitRq(rq);
		ctl->state = disk_LatencyFinishState;
		break;
	case disk_LatencyFinishState:
		if (++ctl->finished < ctl->repeat)
			ctl->state = disk_LatencyLaunchState;
		else {
			RecordLatency(ctl);
			ctl->rsz *= ctl->factor;
			ctl->state = disk_LatencyStartState;
		}
		LatencyStateMachine(ctl);
		break;
	case disk_LatencyLastState:
		delete ctl->rq;
		break;
	}
}

void Disk::DiskLatencyTest()
{
	dctl ctl;

	memset(&ctl, 0, sizeof(ctl));
	ctl.dt = DISK_LATENCY;
	ctl.rsz = 1;
	ctl.maxsz = 1048576 * 256;
	ctl.factor = 2;
    ctl.repeat = 1000;
	ctl.d = new Disk;
	ctl.t = new Thread;
	ctl.stat = new Stat("disk.latency");

	ctl.t->CreateThread(LatencyStateMachine, &ctl);
	ctl.t->RunAfter(1);
	ctl.d->Start();
	
	Thread::Schedule();

	delete ctl.d;
	delete ctl.t;
	delete ctl.stat;
}

void Disk::LaunchIO(dctl *ctl)
{
	for (int i = 0; i < ctl->repeat; i++) {
		request *rq;
		
		rq = new request;
		rq->cmd = WRITE;
		rq->count = ctl->rsz;
		rq->data = ctl;
		rq->completion = RequestCompletion;

		if (ctl->dt == DISK_RANDPERF)
			rq->off = Rand(100000);
		else
			rq->off = i * ctl->rsz;

		ctl->d->SubmitRq(rq);
	}
}

void Disk::DiskPerfTest(int dt)
{
	dctl ctl;

	memset(&ctl, 0, sizeof(ctl));
	ctl.dt = dt;
	ctl.maxsz = 1048576 * 32; /* 32M */
	ctl.totcnt = 2147483648ULL; /* 2G */
	ctl.d = new Disk;

	if (dt == DISK_RANDPERF)
		ctl.stat = new Stat("Disk.rand");
	else if (dt == DISK_SEQPERF)
		ctl.stat = new Stat("Disk.seq");

    ctl.stat->Record("#Performance Test:\n #fmt (rsz[kb], bandwidth[KB/s])\n");
	ctl.d->Start();

	for (ctl.rsz = 4096; ctl.rsz <= ctl.maxsz; ctl.rsz *= 2) {
		ctl.repeat = ctl.totcnt / ctl.rsz;
		ctl.finished = 0;
		
		now = 0;
		LaunchIO(&ctl);
		Thread::Schedule();
	}
	
	delete ctl.d;
	delete ctl.stat;
}
void Disk::DiskRandomTest()
{
	DiskPerfTest(DISK_RANDPERF);
}

void Disk::DiskSequentTest()
{
	DiskPerfTest(DISK_SEQPERF);
}

void Disk::SelfBenchmark()
{
	if (params.test.DiskLatency)
		DiskLatencyTest();
	if (params.test.DiskRandPerf)
		DiskRandomTest();
	if (params.test.DiskSeqPerf)
		DiskSequentTest();
}

void Disk::SetPlugDelay(cfs_duration_t d)
{
	delay = d;
}

void Disk::InitElevator()
{
	if (params.disk.ElvNoop)
		elv = new ElvNoop;
	else if (params.disk.ElvDeadline)
		elv = new ElvDeadline;

	elv->Attach(this);
}

void Disk::Start()
{
	printf(NOW "Start Disk@%d.\n", now, id);
	CreateThread(Process, this);
	timer.SetupTimer(DiskUnplugWork, this);

	InitStat();

	InitElevator();
}
