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
#ifndef LUSTRE_H
#define LUSTRE_H

#include <iostream>
#include <cstdlib>
#include <cassert>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>

using namespace std;

#define UNUSED(x) (void)x
#define RETURN(rc) return(rc)
#define GOTO(label, rc)	do { rc; goto label; } while(0)
#define unlikely(exp)	(exp)
#define likely(exp)	(exp)

#define do_div(a, b) \
    ({ \
        unsigned long remainder; \
        remainder = (a) % (b); \
        (a) = (a) / (b); \
        (remainder);	\
    })

/* gfp.h */
#define GFP_ATOMIC  0x20u

/* lock */
typedef struct {

} rwlock_t;
typedef struct {

} spinlock_t;

#define __acquires(x)
#define __releases(x)
#define spin_lock(x)
#define spin_unlock(x)
#define read_lock(x)
#define read_unlock(x)
#define write_lock(x)
#define write_unlock(x)
#define spin_lock_init(x)
#define rwlock_init(x)

typedef struct { volatile int counter; } atomic_t;
#define ATOMIC_INIT(i) { i }
#define atomic_read(v)	((v)->counter)
#define atomic_set(v, i)	(((v)->counter) = (i))
#define atomic_inc(a)	(((a)->counter)++)
#define atomic_dec(a)	do { (a)->counter--; } while(0)
#define atomic_dec_and_test(a) ((--((a)->counter)) == 0)
#define atomic_dec_and_lock(a, b) ((--((a)->counter)) == 0)

#define OBD_ALLOC(v, nob) do {               \
        (v) = (typeof(v))malloc(nob);                   \
	if (v != NULL)                       \
		memset(v, 0, nob);           \
} while (0)

#define OBD_FREE(v, nob) do {                \
        free((void *)v);                     \
} while (0)

#define OBD_FREE_PTR(ptr)   OBD_FREE(ptr, sizeof*(ptr))
#define OBD_ALLOC_PTR(ptr)  OBD_ALLOC(ptr, sizeof*(ptr))

#define OBD_FREE_RCU_CB(ptr, size, handle, cb)	((*(cb))(ptr, size))

#define OBD_SLAB_FREE(node, type_slab, size) OBD_FREE(node, size)

#define cfs_time_current() (Event::Clock())
#define cfs_time_current_sec() (Event::CurrentSeconds())
#define cfs_time_seconds(sec) ((cfs_duration_t)(sec) * params.TimeUnit)
#define cfs_curproc_pid() (Thread::CurrentPid())

#define OBD_FAIL_CHECK(id)	0
#define OBD_FAIL_TIMEOUT(id, c)	0

#define LPU64 "%Lu"
#define LPX64 "%#lx"

#define CFS_INIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while(0)

#define CFS_LIST_HEAD_INIT(n)	LIST_HEAD_INIT(n)

#define LIBCFS_ALLOC(v, nob) do {               \
        (v) = (typeof(v))calloc(nob, 1);           \
} while (0)

#define LIBCFS_FREE(v, nob) do {                \
        free((void *)v);                        \
} while (0)

#define LIBCFS_ALLOC_ATOMIC(ptr, size)  LIBCFS_ALLOC(ptr, size)
#define LIBCFS_CPT_ALLOC_GFP(ptr, cptab, cpt, size, mask)   LIBCFS_ALLOC(ptr, size)
#define LIBCFS_CPT_ALLOC(ptr, cptab, cpt, size) LIBCFS_ALLOC(ptr, size)

#define ENTRY	do {} while(0)
#define EXIT	do {} while(0)

#define OBD_OBJECT_EOF 0xffffffffffffffffULL

#if 0
#define LASSERT(e)
#define LASSERTF(cond, fmt, arg...)
#define BUG_ON(e)
#define LBUG()
#else
#define LASSERT(e) do {                                         \
        if (!(e)) {                                             \
                printf("Assertion (%s) failed at %s:%d: %s\n",  \
                       #e, __FILE__, __LINE__, __FUNCTION__);   \
                abort();                                        \
        }                                                       \
} while (0)

#define LASSERTF(cond, fmt, arg...) do {                        \
    if (!(cond))                                            \
        printf("Assertion Failed ("#cond") "            \
               "at %s:%d: %s - " fmt, __FILE__, __LINE__,\
               __FUNCTION__, ##arg);                    \
    assert(cond);                                           \
} while(0)

#define BUG_ON(e) do {                                          \
	if (e) {                                                \
		printf("Bug on (%s) at %s:%d\n",                \
                       #e, __FILE__, __LINE__);                 \
                abort();                                        \
        }                                                       \
} while (0)

