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
#include "filesystem.h"
#include "raid0device.h"

int FileSystem::num;
FileSystem::FileSystem()
 : Device()
{
	totFiles = params.fs.FilesPerFileSystem;

    if (params.io.Mode == ACM_FPP &&
            totFiles <= params.cluster.ClientCount + params.cluster.OstCount)
        totFiles = params.cluster.ClientCount + params.cluster.OstCount + 1;

	if (params.fs.AllocALGO == 1) {
		bsbit = 12; /* 4k block */
		mntopt = FS_MBALLOC;
		mb_prealloc_count = 0;
		mb_stream_size = params.fs.StreamRequestSize;
	} else
		mntopt = FS_STREAMALLOC;

	files = NULL;
	disk = NULL;
	site = NULL;
	fsid = num++;
	cache = NULL;

	__dbg = params.debug.FS;
}

FileSystem::~FileSystem()
{
	/*
	 * TODO: release all extent cache and PAs.
	 */

#if 0
	for (int i = 0; i < totFiles; i++) {
		for (int j = 0; j < 2; j++) {
			io_queue *q = &files[i].q[j];
			
			if (q->flags & IO_QUEUE_INITED)
				delete q->s;
		}
	}
#endif

    /** TODO free files */

//    for (int i = 0; i < totFiles; i++) {
//        prealloc_space *pa;
//        file *f = &files[i];

//        if (f->used == false) {
//            assert(list_empty(&f->prealloc_list));
//            continue;
//        }

////        list_for_each_entry(pa, &(f->prealloc_list), inode_list) {
////            assert(pa != NULL);
////            printf("Delete pa@%p, %llu:%llu:%llu\n", pa, pa->lstart, pa->pstart, pa->len);
////            list_del_init(&(pa->inode_list));
////            delete pa;
////        }
//    }
    if (files)
        delete [] files;
	
	if (cache)
		delete cache;

	if (disk)
		delete disk;
}

int FileSystem::ExtentCompare(void *a, void *b)
{
	extent *e1, *e2;
	
	e1 = (extent *)a;
	e2 = (extent *)b;
	
	if (e1->foff + e1->count < e2->foff )
		return -1;
	if (e1->foff >= e2->foff + e2->count)
		return 1;
	return 0; 
}

void FileSystem::MbInitContext(allocation_context *ac)
{
	ac->pa = NULL;
	ac->lstart = 0;
	ac->pstart = 0;
	ac->len = 0;
}

void FileSystem::MbReleaseContext(allocation_context *ac)
{
	prealloc_space *pa = ac->pa;

	if (params.fs.ReservePA && pa && pa->free == 0) {
		pa->deleted = 1;
		list_del(&pa->inode_list);
		delete pa;
	}
}

int FileSystem::MbUsePreallocation(allocation_context *ac)
{
	prealloc_space *pa;
	ioreq *req = ac->req;
	file *f = ac->f;

	list_for_each_entry(pa, &f->prealloc_list, inode_list) {
		if (req->off < pa->lstart || 
                    req->off >= pa->lstart + pa->len)
			continue;

		if (pa->deleted == 0 && pa->free) {
			ac->pa = pa;
			MbUseInodePA(ac);
			req->flags |= REQ_MAP_FROM_PA;
			return 1;
		}
	}
	return 0;
}

void FileSystem::MbUseInodePA(allocation_context *ac)
{
	prealloc_space *pa = ac->pa;
	ioreq *req = ac->req;
	obd_count end;

	assert(pa->lstart <= req->off);
	req->off = pa->pstart + (req->off - pa->lstart);
	end = min(pa->pstart + pa->len, req->off + req->count);
	req->count = end - req->off;
	BUG_ON(pa->free < req->count);
	pa->free -= req->count;
}

int FileSystem::MbNewPreallocation(allocation_context *ac)
{
	file *f = ac->f;
	prealloc_space *pa;

	assert(ac->lstart <= ac->req->off || ac->len > ac->req->count);

	pa = new prealloc_space;
	pa->lstart = ac->lstart;
	pa->pstart = ac->pstart;
	pa->len = ac->len;
	pa->free = pa->len;
	pa->deleted = 0;

	ac->pa = pa;
	MbUseInodePA(ac);
	list_add(&pa->inode_list, &f->prealloc_list);
    //printf("Alloc pa@%p, %llu:%llu:%llu\n", pa, pa->lstart, pa->pstart, pa->len);
	return 0;
}

