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
#include "pios.h"
#include "processor.h"

PIOS::PIOS()
{
}


PIOS::~PIOS()
{
}

void PIOS::Print(const char *fmt...)
{
	if (params.debug.PIOS) {
		va_list args;
	
		va_start(args,fmt);
		vfprintf(stdout,fmt, args);
		va_end(args);
	}
}

/* extracts an unsigned int (64) and K,M,G,T from the strin */
/* converts string to kilo-tera units */
__u64 pios_interpret_KMGT(const char *number_string)
{
        __u32 length;
        __u64 number;

        length = strlen(number_string);
        number = atoll(number_string);
        switch(number_string[length - 1]) {
        case 'k':
        case 'K':
                number = number << 10;
                break;
        case 'm':
        case 'M':
                number = number << 20;
                break;
        case 'g':
        case 'G':
                number = number << 30;
                break;
        case 't':
        case 'T':
                number = number << 40;
                break;
        }
        return number;
}

#if 0
void PIOS::setrlimit_for_nofiles(struct pios_args *pios_args)
{
	struct rlimit rlim;
        unsigned long limit = 0;

        //getrlimit(RLIMIT_NOFILE, &rlim);

        if (pios_args->N.val_count == 0) {
                limit = pios_args->N.val_high;
        } else {
                int i;

                for (i = 0; i < pios_args->N.val_count; i++) {
                        if (pios_args->N.val[i] > limit)
                                limit = pios_args->N.val[i];
                }
        }

        limit += 10;    /* Need some extra files for stdin/stdout/stderr/log */
        if (pios_args->fpp && limit > params.fs.FilesPerFileSystem) {
                fprintf(stderr, "pios: current limit of open files is %lu, "
                        "need %lu for regioncount%s=%lu\n", params.fs.FilesPerFileSystem,
                        limit, pios_args->N.val_count ? "" : "_high", limit-10);
		exit(3);
               /*rlim.rlim_cur = rlim.rlim_max = limit;
                if (setrlimit(RLIMIT_NOFILE, &rlim) == -1) {
                        fprintf(stderr, "pios: setrlimit failed - specify a "
                                "lower regioncount%s= parameter\n",
                                pios_args->N.val_count ? "" : "_high");
                        exit(3);
                }

                fprintf(stderr, "pios: setting soft limit of open files to %lu "
                        "and hard limit to %lu\n",
                        rlim.rlim_cur, rlim.rlim_max);
		*/
        }
}

#endif

/* Convert @num to string postfixed with MB/KB */
char *PIOS::in_MBs(char *str, __u64 num)
{
	if (num < 1048576) {
		num /= 1024;
		sprintf(str, "%lluKB", (long long)num);
	} else {
		num /= 1048576;
		sprintf(str, "%lluMB", (long long)num);
	}
	return str;
}

void PIOS::find_high_low_duration(run_arg *args, cfs_duration_t *highest,
		cfs_duration_t *lowest, cfs_duration_t *average)
{
	int i;

	*lowest = args->streams[0].iotime;
	*highest = args->streams[0].iotime;
	*average = args->streams[0].iotime;

	for (i = 1; i < args->regioncount; i++) {
		*average += args->streams[i].iotime;
		if (args->streams[i].iotime <= *lowest)
			*lowest = args->streams[i].iotime;
		else if (args->streams[i].iotime >= *highest)
			*highest = args->streams[i].iotime;
	}

	*average /= args->regioncount;
}

void PIOS::print_stats(run_arg *args)
{
	char regionstr[32], chunkstr[32], mbstr[32];
	cfs_duration_t highest, lowest, average;
	__u64 agvsize;
	Stat *st = args->st;

	assert(st != NULL);
	st->Record("%d\t%s\t%llu\t%d\t%llu\t%s\t%s\t",
		args->runno,
		args->verify ? "Read" : "Write",
		args->startime,
		args->threadcount,
		args->regioncount,
		in_MBs(regionstr, args->regionsize),
		in_MBs(chunkstr, args->chunksize));

	/* TODO: chunk noise */
	assert(args->chunknoise == 0);
	agvsize = args->regionsize * args->regioncount;

	find_high_low_duration(args, &highest, &lowest, &average);
	in_MBs(mbstr, agvsize *  params.TimeUnit / args->iotime);

	st->Record("%s/s\t\t%2.3fs\t%2.3fs\t%2.3fs\n",
		mbstr,
		((double)(lowest / 1000000)) / 1000.0,
		((double)(highest / 1000000)) / 1000.0,
		((double)(args->iotime / 1000000)) / 1000.0);
}

