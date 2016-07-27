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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <lustre.h>

simulate_params_t params = {
	thread: {
		CtxSwitchTicks: 50,
	},

	network: {
		PacketSize: 4096,
		Bandwidth: 1,
		NetLatency: 1000, /* 1 us */
		InterruptLatency: 25000,
		N: 1,
	},
	
	fs: {
		/*
		 * 0: stream allocation algorithm;
		 * 1: mballoc allocation algorithm;
		 */
        AllocALGO: 1,
		PreallocOrder: 2,
		ReservePA: 1,
		PreallocWind: 4 * 1048576ULL,
		StreamRequestSize: 64 * 1024ULL,
		AllocateBlockTime: 200000,//800000, /* 30 us */ 
        FilesPerFileSystem: 3,
	},

	disk: {
        ElvHashShift: 6,
		ElvNoop: 0,
		ElvDeadline: 1,
		ElvUnplugThreshold: 8,//4,
		ElvMaxReqSize: 4ULL * 1048576ULL, /* 4M */
		TimeUnit: 100,
		Latency: 20000, //100000,
		LatencyRandom: 6000, //30000,
		ReadBandwidth: 50, /* 2 bytes/10ns ~ 200M/s */
		WriteBandwidth: 50,
		UnplugDelay: 3000000, /* 3 ms */
        SeekTicks: 3000000, /* 5 ms */
		SeekRandom: 200000, /* 0.8 ms */
		/*
		 * 0: single Disk;
		 * 1: raid0 device;
		 */
		BlockDeviceType: 0,
		raid0: {
			DiskCount: 20,
			DiskSize: 0,
			ChunkSize: 32 * 1048576ULL,
		},
	},

	handle: {
		PingHandleTicks: 1000000ULL,
		IntervalPerPing: 25000000000ULL,
	},

	cluster: {
		MDT: 0,
		CMD: 0,
		MdtCount: 1,
		OstCount: 1,//256,//32,
        ClientCount: 1, //*1000,*/2048, //32000,
		PingON: 0,
		OstThreadCount: 64,
		MdtThreadCount: 2,
        MaxRpcsInflight: 8,
		ClientSet: 1,
		Scale: 0,
        TimeSkew: 5000,
	},

	debug: {
		Nrs: 1,
        NIC: 0,
		Elv: 0,
        Disk: 0,
		Raid0: 0,
        FS: 0,
        Client: 1,
		LOV: 0,
        OSC: 1,
        OST: 1,
		OSD: 0,
		MDC: 0,
		MDT: 0,
		Ptlrpc: 0,
        Pool: 0,
		PIOS: 0,
        Rand: 1,
	},

	stat: {
		MDTNrs: 0,
        OSTNrs: 1,
        NIC: 0,
		DiskMaxStat:5,
        DiskReqSize: 0,
        DiskBandwidth: 1,
        Timeout: 0,
	},

	test: {
		NetTestTicks: 10000000000ULL,
		NetLatency: 1,
		NetBandwidth: 0,
		NicPerformance: 0,
		Network: 0,
		NetworkNodisk: 0,
		DiskLatency: 0,
        DiskRandPerf: 1,
        DiskSeqPerf: 0,
		FsElv: 1,
		FsLatency: 0,
		FsPerformance: 0,
		ClientAT: 0,
		PingOnly: 0,
	},

	io: {
		/*
		 * ACM_SHARE 0: shared access mode;
		 * ACM_FPP 1: File Per Processor (FPP) access mode;
		 */
		Mode: ACM_FPP,
		TestRead: 0,
		WaitAllWrites: 1,
		StripeCount: 1,
		StripePattern: 0,
		StripeSize: 1048576, //64 * 1024ULL,//1024 * 1024ULL,//1048576,
        IOCountFPP: 4 * 1048576ULL,//16 * 1048576ULL,//32ULL * 1048576ULL, /* 32M */
        AgvFileSizeSF: 64 * 1000ULL, /* in MB */
		WriterCount: 8000,
		ReaderCount: 8000,
		OpenTicks: 1000000, /* 2 ms */
        XferSize: 1048576, //0, //1024 * 1024ULL,
        DirectIO: 0,
		IOR: 1,
		/* Interval time */
        Interval: 0, //50000000,//0, //18000000000ULL,
		StepSize: 16 * 1048576ULL,
        IOPS: 0,
	},

    ptlrpc: {
		ToON: 0,
		ToPoll:0,
		ToSched:0,
		AtON: 0,
		AtEpON: 0,
		AtExtra: 30,
		AtMin: 0,
		AtMax: 600,
		AtHistWnd: 40,
		AtEpMargin: 5,
		AtSubTimeWndCnt: 8,
		ObdTimeout: 50,
	},

	cc: {
		ON: 0,
		CLON: 0,
		FIX: 1,
		Dmin: 64,
		Dmax: 8192,
		Lmax: 25,
		Cmin: 1,
		Cmax: 32,
		Cbest: 8,
		CQmax: 1,
		RCC: 8,
		IOPS: 400,
	},

	nrs: {
		//ByArrivalTime: 0, /* Default algorithm used by Lustre: FCFS */
        algo: NRS_ALGO_FIFO,//NRS_ALGO_PRIO,//NRS_ALGO_FRR,
		Deadline: NRS_ALGO_DL_OFF,//NRS_ALGO_DL_2L,
		MDLV: 5000000000ULL, /* mandatory service time value: 5s*/
		DDLD: 5000000, /* dynamical deadline delta: 5ms */
        tbf: {
            TbfDepth: 1,
            NumJobidRule: 1,
            DumpRules:true,
            DefaultRate: 100,
            TbfStat:true,
            TbfRuleStat:true,
            MaxNumRuleStats: 100,
            Alpha:130,
            Beta:150,
            StopTime: 10000000000, // stop I/O at 10s.
            StopTicks: 10000000000, // stop time length. 10s.
            Interval: 50000000, // 50ms
            TestCase: TBF_TEST_DIFF_IOSIZE,
            Rate: { 0 },
            DepInfo: { 0 },
        },
	},

	MaxRunTicks: 0, //100000000000ULL,
	TimeUnit: 1000000000ULL, /* 1s = 10 ^ 9 tick */
	SizeUnit: 1048576ULL,
};