/* 
 * simulation for the ldiskfs mballoc algorithm.
 * TODO: Mballoc should be an independent Class
 */
int FileSystem::MbAllocBlock(file *f, ioreq *req)
{
	allocation_context ac;
	prealloc_space *pa;
	obd_off start, end;
	obd_count size;
	int rc;
	/* 
	 * Init allocation context.
	 */
	ac.req = req;
	ac.f = f;
	MbInitContext(&ac);

	if (MbUsePreallocation(&ac)) {
		Print(NOW"Mballoc: find mapping from preallocation,"
			"off %llu, len %llu.\n", now, req->off, req->count);
		GOTO(out, rc = 0);
	}
	
	size = req->off + req->count;
	if (size < f->size)
		size = f->size;

	start = 0;
	assert(bsbit != 0);
	if (size <= 64 * 1024)
		size = 64 * 1024;
	else if (size <= 128 * 1024)
		size = 128 * 1024;
	else if (size <= 256 * 1024)
		size = 256 * 1024;
	else if (size <= 512 * 1024)
		size = 512 * 1024;
	else if (size <= 1024 * 1024)
		size = 1024 * 1024;
	else if (size <= 4 * 1024 * 1024) {
		start = req->off >> (21 - bsbit) << 21;
		size = 2 * 1024 * 1024;
	} else if ( size <= 8 * 1024 * 1024) {
		start = req->off >> (22 - bsbit) << 22;
		size = 4 * 1024 * 1024;
	} else if (req->count <= (8 << 20)) {
		start = req->off >> (23 - bsbit) << 23;
		size = 8 * 1024 * 1024;
	} else {
		Print(NOW"The req size (%llu) in Lustre should be less than 1M.\n",
			now, req->count);
		return 1;
	}

	start >>= bsbit;
	end = start + size;
	/* check whether corss the already preallocated blocks */
	list_for_each_entry(pa, &f->prealloc_list, inode_list) {
		obd_count pa_end;
		
		if (pa->deleted)
			continue;
		
		pa_end = pa->lstart + pa->len;
		assert(req->off >= pa_end || req->off < pa->lstart);
		
		if (pa->lstart >= end)
			continue;

		if (pa_end <= start)
			continue;
		
		BUG_ON(pa->lstart <= start && pa_end >= end);

		if (pa->free == 0)
			Print(NOW "XXX Mballoc: [%llu, %llu] overlap with the existed PA (use out) [%llu, %llu]",
				now, start, end, pa->lstart, pa_end);
		if (pa_end <= req->off){
			assert(pa_end >= start);
			start = pa_end;
		}

		if (pa->lstart > req->off) {
			assert(pa->lstart <= end);
			end = pa->lstart;
		}
	}

	size = end - start;

	if (start + size <= req->off && start > req->off)
		Print(NOW"Mballoc: start %llu size %llu, logical off %llu\n",
			now, start, size, req->off);

	BUG_ON(start + size <= req->off && start > req->off);
	ac.lstart = start;
	/*
	 * Supposed that there is only one ext4 group,
	 * allocate blocks via sequential way. And frist 
	 * try to goal always succeeds.
	 */
	ac.pstart = disk->AllocBlock(size);
	ac.len = size;

	if (!params.fs.ReservePA && start == req->off && size <= req->count) {
		req->off = ac.pstart;
		req->count = size;
		GOTO(out, rc = 0);
	}
	
	/*
	 * Just preallocated more space than user requested originally,
	 * store allocated space in a special descriptor (PA) 
	 */
	rc = MbNewPreallocation(&ac);

out:
	MbReleaseContext(&ac);
	return rc;
}