int PIOS::get_work_item(run_arg *args, pios_thdata *ptd)
{
	int i, idx, count = 0;

	i = args->next_region;
	while (count < args->regioncount) {
		idx = i % args->regioncount;

		/* test if region is fully written */
		if (args->streams[idx].offset + args->chunksize >
		    args->streams[idx].max_offset) {
			i++;
			count++;
			continue;
		}

		/* found a region that still needs work 
		 * add random stuff to chunksize */
		ptd->io.off = args->streams[idx].offset;
		ptd->io.fid = args->streams[idx].fid;
		ptd->io.count = args->chunksize;
		ptd->io.cmd = args->verify ? READ : WRITE;

		ptd->regionidx = idx;
		args->streams[idx].offset += args->chunksize;

		/* Add region noise. */
		assert(args->regionnoise == 0);
		args->next_region++;
		return 1;
	}
	return 0;
}

void PIOS::do_one_thread(void *data)
{
	ThreadLocalData *tld = (ThreadLocalData *)data;
	pios_thdata *ptd = (pios_thdata *)tld->v;
	run_arg *args = ptd->args;
	FileSystem *fs = args->fs;

	switch(tld->state) {
	case pios_ThreadStartState:
	case pios_ThreadRWState:
		if (!get_work_item(args, ptd)) {
			tld->state = pios_ThreadLastState;
			do_one_thread(tld);
			break;
		}
		
		Print(NOW "PIOS: thno %d fid %llu, off %llu, count %llu\n",
			Event::Clock(), tld->id, ptd->io.fid, ptd->io.off, ptd->io.count);
		assert(ptd->regionidx < args->regioncount);
		ptd->startime = Event::Clock();
		tld->state = pios_ThreadRWFinishState;
		fs->ReadWrite(&ptd->io);
		break;
	case pios_ThreadRWFinishState: {
		assert(ptd->regionidx < args->regioncount);
		args->streams[ptd->regionidx].iotime += Event::Clock() - ptd->startime;
		tld->state = pios_ThreadRWState;
		do_one_thread(data);
		break;
	}
	case pios_ThreadLastState:
		Print(NOW "Thread No. %d Finished.\n",
			Event::Clock(), tld->id);
		if (++args->finicount >= args->threadcount)
			ptd->parent->Signal();
		break;
	}
}

