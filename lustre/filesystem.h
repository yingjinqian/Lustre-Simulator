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
#ifndef FILESYSTEM_H
#define FILESYSTEM_H

//#include <device.h>
#include <rbtree.h>
#include <blockdevice.h>
#include "processor.h"
#include "disk.h"
#include "node.h"
#include "obd.h"

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/
class FileSystem : public Device
{
public:
	#define FS_STREAMALLOC	0
	#define FS_MBALLOC	1

	struct extent {
		rb_node rbnode;
		obd_off foff; /* file offset */
		obd_off doff; /* corresponding disk offset */
		obd_count count;
	};

	struct prealloc_space {
        struct list_head inode_list;
		unsigned deleted;
		obd_off lstart; /* logical start */
		obd_off pstart; /* physical start */
		obd_count len;
		obd_count free;
	};

#define REQ_MAP_FROM_PA 1
	struct file {
		int flags;
        bool used;
		obd_id fid;
		obd_count size;
		Rbtree extTree;
		extent *last;

		prealloc_space pa; /* used by FPP mode */
		list_head prealloc_list; 
		void *data;
		void destory(file *f);

		/* used by FRR I/O scheduling algorithm */
		io_queue q[2];
	public:
		file()
		{
			data = NULL;
            used = false;
			INIT_LIST_HEAD(&prealloc_list);
		}

		~file()
		{
            prealloc_space *pa, *temp;

            if (used == false) {
                assert(list_empty(&prealloc_list));
                return;
            }

            list_for_each_entry_safe(pa, temp, &prealloc_list, inode_list) {
                list_del_init(&pa->inode_list);
                delete pa;
            }
		}
	};

	struct allocation_context {
		file *f;
		ioreq *req;
		obd_off lstart;
		obd_off pstart;
		obd_count len;
		prealloc_space *pa;
	};

private:
	char name[MAX_NAME_LEN];
	int totFiles;
	file *files;
	static  int num; /*Number of file system. */
	int fsid;
	int mntopt;
	BlockDevice *disk;
	Server *site;

	extent *cache;
	static int ExtentCompare(void *a, void *b);
	static int Completion(ioreq *req);

	/*Multi-Block Allocation */ 
	int bsbit;
	obd_count mb_stream_size;
	obd_count mb_prealloc_count;
	int MbAllocBlock(file *f, ioreq *req);
	int MbUsePreallocation(allocation_context *ac);
	int MbNewPreallocation(allocation_context *ac);
	void MbUseInodePA(allocation_context *ac);
	void MbInitContext(allocation_context *ac);
	void MbReleaseContext(allocation_context *ac);

	enum {
		fs_RWStartState,
		fs_RWAllocateBlockState,
		fs_RWSubmitState,
		fs_RWFinishState,
	};
	struct rwdesc {
		IO *io;
		file *f;
		ioreq *req;
		obd_count count;
		obd_count off;
		FileSystem *fs;
	};
	static void ReadWriteStateMachine(void *arg);

	/** TEST */
	struct fctl {
		int state;
		int factor;
		int repeat;
		int finished;
		int seq; /* sequential test */
		obd_count rsz;
		obd_count maxsz;
		cfs_time_t stime;

		/* multiple threads */
		int thnr;
		int maxtnr;
		int iocnt;
		cfs_duration_t delay;
		cfs_duration_t mindly;
		cfs_duration_t maxdly;
		
		Disk *d;
		ioreq *req;
		Thread *t;
		Stat *stat;
	};
	struct fdata {
		int state;
		fctl *ctl;
		Thread *t;
	};
	enum {
		fs_LatencyFirstState,
		fs_LatencyStartState,
		fs_LatencyLaunchState,
		fs_LatencyFinishState,
		fs_LatencyLastState,
	};
	enum {
		fs_PerfFirstState,
		fs_PerfStartState,
		fs_PerfLaunchState,
		fs_PerfFinishState,
		fs_PerfLastState,
	};
	static int TesterCompletion(ioreq *req);
	static void RecordLatency(fctl *ctl);
	static void LatencyStateMachine(void *arg);
	static void RecordPerformance(fctl *ctl);
	static void PerfStateMachine(void *arg);
	static void FileSystemElvTest();
	static void FileSystemLatencyTest();
	static void FileSystemPerfTest();
public:
	FileSystem();

	~FileSystem();

	int Map(file *f, ioreq *req, int alloc);
	int Write(IO *io);
	int Read(IO *io);
	int ReadWrite(IO *io);

	void Start();
	void Attach(Server *s);

	void InitIoQueue(file *f);
	io_queue *GetIoQueue(obd_id fid, int rw);

	static void SelfTest();
};

#endif