int FileSystem::Map(file *f, ioreq *req, int alloc)
{
	Rbtree	*tree = &f->extTree;
	rb_node **p, *parent = NULL;
	extent *near = NULL;
	int rc = 1;
		
	if (cache == NULL)
		cache = new extent;
	
	cache->foff = req->off;
	cache->count = 0;
	cache->rbnode.rb_entry = cache;
	
	//rc = tree->rb_find_nearest(&cache->rbnode, parent, p);
	p = &tree->root.rb_node;
	while (*p) {
		parent = *p;
		rc = ExtentCompare(cache, parent->rb_entry);
		if (rc < 0)
			p = &parent->rb_left;
		else if (rc > 0)
			p = &parent->rb_right;
		else
			break;
	}
	
	if (parent != NULL)
		near = (extent *)parent->rb_entry;
		
	/*
	 * Mapping offset is in the range [foff, foff + count] 
	 * of returned parent node.
	 */
	if (rc == 0) {
		obd_count delta;
		
		assert(parent != NULL);

		delta = req->off - near->foff;
		req->off = near->doff + delta;
		req->count = min(req->count, near->count - delta);
		assert(req->count > 0);
		return 0;
	} else if (alloc == 0)
		return -1;
	
	if (rc < 0) 
		req->count = min(req->count, near->foff - req->off);
	
	if (mntopt == FS_STREAMALLOC) {
		req->off = disk->AllocBlock(req->count);
	} else { // FS_MBALLOC
		rc = MbAllocBlock(f, req);
		if (rc < 0) {
			Print(NOW "Mballoc failed: %d.\n", now, rc);
			return rc;
		}
	}

	/* contiguous with previous node, Merge them. */
	if (near && (cache->foff == near->foff + near->count) && 
            (req->off == near->doff + near->count)) {
		Print(NOW "Merge mapping Extent %llu:%llu -> %llu:%llu.\n",
			now, req->off, req->count, near->doff, near->count);
		near->count += req->count;
		return 0;
	}

	cache->count = req->count;
	cache->doff = req->off;
	tree->rb_link_node(&cache->rbnode, parent, p);
	tree->rb_insert_color(&cache->rbnode);
	
	cache = NULL;
	return 0;
}

int FileSystem::Completion(ioreq *req)
{
	IO *io = req->io;
	
	if (--io->ref == 0 && io->left == 0) {
		if (io->waiter) {
			io->waiter->Signal();
		} else {
			Message *msg;
		
			msg = (Message *) io->data;
			//printf(NOW "FINI Disk I/O for REQ@%d.\n",
			//	now, msg->GetId());
			msg->Notify();
		}
	}
	delete req;
}

void FileSystem::ReadWriteStateMachine(void *arg)
{
	ThreadLocalData *tld = (ThreadLocalData *)arg;
	Thread *t = tld->t;
	rwdesc *desc = (rwdesc *)tld->v;
	FileSystem *fs = desc->fs;
	ioreq *req = desc->req;
	file *f = desc->f;
	IO *io = desc->io;
	int rc;

	switch (tld->state) {
	case fs_RWStartState: {
		desc->count = io->left = io->count;
		desc->off = io->off;
	}
	case fs_RWAllocateBlockState: {
		cfs_duration_t ticks;

		desc->req = req = new ioreq;
		req->io	= io;
		req->cmd = io->cmd;
		req->off = desc->off;
		req->count = desc->count;
		req->ref = 0;
		req->flags = 0;
		req->next = NULL;
		req->completion = Completion;

		assert(req->count > 0);
		rc = fs->Map(f, req, io->cmd == WRITE);
		if (rc < 0) {
			printf(NOW "%s Mapping FID@%llu %llu:%llu failed.\n",
				now, fs->GetDeviceName(), io->fid, 
				io->off, io->count);
			exit(1);
		}

		ticks = params.fs.AllocateBlockTime;
		/* It takes much more time to allocate 
		 * new blocks for write. */
		if (req->cmd == READ)
			ticks /= 20;
		if (req->cmd == WRITE && req->flags & REQ_MAP_FROM_PA)
			ticks /= 5;

		tld->state = fs_RWSubmitState;
		t->RunAfter(ticks + SignRand(ticks / 5));
		break;
	}
	case fs_RWSubmitState:
		fs->Print(NOW "%s SUBMIT %s FID@%llu File@%llu:%llu <-> Disk@%llu:%llu\n",
			now, fs->GetDeviceName(), io->cmd ? "WRITE" : "READ",
			io->fid, io->off, io->count, req->off, req->count);
		fs->disk->SubmitIoreq(req);
		io->ref++;
		desc->off += req->count;
		desc->count -= req->count;
		io->left = desc->count;
		if (desc->count > 0) {
			tld->state = fs_RWAllocateBlockState;
			ReadWriteStateMachine(tld);
			break;
		}
	case fs_RWFinishState:
		assert(desc->count == 0);
		PopCurCtxt();
		//tld->v = NULL;
		delete desc;
		break;
	}
}