#define LBUG()	assert(0)
#endif

#define ergo(a, b)  (!(a) || (b))
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr: the pointer to the member.
 * @type: the type of the container struct this is embedded in.
 * @member: the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({      \
    const decltype( ((type *)0)->member ) *__mptr = (ptr); \
    (type *)( (char *)__mptr - offsetof(type,member) );})

#define MAX_ERRNO   4095
#define IS_ERR(a)   ((unsigned long)(a) > (unsigned long)-MAX_ERRNO)
#define ERR_CAST(ptr)   ((void *)ptr)

/* container_of depends on "likely" which is defined in libcfs_private.h */
static inline void *__container_of(const void *ptr, unsigned long shift)
{
        if (unlikely(IS_ERR(ptr) || ptr == NULL))
                return ERR_CAST(ptr);
        else
                return (char *)ptr - shift;
}

#define container_of0(ptr, type, member)                                \
        ((type *)__container_of((ptr), offsetof(type, member)))

#define EXPORT_SYMBOL(s)
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef uint64_t cfs_time_t;
typedef uint64_t cfs_duration_t;
typedef uint64_t obd_count;
typedef uint64_t obd_id;
typedef	uint64_t obd_off;
typedef uint64_t bandwidth_t;
typedef unsigned char __u8;

struct lustre_handle {
	__u64 cookie;
};

static inline cfs_time_t cfs_time_add(cfs_time_t t, cfs_duration_t d)
{
	return t + d;
}

static inline int cfs_time_before(cfs_time_t t1, cfs_time_t t2)
{
	return (int64_t)t1 - (int64_t)t2 < 0;
}

#define MAX_NAME_LEN 32

struct simulate_params_t
{
	struct thread_params_t {
		const cfs_duration_t CtxSwitchTicks;
	} thread;

	struct network_params_t {
		const obd_count PacketSize;
		const obd_count Bandwidth;
		const cfs_duration_t NetLatency;
		const cfs_duration_t InterruptLatency;
		const int N;
	} network;
	
	struct fs_params_t {
        int AllocALGO;
        int PreallocOrder;
        int ReservePA;
        obd_count PreallocWind;
        obd_count StreamRequestSize;
        cfs_duration_t AllocateBlockTime;
        cfs_duration_t FilesPerFileSystem;
	} fs;

	struct disk_params_t {
		const int ElvHashShift;
		const int ElvNoop;
		const int ElvDeadline;
		const int ElvUnplugThreshold;
		const obd_count ElvMaxReqSize;
		const cfs_duration_t TimeUnit;
		const cfs_duration_t Latency;
		const cfs_duration_t LatencyRandom;
		const obd_count ReadBandwidth;
		const obd_count WriteBandwidth;
		const cfs_duration_t UnplugDelay;
		const cfs_duration_t SeekTicks;
		const cfs_duration_t SeekRandom;
		const int BlockDeviceType;
		struct raid0_params_t {
			const int DiskCount;
			const obd_count DiskSize;
			const unsigned long ChunkSize;
		} raid0;
	} disk;

	struct handle_params_t {
		const cfs_duration_t PingHandleTicks;
		const cfs_duration_t IntervalPerPing;
	} handle;

	struct cluster_params_t {
        int MDT; /* Support metadata I/O path */
        int CMD; /* Cluster Metadata Server */
        int MdtCount;
        int OstCount;
        int ClientCount;
        int PingON;
        int OstThreadCount;
        int MdtThreadCount;
        int MaxRpcsInflight;
        int ClientSet;
        int Scale;
        __u64 TimeSkew;
	} cluster;

	struct debug_params_t {
		const int Nrs;
		const int NIC;
		const int Elv;
		const int Disk;
		const int Raid0;
		const int FS;
		const int Client;
		const int LOV;
		const int OSC;
		const int OST;
		const int OSD;
		const int MDC;
		const int MDT;
		const int Ptlrpc;
		const int Pool; /* thread pool */
		const int PIOS;
        int Rand;
	} debug;

	struct stat_params_t {
		const int MDTNrs;
		const int OSTNrs;
		const int NIC;
		const int DiskMaxStat;
		const int DiskReqSize;
		const int DiskBandwidth;
		const int Timeout;
	} stat;

	struct test_params_t {
		const cfs_duration_t NetTestTicks;
		const int NetLatency;
		const int NetBandwidth;
		const int NicPerformance;
		const int Network;
		const int NetworkNodisk;
		const int DiskLatency;
		const int DiskRandPerf;
		const int DiskSeqPerf;
		const int FsElv;
		const int FsLatency;
		const int FsPerformance;
		const int ClientAT;
		const int PingOnly;
	} test;