#include <disk.h>
#include <filesystem.h>
#include <nic.h>
#include <cluster.h>
#include <nrsrbtree.h>
#include <raid0device.h>
#include <pios.h>
#include <ldlm.h>

/**
 * @brief Configure: Parameter configuration for TBF algorithm.
 */
void Configure()
{
    params.debug.Rand = 1;

    // fs setting
    params.fs.AllocALGO = 1;

    params.cluster.OstThreadCount = 64;//128;
    params.cluster.MaxRpcsInflight = 8;

    // buffered I/O
    params.io.DirectIO = 1;
    params.io.XferSize = 1048576;//4096;//
    params.io.StripeSize = 1048576;//4096;

    params.cluster.ClientCount = 1;
    params.io.IOCountFPP = 1024 * 1048576ULL;

    params.nrs.algo = NRS_ALGO_TBF;
    params.nrs.tbf.DefaultRate = 10000;
    params.nrs.tbf.TestCase = TBF_TEST_DEPRULE_OVERLOAD;

    if (params.nrs.algo != NRS_ALGO_TBF)
        return;

    for (int i = 0; i < MAX_RULE_NUM; i++) {
        params.nrs.tbf.DepInfo[i].DepIndex = -1;
    }

    switch (params.nrs.tbf.TestCase) {
    case TBF_TEST_DIFF_IOSIZE:
        params.cluster.ClientCount = 2;
        params.io.IOCountFPP = 1024 * 1048576ULL;
        params.nrs.tbf.NumJobidRule = 3;
        params.nrs.tbf.Rate[0] =  params.nrs.tbf.DefaultRate;
        params.nrs.tbf.Rate[1] = 300;
        params.nrs.tbf.Rate[2] = 200;
        break;
    case TBF_TEST_BATCH_JOBID:
        params.cluster.ClientCount = 3;
        params.io.IOCountFPP = 1024 * 1048576ULL;
        params.nrs.tbf.NumJobidRule = 3;
        params.nrs.tbf.Rate[0] = params.nrs.tbf.DefaultRate;
        params.nrs.tbf.Rate[1] = 60;
        params.nrs.tbf.Rate[2] = 50;
        break;
    case TBF_TEST_DEPRULE_STOP:
        params.cluster.ClientCount = 2;
        params.io.IOCountFPP = 2048 * 1048576ULL;
        params.nrs.tbf.NumJobidRule = 3;
        params.nrs.tbf.Rate[0] = params.nrs.tbf.DefaultRate;
        params.nrs.tbf.Rate[1] = 60;
        params.nrs.tbf.Rate[2] = 40;
        params.nrs.tbf.Alpha = 150;
        params.nrs.tbf.Beta = 130;

        params.nrs.tbf.StopTime = 10000000000;
        params.nrs.tbf.StopTicks = 10000000000;

        /**
         * Total three rule: "default", "JOBID.0" and "JOBID.1"
         * Rule "JOBID.1" depends with "JOBID.0".
         */
        params.nrs.tbf.DepInfo[0].DepIndex = -1;
        params.nrs.tbf.DepInfo[1].DepIndex = -1;
        params.nrs.tbf.DepInfo[2].DepIndex = 1;
        params.nrs.tbf.DepInfo[2].LowerRate = 40;
        params.nrs.tbf.DepInfo[2].UpperRate = 80;
        break;
    case TBF_TEST_DEPRULE_INTEVEL:
        params.cluster.ClientCount = 2;
        params.io.IOCountFPP = 2048 * 1048576ULL;
        params.nrs.tbf.NumJobidRule = 3;
        params.nrs.tbf.Rate[0] = params.nrs.tbf.DefaultRate;
        params.nrs.tbf.Rate[1] = 60;
        params.nrs.tbf.Rate[2] = 40;
        params.nrs.tbf.Alpha = 150;
        params.nrs.tbf.Beta = 130;

        params.nrs.tbf.Interval = 50000000; //100ms
        /**
         * Total three rule: "default", "JOBID.0" and "JOBID.1"
         * Rule "JOBID.1" depends with "JOBID.0".
         */
        params.nrs.tbf.DepInfo[0].DepIndex = -1;
        params.nrs.tbf.DepInfo[1].DepIndex = -1;
        params.nrs.tbf.DepInfo[2].DepIndex = 1;
        params.nrs.tbf.DepInfo[2].LowerRate = 40;
        params.nrs.tbf.DepInfo[2].UpperRate = 80;
        break;
    case TBF_TEST_DEPRULE_OVERRATE:
        params.cluster.ClientCount = 2;
        params.io.IOCountFPP = 2048 * 1048576ULL;
        params.nrs.tbf.NumJobidRule = 3;
        params.nrs.tbf.Rate[0] = params.nrs.tbf.DefaultRate;
        params.nrs.tbf.Rate[1] = 100;
        params.nrs.tbf.Rate[2] = 40;
        params.nrs.tbf.Alpha = 150;
        params.nrs.tbf.Beta = 130;

        params.nrs.tbf.Interval = 50000000; //100ms
        /**
         * Total three rule: "default", "JOBID.0" and "JOBID.1"
         * Rule "JOBID.1" depends with "JOBID.0".
         */
        params.nrs.tbf.DepInfo[0].DepIndex = -1;
        params.nrs.tbf.DepInfo[1].DepIndex = -1;
        params.nrs.tbf.DepInfo[2].DepIndex = 1;
        params.nrs.tbf.DepInfo[2].LowerRate = 40;
        params.nrs.tbf.DepInfo[2].UpperRate = 80;
        break;
    case TBF_TEST_DEPRULE_OVERLOAD:
        params.cluster.ClientCount = 100;
        params.io.IOCountFPP = 1024 * 1048576ULL;
        params.nrs.tbf.NumJobidRule = params.cluster.ClientCount + 1;
        for (int i = 0; i < MAX_RULE_NUM; i++) {
            params.nrs.tbf.Rate[i] = 50;
        }
        params.nrs.tbf.Rate[0] = params.nrs.tbf.DefaultRate;
        //params.nrs.tbf.Rate[1] = 20;
        //params.nrs.tbf.Rate[2] = 40;
        params.nrs.tbf.Alpha = 150;
        params.nrs.tbf.Beta = 130;

        //params.nrs.tbf.Interval = 50000000; //100ms
        /**
         * Total rules: "default", "JOBID.0" and "JOBID.1"..."JOBID.100"
         * Rule "JOBID.1" depends with "JOBID.0".
         */
        //params.nrs.tbf.DepInfo[0].DepIndex = -1;
        //params.nrs.tbf.DepInfo[1].DepIndex = -1;
        params.nrs.tbf.DepInfo[2].DepIndex = 1;
        params.nrs.tbf.DepInfo[2].LowerRate = 40;
        params.nrs.tbf.DepInfo[2].UpperRate = 80;
        break;
    case TBF_TEST_2JOBID_DIRECTIO:
        params.cluster.ClientCount = 2;
        params.io.IOCountFPP = 1024 * 1048576ULL;
        params.nrs.tbf.NumJobidRule = 3;
        params.nrs.tbf.Rate[0] = params.nrs.tbf.DefaultRate;
        params.nrs.tbf.Rate[1] = 60;
        params.nrs.tbf.Rate[2] = 40;
        break;
    default:
        break;
    }



    //params.MaxRunTicks = 20000000000ULL;
}

int main(int argc, char *argv[])
{
	Cluster lustre("lustre");

    //Disk::SelfBenchmark();
	//NIC::SelfBenchmark();
    //NrsRbtree::SelfTest();
    //FileSystem::SelfTest();
	//Raid0Device::SelfTest();
    //PIOS::Run();
	//LDLM::ldlm_test();

    Configure();
    lustre.Start();
	return EXIT_SUCCESS;
}