void PIOS::do_one_run(void *data)
{
	ThreadLocalData *tld = (ThreadLocalData *) data;
	run_arg *args = (run_arg *)tld->v;
	
	switch (tld->state) {
	case pios_RunOneStartState: {
		int i;

		args->startime = Event::Clock();
		Print(NOW "Start run: T %lu N %llu C %llu S %llu O %llu\n",
			args->startime, args->threadcount, args->regioncount,
			args->chunksize, args->regionsize, args->offset);

		/* Setup stream */
		args->streams = new pios_stream [args->regioncount];
		for (i = 0; i < args->regioncount; i++) {
			if (args->fpp) {
				args->streams[i].fid = i;
				args->streams[i].offset = args->offset;
				args->streams[i].max_offset = args->offset + args->regionsize;
				args->num_open_files++;
			} else {
				args->streams[i].fid = 0;
				args->streams[i].offset = args->offset * i;
				args->streams[i].max_offset = args->offset * i + args->regionsize;
			}
			args->streams[i].iotime = 0;
		}

		/* Setup pios thread data */
		args->ptds = new pios_thdata [args->threadcount];
		args->tlds = new ThreadLocalData [args->threadcount];

		memset(args->ptds, 0, sizeof(pios_thdata) * args->threadcount);
		memset(args->tlds, 0, sizeof(ThreadLocalData) * args->threadcount);
		for (i = 0; i < args->threadcount; i++) {
			Thread *t;

			t = new Thread;

			args->ptds[i].parent = tld->t;
			args->ptds[i].args = args;
			args->ptds[i].io.cmd = WRITE;
			args->ptds[i].io.count = 0;
			args->ptds[i].io.fid = 0;
			args->ptds[i].io.off = 0;
			args->ptds[i].io.waiter = t;
			args->ptds[i].io.left = 0;
			args->ptds[i].io.parent = NULL;
			args->ptds[i].io.ref = 0;

			args->tlds[i].t = t;
			args->tlds[i].id = i;
			args->tlds[i].f = do_one_thread;
			args->tlds[i].v = &args->ptds[i];
			args->tlds[i].flags |= TLD_CTX_SWITCH;
			t->CreateThread(do_one_thread, &args->tlds[i]);

			t->RunAfter(/*Event::Rand(args->threadcount * 10)*/1);
		}
		tld->state = pios_RunOneAllFinishState;
		break;
	}
	case pios_RunOneAllFinishState:
		args->iotime = Event::Clock() - args->startime;
		print_stats(args);
	case pios_RunOneLastState:
		/* cleanup all resource. */
		delete [] args->streams;
		delete [] args->ptds;
		delete [] args->tlds;
		break;
	}
}

void PIOS:: OneRun(run_arg *args)
{
	Thread *t;
	ThreadLocalData *tld;

	args->fs = new FileSystem;
	t = new Thread;
	tld = new ThreadLocalData;
	memset(tld, 0, sizeof(*tld));
	tld->v = args;
	tld->t = t;

	Event::SetClock(0);

	args->fs->Start();
	t->CreateThread(do_one_run, tld);
	t->RunAfter(1);

	Event::Schedule();
	
	delete args->fs;
	delete tld;
	delete t;
}

void PIOS::Run()
{
	struct pios_params params;
	struct run_arg args;
	int runno = 0;
	Stat *st;
	__u32 T;
	__u64 C;
	__u64 O;
	__u64 S;
	__u64 N;

	memset(&params, 0, sizeof(params));

	params.factor = 2;
	params.fpp = 1;
	params.C[0] = /*8 * 1048576; //*/512 * 1024;
	params.C[1] = /*8 * 1048576; //*/512 * 1024;
	params.O[0] = 8 * 1048576;
	params.O[1] = 8 * 1048576;
	params.S[0] = 8 * 1048576;
	params.S[1] = 8 * 1048576;
	params.N[0] = 1;
	params.N[1] = 128;
	params.T[0] = 1;
	params.T[1] = 16;

	st = new Stat(/*"pios.res"*/stdout);

	st->Record("Run\tTest\tTstamp\t\tT\tN\tS\tC\t%s"
               "Aggregate\tLowest\tHighest\tRun_time\n",
               params.cpuload ? "CPUload\t" : "");
	
        st->Record("-------------------------------------------------------"
               "----------------------------------------------------\n");

	for (T = params.T[0]; T <= params.T[1]; T *= params.factor) {
		for (C = params.C[0]; C <= params.C[1]; C *= params.factor) {
			for (S = params.S[0]; S <= params.S[1]; S *= params.factor) {
				if (S < C)
					printf("Error: chunksize (%llu) can not be"
						"smaller than regionsize (%llu)\n", S, C);
				for (N = params.N[0]; N <= params.N[1]; N *= params.factor) {
					for (O = params.O[0]; O <= params.O[1]; O *= params.factor) {
						BUG_ON(O < S || O < C);
						memset(&args, 0, sizeof(args));
						args.runno = runno;
						args.threadcount = T;
						args.regioncount = N;
						args.chunksize = C;
						args.regionsize = S;
						args.offset = O;
						args.st = st;
						args.fpp = params.fpp;

						OneRun(&args);
						runno++;
					}
				}
			}
		}
	}

	st->Record("------------------------------------------------------"
               "---------------------------------------------------\n");
	delete st;
}