	struct io_params_t {
		/* Access Mode */
		#define ACM_SHARE 0
		#define ACM_FPP 1
        int Mode;
        int TestRead;
        int WaitAllWrites;
        uint32_t StripeCount;
        uint32_t StripePattern;
        obd_count StripeSize;
        obd_count IOCountFPP;
        obd_count AgvFileSizeSF;
        int WriterCount;
        int ReaderCount;
        cfs_duration_t OpenTicks;
        obd_count XferSize;
        int DirectIO;
        int IOR; /* whether user IOR model to compute the bandwidth */
		/* I/O interval */
        cfs_duration_t Interval;
        obd_count StepSize;
        int IOPS;
	} io;

	struct ptlrpc_params_t {
		const int ToON; /* Timeout on */
		/* 
		 * Polling based mechanims for improved RPC timeout handling.
		 */
		const int ToPoll;
		const int ToSched;
		const int AtON;
		const int AtEpON;
		const unsigned int AtExtra;
		const unsigned int AtMin;
		const unsigned int AtMax;
		const unsigned int AtHistWnd;
		const unsigned int AtEpMargin;
		const int AtSubTimeWndCnt;
		const unsigned int ObdTimeout;
	} ptlrpc;

	/* Congestion control parameters */
	struct cc_params_t {
		const int ON;
		const int CLON;
		const int FIX;
		const int Dmin;
		const int Dmax;
		const int Lmax; /* in second */
		const int Cmin; /* min RPC concurrent credits */
		const int Cmax; /* max RPC concurrent credits */
		const int Cbest; /* best RPC concurrent credits */
		const int CQmax; /* max client queue depth */
		const int RCC;  /* default RPC concurrent credits */ 
		const int IOPS;
		
	} cc;

#define NRS_ALGO_FIFO 1
#define NRS_ALGO_BINHEAP 2
#define NRS_ALGO_RBTREE 3
#define NRS_ALGO_EPOCH 4
#define NRS_ALGO_FRR 5
#define NRS_ALGO_BYOBJID 6
#define NRS_ALGO_BYDEADLINE 7
#define NRS_ALGO_BYKEY  8
#define NRS_ALGO_FCFS   9
#define NRS_ALGO_DL_ONLY    10
#define NRS_ALGO_DL_OFF 11
#define NRS_ALGO_DL_2L  12
#define NRS_ALGO_PRIO   13
#define NRS_ALGO_TBF    14
	struct nrs_params_t {
		//const int ByArrivalTime;
		//const int ByKeyVaule; /* used by round-robin algorithm */
		//const int GreedByObjid; /* design to mballoc algorithm */
		//const int ByDeadline;
		//const int AlgoEpoch;
        int algo;
		const int Deadline;
		const cfs_duration_t MDLV;
		const cfs_duration_t DDLD;
        // parameters for NRS TBF
        struct nrs_tbf_prarms_t {
            int TbfDepth;
            int NumJobidRule;
            bool DumpRules;
            __u64 DefaultRate;
            bool TbfStat;
            bool TbfRuleStat;
            int MaxNumRuleStats;
            // parameters for rules with dependency.
            __u32 Alpha;
            __u32 Beta;
            // parameters for SEND/STOP/CONTINUE I/O generator.
            __u64 StopTime;
            __u64 StopTicks;
            /** Parameters for Interval I/O. */
            __u64 Interval;
#define TBF_TEST_SINGLE_STREAM  0
#define TBF_TEST_DIFF_IOSIZE    1
#define TBF_TEST_BATCH_JOBID    2
#define TBF_TEST_DEPRULE_STOP   3
#define TBF_TEST_DEPRULE_INTEVEL    4
#define TBF_TEST_DEPRULE_SCALE  5
#define TBF_TEST_DEPRULE_OVERRATE   6
#define TBF_TEST_DEPRULE_OVERLOAD   7
#define TBF_TEST_2JOBID_DIRECTIO    8
            int TestCase;
#define MAX_RULE_NUM 1001
            int Rate[MAX_RULE_NUM];
            struct nrs_tbf_deprule_t {
                int DepIndex;
                __u64 UpperRate;
                __u64 LowerRate;
            } DepInfo[MAX_RULE_NUM];
        } tbf;
	} nrs;

    cfs_duration_t MaxRunTicks;
	const cfs_time_t TimeUnit;
	const obd_count SizeUnit;


};

extern simulate_params_t params;

typedef int (*compare_f)(void *a, void *b);

#define READ	0
#define WRITE	1

#define NOW "At %llu	"
#endif /*LUSTRE_H*/