int FileSystem::ReadWrite(IO *io)
{
	file *f;
	ioreq *req;
	obd_count count = io->count;
	obd_count off = io->off;
	int rc = 0;

	if (io->count == 0)
		return 0;

	if (params.fs.AllocateBlockTime) {
		ThreadLocalData *tld = CurrentTLD();
		rwdesc *desc;

		//assert(tld->v == NULL);
		desc = new rwdesc;
		memset(desc, 0, sizeof(*desc));
		desc->io = io;
		desc->fs = this;
		desc->f = &files[io->fid];
        desc->f->used = true;
		assert((desc->f != NULL) && io->fid < totFiles);

		PushCurCtxt(fs_RWStartState, ReadWriteStateMachine);
		tld->v = desc;
		ReadWriteStateMachine(tld);
		return 0;
	}

	assert(io->fid <= totFiles);
	f = &files[io->fid];
    f->used = true;
	assert(f != NULL);
	while (count > 0) {
		req = new ioreq;
		req->io	= io;
		req->cmd = io->cmd;
		req->off = off;
		req->count = count;
		req->ref = 0;
		req->next = NULL;
		req->completion = Completion;

		rc = Map(f, req, io->cmd == WRITE);
		if (rc < 0) {
			printf(NOW "%s Mapping FID@%llu %llu:%llu failed.\n",
				now, name, io->fid, io->off, io->count);
			exit(1);
		}

		Print(NOW "%s SUBMIT %s FID@%llu File@%llu:%llu <-> Disk@%llu:%llu\n",
			now, name, io->cmd ? "WRITE" : "READ", io->fid, io->off, 
			io->count, req->off, req->count);
		disk->SubmitIoreq(req);
		io->ref++;
		off += req->count;
		count -= req->count;
		io->left = count;
	}	

	return rc;
}

int FileSystem::Read(IO *io)
{
	return ReadWrite(io);
}

int FileSystem::Write(IO *io)
{
	return ReadWrite(io);
}

void FileSystem::FileSystemElvTest()
{
	FileSystem fs;
	ioreq *req;
	int i, num;

    //num = 16;
    num = 3;
    req = new ioreq [num];
	fs.Start();
	for (i = 0; i < num; i++) {
		req[i].cmd = WRITE;
		req[i].completion = NULL;
		req[i].count = 1048576;
		req[i].next = NULL;
		//req[i].off = i * 1048576;
	}

//	req[0].off = 1048576 * 2;
//	req[1].off = 1048576 * 0;
//	req[2].off = 1048576 * 9;
//	req[3].off = 1048576 * 3;
//	req[4].off = 1048576 * 1;
//	req[5].off = 1048576 * 7;
//	req[6].off = 1048576 * 5;
//	req[7].off = 1048576 * 6;
//	req[8].off = 1048576 * 8;
//	req[9].off = 1048576 * 4;
//	req[10].off = 1048576 * 15;
//	req[11].off = 1048576 * 11;
//	req[12].off = 1048576 * 13;
//	req[13].off = 1048576 * 12;
//	req[14].off = 1048576 * 14;
//	req[15].off = 1048576 * 10;

    /* for debug */
    req[0].off = 1048576 * 2;
    req[1].off = 1048576 * 0;
    req[2].off = 1048576 * 1;

	for (i = 0; i < num; i++) {
		fs.disk->SubmitIoreq(&req[i]);
	}
	
	Event::Schedule();
	delete [] req;
}

int FileSystem::TesterCompletion(ioreq *req)
{
	fctl *ctl = (fctl *) req->next;

	if (params.test.FsPerformance) {
		delete req;
		if (--ctl->iocnt)
			return 0;
	}

	ctl->t->Signal();
	return 0;
}

