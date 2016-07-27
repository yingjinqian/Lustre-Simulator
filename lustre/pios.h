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
#ifndef PIOS_H
#define PIOS_H

/**
	@author yingjin.qian <yingjin.qian@sun.com>
	parallel I/O simulator
*/
#include "lustre.h"
#include "event.h"
#include "stat.h"
#include "filesystem.h"

class PIOS{

#define PIOS_VERSION "1.3"

#define __u64 uint64_t
#define __u32 uint32_t

/* IO types */
#define COWIO 1
#define DIRECTIO 2
#define POSIXIO 4
#define DMUIO 8          /* ZFS DMU mode */

/* Regular expressions */
#define REGEX_NUMBERS "^[0-9]*[0-9]$"
#define REGEX_NUMBERS_COMMA "^([0-9]+,)*[0-9]+$"
#define REGEX_SIZE "^[0-9][0-9]*[kmgt]$"
#define REGEX_SIZE_COMMA "^([0-9][0-9]*[kmgt]+,)*[0-9][0-9]*[kmgt]$"

/* Flags for low, high, incr */
#define FLAG_LOW 2
#define FLAG_HIGH 4
#define FLAG_INCR 8

	/* low/high */
	struct pios_params {
		__u32 T[2];
		__u64 N[2];
		__u64 O[2];
		__u64 C[2];
		__u64 S[2];
		__u64 V[2];

		__u32 fpp;
		__u32 iotype; /* COW, POSIX, DIRECTIO */
		__u32 factor;

		/* three noises */
		__u32 regionnoise;
		__u64 chunknoise;
		__u32 threaddelay;

		int fsync;
		int cpuload;
		int cleanup; /* cleanup the files created during testing */
	};

	struct pios_stream {
		__u64 offset;
		__u64 max_offset;
		__u64 fid;
		cfs_duration_t iotime;
	};

	struct pios_thdata;
	/* arguments for one run */
	struct run_arg {
        	int runno;
        	int num_open_files;
        	__u32 threadcount;
        	__u64 chunksize;
        	__u64 regioncount;
        	__u64 regionsize;
        	__u64 offset;

        	__u32 regionnoise;
        	__u32 chunknoise;
        	__u32 thread_delay;
        	__u64 verify;
        	__u32 cpu_load;
        	//time_t timestamp;
        	__u32 io_type;
		__u32 fpp;
        	//const char *path;

		__u32 next_region;
		pios_stream *streams;
		pios_thdata *ptds;
		ThreadLocalData *tlds;
		
		__u32 finicount;
		cfs_time_t startime;
		cfs_duration_t iotime;

		FileSystem *fs;
		BlockDevice *blk;

		Thread *t;
		Stat *st;
	};

	struct pios_thdata {
		__u32 regionidx;
		run_arg *args;
		IO io;
		Thread *parent;
		cfs_time_t startime;
	};

	enum {
		pios_RunOneStartState,
		pios_RunOneAllFinishState,
		pios_RunOneLastState,
	};
	
	enum {
		pios_ThreadStartState,
		pios_ThreadRWState,
		pios_ThreadRWFinishState,
		pios_ThreadLastState,
	};

	static void do_one_run(void *data);
	static void do_one_thread(void *data);
	static void MainThread(void *data);
	static int get_work_item(run_arg *args, pios_thdata *ptd);
	static void print_stats(run_arg *arg);
	static char *in_MBs(char *str, __u64 num);
	static void find_high_low_duration(run_arg *args, cfs_duration_t *highest,
		cfs_duration_t *lowest, cfs_duration_t *average);
	static __u64 pios_interpret_KMGT(const char *numstr);

	static void OneRun(run_arg *args);
	static void Print(const char *fmt...);

public:
	PIOS();
	~PIOS();

	static void Run();
};

#endif
