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
#ifndef CLIENT_H
#define CLIENT_H

//#include <node.h>
#include <ptlrpc.h>
#include <obd.h>

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/

#define START_TIME	~(0ULL)
struct setctl
{
	int setnr; /* number of sets */
	int type;
	int c;

	/*used by client component */

	/* shared access mode */
	obd_count fsz;
	obd_id fid;
	int rdnr;
	int wtnr;

	/* used by IOR statistics */
	cfs_time_t swtime;
	cfs_time_t srtime;
	cfs_time_t sotime;
	cfs_time_t sctime;
	cfs_duration_t writetime;
	cfs_duration_t readtime;	
	cfs_duration_t opentime;
	cfs_duration_t closetime;

	cfs_time_t stime[2];
	obd_count bytes[2];
	//obd_count bw[2];
	obd_count tot[2];
	Stat *stat;
};

struct rpc_stat {
	Stat st;
	cfs_time_t tot_rtt;
	obd_count nr_rpc;
};

struct time_stat {
	Stat st;
	int count;
	cfs_time_t start;
};

class Client;
typedef vector<Client *> clivec_t;
class Client : public Ptlrpc//Node
{
	static int finished;
	static int waitnr;
	static List rwQ;
	static int num;
	static clivec_t vec;

	/*
	 * mandatory deadline.
	 */
	static rpc_stat *rst;
	static time_stat *iopst;
	static Stat *ccst;
	static Stat *rccst;
	static cfs_time_t d1;
	static cfs_time_t d2;
	static int d1nr;
	static obd_count noisesize; /* noise I/O size resulting from lockcallback.*/

	int id;
	DataDevice *dt;
	MetadataDevice *mt;
    char jobid[LUSTRE_JOBID_SIZE];

	struct rw_t
	{
		int type;
		int rw;
		int prio;
		IO io;
		FileObject obj;
		Request req;

		/* I/O information */
		obd_off off;
		obd_count count;

		/* stat information. */
		cfs_time_t rtime; /* time when start reading */
		cfs_time_t wtime; /* time when start writing */
		cfs_duration_t rticks;
		cfs_duration_t wticks;
		obd_count rbytes;
		obd_count wbytes;
		obd_count rperf;
		obd_count wperf;

		/* I/O interval parameters */
		obd_count stepsize;
		obd_count xfersize;
		obd_count iosize;
		cfs_duration_t interval;
		cfs_time_t launch;
		cfs_time_t noise; /* launch time noise */
		cfs_time_t max_runtime;

		/* IOPS */
		int tot_ios;
		int fini_ios;

        /**
         * I/O mode: SEND/STOP/CONTINUE
         */
        cfs_time_t stoptime;
        cfs_duration_t stopticks;
	};

	rw_t rw;
	Thread user;

	setctl *set;

	/* Application state machine */
	enum userstat_t{
		user_FirstState,
		user_OpenFileState,
		user_InitRWState,
		user_WriteStartState,
        user_WriteSleepState,
		user_WriteIntervalState,
		user_WriteXferState,
		user_WriteCompletionState,
		user_WriteWaitAllFinishState,
		user_ReadStartState,
		user_ReadXferState,
		user_ReadCompletionState,
		user_CloseFileState,
		user_IoCongestState,
		user_LastState,
	} state, oldstate;

	enum {
		cl_OpenStartState,
		cl_OpenLastState,
	};
	enum {
		cl_WriteStartState,
		cl_WriteRecvGetState,
		cl_WriteCompletionState,
		cl_WriteLastState,
	};
	enum {
		cl_ReadStartState,
		cl_ReadRecvPutState,
		cl_ReadCompletionState,
		cl_ReadLastState,
	};
	
	void OpenStateMachine(ThreadLocalData *tld);
	void ReadStateMachine(ThreadLocalData *tld);
	void WriteStateMachine(ThreadLocalData *tld);
	void UserStateMachine();
	
    const char *GetCurrentUserState();
    const char *GetOldUserState();

    static const char *GetUserState(userstat_t s);
	static void UserProcess(void *arg);
	static void Handle(void *arg);

	/* statistics */
	void StatSetPerformance(IO *io);
	void SetIoParams();
	void SetIoDeadline();
	void SetIoPriority();
	void SetIoCongestControl();

    static void InitRpcStat(const char *s1, const char *s2);
	static void FiniRpcStat();

public:
	Client();
	~Client();

	int GetId();
	void Init(setctl *sctl);
	void Start();

    void ConfTbfTestSuit();
	obd_count GetBandwidth(int rw);
	cfs_duration_t GetRWTime(int rw);

	void RPCLastPhase(Message *msg);
	int SetIoCongestFlag();
	void ClearIoCongestFlag(int state);

	static Stat *GetStat(int i);
	static Stat *GetRCCStat();
	static setctl *GetSetctl();
	static void RecordAggregateBandwidth();

	static void InitDeadline();
	static void StatDeadline(Message *msg);
	static void FiniDeadline();

	static void InitPriority();
	static void StatPriority(Message *msg);
	static void FiniPriority();

	static void InitCongestControl();
	static void StatCongestControl(Message *msg);
	static void FiniCongestControl();

	static void InitClient();
	static void FiniClient();
};

#endif