void FileSystem::RecordLatency(fctl *ctl)
{
	cfs_duration_t latency;
	obd_count bw;

	latency = (now - ctl->stime) / ctl->repeat;
	bw = ctl->rsz * params.TimeUnit / latency;
	ctl->stat->Record("%llu %llu.%06llu %llu.%03llu\n",
		ctl->rsz / 1024, latency / 1000000, latency % 1000000, 
		bw / params.SizeUnit, (bw % params.SizeUnit) * 1000 / params.SizeUnit);
}

void FileSystem::LatencyStateMachine(void *arg)
{
	fctl *ctl = (fctl *) arg;
	ioreq *req = ctl->req;

	switch (ctl->state) {
	case fs_LatencyFirstState:
		req = new ioreq;
		req->cmd = WRITE;
		req->next = (ioreq *) ctl;
		req->completion = TesterCompletion;
		ctl->req = req;
		ctl->stat->Record("# Filesystem latency:\n"
			"#fmt: (rsz[kb] latency[ms] perf[MB/s])\n");
	case fs_LatencyStartState:
		if (ctl->rsz > ctl->maxsz) {
			ctl->state = fs_LatencyLastState;
			LatencyStateMachine(ctl);
			break;
		}
		ctl->stime = now;
		ctl->finished = 0;
		req->count = ctl->rsz;
	case fs_LatencyLaunchState:
		if (ctl->seq)
			req->off = ctl->rsz * ctl->finished;
		else 
			req->off = Rand(100000);
		ctl->d->SubmitIoreq(req);
		ctl->state = fs_LatencyFinishState;
		break;
	case fs_LatencyFinishState:
		if (++ctl->finished < ctl->repeat)
			ctl->state = fs_LatencyLaunchState;
		else {
			RecordLatency(ctl);
			ctl->rsz *= ctl->factor;
			ctl->state = fs_LatencyStartState;
		}
		LatencyStateMachine(ctl);
		break;
	case fs_LatencyLastState:
		delete ctl->req;
		break;
	}
}

void FileSystem::FileSystemLatencyTest()
{
	fctl ctl;

	memset(&ctl, 0, sizeof(ctl));
	ctl.factor = 2;
	ctl.repeat = 30;
	ctl.rsz = 1024;
	ctl.maxsz = 1048576 * 256; /* 256 M */
	ctl.seq = 1;
	ctl.thnr = 1;

	ctl.stat = new Stat("fs.latency");
	ctl.d = new Disk;
	ctl.t = new Thread;
	ctl.t->CreateThread(LatencyStateMachine, &ctl);
	ctl.t->RunAfter(1);
	ctl.d->Start();

	Event::Schedule();

	delete ctl.d;
	delete ctl.t;
	delete ctl.stat;

}

void FileSystem::RecordPerformance(fctl *ctl)
{
	cfs_duration_t latency;
	obd_count bw;

	latency = (now - ctl->stime) / ctl->repeat;
	bw = ctl->thnr * ctl->rsz * params.TimeUnit / latency;
	ctl->stat->Record("%d	%llu.%llu	%llu	%llu.%06llu	%llu.%03llu\n",
		ctl->thnr, ctl->delay / 1000000, ctl->delay % 1000000 / 100000,
		ctl->rsz / 1024, latency / 1000000, latency % 1000000,
		bw / params.SizeUnit, (bw % params.SizeUnit) * 1000 / params.SizeUnit);
	ctl->stime = now;
}

void FileSystem::PerfStateMachine(void *arg)
{
	fctl *ctl = (fctl *) arg;

	switch (ctl->state) {
	case fs_PerfFirstState:
		ctl->stat->Record("# Filesystem latency:\n"
			"#fmt: (thnr unplug_delay[ms]	rsz[kb] latency[ms] perf[MB/s])\n");
		ctl->delay = ctl->mindly;
	case fs_PerfStartState:
		if (ctl->rsz > ctl->maxsz) {
			ctl->state = fs_PerfLastState;
			LatencyStateMachine(ctl);
			break;
		}
		printf(NOW "start to test for rsz@%llu\n",
			now, ctl->rsz);
	case fs_PerfLaunchState: {
		assert(ctl->iocnt == 0);
		for (int i = 0; i < ctl->thnr; i++) {
			ioreq *req;

			req = new ioreq;
			req->cmd = WRITE;
			req->next = (ioreq *) ctl;

			/* Make sure the offset is not contiguous (random),
			 * Otherwise, the elevator will merge them.
			 */
			req->off = Rand(ctl->rsz) + 2 * i * ctl->rsz;
			req->count = ctl->rsz;
			req->completion = TesterCompletion;
			ctl->d->SubmitIoreq(req);
			ctl->iocnt++;
		}
		ctl->state = fs_PerfFinishState;
		break;
	}
	case fs_PerfFinishState:
		if (++ctl->finished < ctl->repeat) {
			ctl->state = fs_PerfLaunchState;
		} else {
			RecordPerformance(ctl);
			
			ctl->finished = 0;
			printf(NOW "finish the test for rsz@%llu thnr@%d\n",
				now, ctl->rsz, ctl->thnr);
			if (ctl->thnr < ctl->maxtnr) {
				ctl->thnr *= ctl->factor;
				ctl->state = fs_PerfLaunchState;
			} else {
				ctl->thnr = 1;
 				if (ctl->delay < ctl->maxdly) {
					ctl->delay *= ctl->factor;
					ctl->d->SetPlugDelay(ctl->delay);
					printf(NOW "chage the disk unplug delay %llu.\n",
						now, ctl->delay);
					ctl->state = fs_PerfLaunchState;
				} else {
					ctl->delay = ctl->mindly;
					ctl->rsz *= ctl->factor;
					ctl->state = fs_PerfStartState;
				}
			}
		}
		PerfStateMachine(ctl);
		break;
	case fs_PerfLastState:
		break;
	}
}

void FileSystem::FileSystemPerfTest()
{
	fctl ctl;

	memset(&ctl, 0, sizeof(ctl));
	ctl.factor = 2;
	ctl.repeat = 1;
	ctl.rsz = 1048576;
	ctl.maxsz = 1048576 * 128; /* 128 M */
	ctl.seq = 0;
	ctl.thnr = 1;
	ctl.maxtnr = 256;
	ctl.mindly = 100000; /* 0.1ms */
	ctl.maxdly = 30000000; /* 30ms */

	ctl.stat = new Stat("fs.perf");
	ctl.d = new Disk;
	ctl.t = new Thread;
	ctl.t->CreateThread(PerfStateMachine, &ctl);
	ctl.t->RunAfter(1);
//	ctl.d->SetPlugDelay(ctl.mindly);
	ctl.d->Start();

	Event::Schedule();

	delete ctl.d;
	delete ctl.t;
	delete ctl.stat;
}

void FileSystem::SelfTest()
{
	if (params.test.FsElv)
		FileSystemElvTest();
	if (params.test.FsLatency)
		FileSystemLatencyTest();
	if (params.test.FsPerformance)
		FileSystemPerfTest();
}

void FileSystem::Attach(Server *s)
{
	site = s;
}

void FileSystem::InitIoQueue(file *f)
{
	
}

io_queue *FileSystem::GetIoQueue(obd_id fid, int rw)
{
	LASSERT(fid < totFiles);
	return &files[fid].q[rw];
}

void FileSystem::Start()
{
	if (site != NULL)
		sprintf(name, "FS%d@%s", fsid, site->GetDeviceName());
	else
		sprintf(name, "FS%d", fsid);

	Print(NOW"Start %s...\n", now, name);

	files = new file [totFiles];
	for (int i = 0; i < totFiles; i++) {
		files[i].flags = 0;
		files[i].size = 0;
		files[i].last = NULL;
		files[i].extTree.rb_set_compare(ExtentCompare);
		files[i].fid = i;
		INIT_LIST_HEAD(&files[i].prealloc_list);
	}
	
	switch (params.disk.BlockDeviceType) {
	case BLK_PLAIN:
		disk = new Disk;
		break;
	case BLK_RAID0:
		disk = new Raid0Device;
		break;
	default:
		printf("Not support block device type!\n");
		abort();
	}

	disk->type = params.disk.BlockDeviceType;
	disk->Attach(site);
	disk->Start();
}
