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
#include "client.h"
#include "lov.h"
#include "mdc.h"

int Client::finished;
List Client::rwQ;
int Client::num;
int Client::waitnr;
clivec_t Client::vec;

cfs_time_t Client::d1 = 5000000000ULL;
cfs_time_t Client::d2 = 20000000000ULL;
//Stat *Client::dst1;
//Stat *Client::dst2;
int Client::d1nr = 200;
rpc_stat *Client::rst;
obd_count Client::noisesize;
time_stat *Client::iopst;
Stat *Client::rccst;

#define RW_READ 1
#define RW_WRITE 2
#define RW_WRITE_FROM_LOCK_CB 4
#define RW_IOPS 8

Client::Client()
	: Ptlrpc()//Node()
{
	id = num++;
	sprintf(name, "Client%d", id);
	vec.push_back(this);
	__dbg = params.debug.Client;
	__type = NODE_CLIENT;

	dt = NULL;
	mt = NULL;
	set = NULL;
    state = oldstate = user_FirstState;
    rw.stopticks = 0;
    rw.stoptime = 0;
	rw.rbytes = rw.wbytes = 0;
	rw.rticks = rw.wticks = 0;
	rw.rtime = rw.wtime = 0;
	rw.wperf = rw.rperf = 0;
	rw.launch = 0;
	rw.noise = 0;
	rw.stepsize = 0;
	rw.xfersize = params.io.XferSize;
	rw.iosize = 0;
	rw.type = 0;
	rw.interval = params.io.Interval;
	rw.max_runtime = START_TIME;

	rw.io.deadline = 0;
    memset(jobid, 0, LUSTRE_JOBID_SIZE);
}


Client::~Client()
{
	if (mt) {
		mt->Disconnect();
		mt->Cleanup();
		delete mt;
	}
	if (dt) {
		dt->Disconnect();
		dt->Cleanup();
		delete dt;
	} 
	
}

void Client::SetIoPriority()
{
	if (params.nrs.algo == NRS_ALGO_PRIO) {
		rw.io.flags |= IO_FLG_PRIO;
		if (id < 300) {
			rw.io.prio = 1;
			rw.interval = params.TimeUnit / 1; /* 100ms */
			rw.xfersize = 1048576; /* 1M */

			#define LKCB_SET 10
			/* set the launch interval for lock callback. */
			//rw.launch = (id % LKCB_SET) * params.TimeUnit;
			rw.max_runtime = 90000000000ULL;
			rw.count = 256 * 1048576;
		} else {
			rw.io.prio = 0;
			rw.interval = params.TimeUnit / 10;
		}

		rw.type |= RW_IOPS;
		rw.io.flags |= IO_FLG_IOPS;
		rw.tot_ios = rw.count / params.io.StripeSize;
		rw.fini_ios = 0;
	}
}

/*
 * deadline was indicated by clinets.
 */
void Client::SetIoDeadline()
{
	if (params.nrs.Deadline == NRS_ALGO_DL_OFF)
		return;

	if (params.nrs.Deadline == NRS_ALGO_DL_2L) {
		rw.io.flags |= IO_FLG_DEADLINE;
		d1nr = 0;
		if (id < d1nr) {
			rw.io.flags |= (IO_FLG_MANDATORY_DEADLINE | IO_FLG_LOCK_CB);
			rw.io.deadline = params.nrs.MDLV;
			rw.interval = 100000000ULL; /* 100ms */
			rw.xfersize = 1048576; /* 1M */

			#define LKCB_SET 10
			/* set the launch interval for lock callback. */
			rw.launch = (id % LKCB_SET) * params.TimeUnit;
			rw.max_runtime = 90000000000ULL; /* 70s */
		} else {
			rw.io.flags |= IO_FLG_DYNAMIC_DEADLINE;
		}
	}

	if (params.nrs.Deadline == NRS_ALGO_DL_ONLY) {
		rw.io.flags &= IO_FLG_MANDATORY_DEADLINE;
		if (id <= d1nr) {
			rw.io.flags |= IO_FLG_DEADLINE_1;
			rw.io.deadline = 5000000000ULL;
		} else {
			rw.io.flags |= IO_FLG_DEADLINE_2;
			rw.io.deadline = 20000000000ULL;
		}
	}
}

void Client::SetIoCongestControl()
{
	int IOPS;

	if (!params.cc.ON || !params.io.IOPS)
		return;

	IOPS = params.cc.IOPS;
	rw.tot_ios = rw.count / params.io.StripeSize;
	rw.fini_ios = 0;
	rw.type |= RW_IOPS;
	rw.xfersize = 1048576;
	rw.io.flags |= IO_FLG_IOPS;
	rw.interval = params.TimeUnit / IOPS;
}

/*
 * Divide the client into 8 sets (each 1000 clients)
 * with different I/O step size and I/O interval.
 */
#define SET_NUM 16
void Client::SetIoParams()
{
	int i;

	SetIoCongestControl();
	SetIoDeadline();
	SetIoPriority();
	if (!params.test.ClientAT)
		return;

	/* only test write for Adaptive timeout */
	LASSERT(params.io.TestRead == 0 && params.io.Mode == ACM_FPP);

//#if 0
	/* Peak simulation */
	if (params.cluster.ClientCount == 32000) {
		i = id % SET_NUM;
		rw.launch = i * 5 * params.TimeUnit;
	}
//#endif
	/* Scheduling according to deadline of RPC. */ 
	if (params.cluster.ClientCount == 48000) {
		if (id <= 32000) {
			i = id % SET_NUM;
			rw.launch = i * 5 * params.TimeUnit;
		} else {
			rw.launch = (200 + (id - 32000) / 2000 * 5) * params.TimeUnit;
			rw.noise = 8 * params.TimeUnit;
			rw.io.off = 0;
			rw.io.count = 1048576; /* 1M, generate noise */
		}
	}

#if 0
	//rw.launch = (id % 400) * IO_RTT;
	switch (i) {
	case 0:
		rw.launch = 0 ;
		rw.stepsize = 1048576;
		rw.interval = 5 * params.TimeUnit;
		break;
	case 1:
		rw.launch = 5 * params.TimeUnit;
		rw.stepsize = 1048576;
		rw.interval = 20 * params.TimeUnit;
		break;
	case 2:
		rw.launch = 10 * params.TimeUnit;
		rw.stepsize = 1048576;
		rw.interval = 20 * params.TimeUnit;
		break;
	case 3: 
		rw.launch = 15 * params.TimeUnit;
		rw.stepsize = 1048576;
		rw.interval = 30 * params.TimeUnit;
		break;
	case 4:
		rw.launch = 20 * params.TimeUnit;
		rw.stepsize = 1048576;
		rw.interval = 40 * params.TimeUnit;
		break;
	case 6:
		rw.launch = 25 * params.TimeUnit;
		rw.stepsize = 2 * 1048576;
		rw.interval = 40 * params.TimeUnit;
		break;
	case 7:
		rw.launch = 30 * params.TimeUnit;
		rw.stepsize = 4 * 1048576;
		rw.interval = 40 * params.TimeUnit;
		break;
	}
	rw.stepsize = 0;
#endif
	//rw.launch = i * 5 * params.TimeUnit;
}

int Client::SetIoCongestFlag()
{
	int s = state;

	oldstate = state;
	LASSERT(s != user_IoCongestState);
	state = user_IoCongestState;
	return s;
}

void Client::ClearIoCongestFlag(int s)
{
	LASSERT(s != user_IoCongestState);
	if (s != user_WriteCompletionState || s != user_ReadCompletionState) {
		state = (userstat_t)s;
		user.Signal();
	}
}

/* Launch the I/O */
void Client::UserStateMachine()
{
	FileObject *obj = &rw.obj;
	IO *io = &rw.io;

	switch (state) {
	case user_FirstState:
	case user_OpenFileState:
		/* FIXME: The file should be created by metadata device.*/
		obj->waiter = &user;
		dt->Create(obj);
		state = user_InitRWState;

		/* Record the first open time.*/
		if (set->sotime == START_TIME)
			set->sotime = now;
		if (params.cluster.MDT) {
			mt->Open(obj);
			break;
		}

	case user_InitRWState:	
		io->obj = obj;
		io->waiter = &user;
		io->ref = 0;
		io->fid = obj->GetId();
		io->flags = 0;

		if (params.io.Mode == ACM_SHARE && params.io.TestRead &&
		    set->rdnr > set->wtnr && id >= set->wtnr) {
			state = user_ReadStartState;
			assert(params.io.WaitAllWrites);
			user.Insert(&rwQ);
			waitnr++;
			break;
		}

		if (params.io.Mode == ACM_FPP) {
			io->off = 0;
            io->count = params.io.IOCountFPP;
		} else {
			obd_count step;

			step = set->fsz / set->wtnr;
			io->count = step * params.SizeUnit;
			io->off = step * id * params.SizeUnit;
			if (id == params.io.WriterCount - 1)
				io->count += (set->fsz % set->wtnr) * params.SizeUnit;
            //io->count;
		}

		rw.off = io->off;
		rw.count = io->count;
		rw.wbytes = io->count;
        //SetIoParams();

	case user_WriteStartState:
		rw.type |= RW_WRITE;
		io->cmd = WRITE;

		/*
		 * Record the time first client
		 * starts write.
		 */
		if (set->swtime == START_TIME) {
			set->swtime = now;
			set->opentime = now - set->sotime;
            rw.wtime = now;
		}
		if (!(rw.io.flags & IO_FLG_LOCK_CB) && 
            (params.io.DirectIO == 0 || params.io.XferSize == 0) && rw.xfersize == 0) {
			Print(NOW"%s WRITE file (fid@%llu,%llu:%llu).\n",
			      now, name, io->fid, io->off, io->count);
			state = user_WriteCompletionState;
			dt->Write(io);
			break;
		}

        Print(NOW "%s PID@%d  WRITE file (fid@%llu) mode: SYNC, xfersize: %llu\n",
            now, name, user.GetPid(), io->fid, params.io.XferSize);
		//if (params.io.Interval || rw.interval) {
		if (rw.launch) {
			state = user_WriteXferState;
			user.RunAfter(rw.launch + Rand(rw.launch / 10));
			break;
		}

	case user_WriteXferState:
		/* Direct I/O. (O_DIRECT) */
        Print(NOW "%s PID@%d WriteXfer date rw.count:%llu rw.xfersize:%llu\n",
              now, name, user.GetPid(), rw.count, rw.xfersize);
		if (rw.count != 0 && (now < rw.max_runtime)) {
			io->count = min(rw.count, rw.xfersize);
			io->off = rw.off;
			rw.count -= io->count;
			rw.off += io->count;
			rw.stepsize += io->count;

			if (rw.type & RW_IOPS) {
				if (rw.count == 0) {
					state = user_WriteCompletionState;
				} else {
					state = user_WriteIntervalState;
					dt->Write(io);
					UserStateMachine();
					break;
				}
			} else if ((params.io.Interval && rw.stepsize >= params.io.StepSize) ||
			    rw.interval != 0) {
				if (rw.count == 0) {
					state = user_WriteCompletionState;
				} else {
					rw.stepsize = 0;
					state = user_WriteIntervalState;
				}
            } else if (params.io.Interval != 0) {
                state = user_WriteIntervalState;
            } else if (rw.stopticks != 0 && now >= rw.stoptime) {
                state = user_WriteSleepState;
            }else
				state = user_WriteXferState;

            dt->Write(io);

			if (io->flags & IO_FLG_LOCK_CB)
				noisesize += io->count;
			break;
		} else {
			state = user_WriteCompletionState;
			UserStateMachine();
			break;
		}
	case user_WriteIntervalState: {
		cfs_duration_t interval;

		Print(NOW "Enter I/O interval \n", now);
		state = user_WriteXferState;
		interval = rw.interval ? : params.io.Interval;
        interval += Rand(interval / 20);
		user.RunAfter(interval);
		break;
	}
    case user_WriteSleepState: {
        Print(NOW "Enter I/O Sleep state, sleep for %llu\n",
              now, rw.stopticks);
        state = user_WriteXferState;
        user.RunAfter(rw.stopticks);
        rw.stopticks = 0;
        break;
    }
	case user_WriteCompletionState:
        Print(NOW"%s PID@%d FINI ALL write.\n", now, name, user.GetPid());
        rw.wticks = now - rw.wtime;
		LASSERT(rw.wticks != 0);
		set->writetime = now - set->swtime;
		if (!params.io.TestRead) {
			state = user_LastState;
			UserStateMachine();
			if (params.test.ClientAT && ++finished >= set->wtnr)
				over = 1;
			break;
		} else if (params.io.WaitAllWrites) {
			if (params.io.Mode == ACM_SHARE && 
		            set->wtnr > set->rdnr && id >= set->rdnr) {
				state = user_LastState;
			} else {
				state = user_ReadStartState;
				user.Insert(&rwQ);
				waitnr++;
			}
			if (id < set->wtnr && ++finished >= set->wtnr) {
				Print(NOW "Wakeup %d clients to start reading...\n", now, waitnr);
				WakeupAll(&rwQ);
			}
			rw.rtime = now;
			if (state == user_LastState)
				UserStateMachine();
			break;
		}
	case user_ReadStartState:
		if (params.io.Mode == ACM_SHARE) {
			obd_count step;

			step = set->fsz / set->rdnr;
			io->count = step * params.SizeUnit;
			io->off = step * id * params.SizeUnit;
		} else { /* Mode FPP */
			io->off = 0;
            io->count = params.io.IOCountFPP;
		}
		rw.type |= RW_READ;
		rw.rbytes = io->count;
		rw.off = io->off;
		rw.count = io->count;
		io->cmd = READ;

		if (set->srtime == START_TIME)
			set->srtime = now;
        if (params.io.DirectIO == 0 || params.io.XferSize == 0) {
			Print(NOW"%s READ file (fid@%llu, %llu:%llu) .\n",
				now, name, io->fid, io->off, io->count);
			
			state = user_ReadCompletionState;
			dt->Read(io);
			break;
		}
	
		Print(NOW "%s WRITE file (fid@%llu) mode: SYNC, xfersize: %llu\n",
			now, name, io->fid, params.io.XferSize);
	case user_ReadXferState:
		if (rw.count != 0) {
			io->count = min(rw.count, params.io.XferSize);
			io->off = rw.off;
			rw.count -= io->count;
			rw.off += io->count;
			state = user_ReadXferState;
			dt->Read(io);
			break;
		}
	case user_ReadCompletionState:
		set->readtime = now - set->srtime;
		rw.rticks = now - rw.rtime;
		Print(NOW"%s FINI all the READ.\n", now, name);
	case user_CloseFileState:
		/* TODO: implement the close metadate I/O path.*/

	case user_LastState:
		Print(NOW"%s FINI all the test.\n", now, name);
		dt->Destroy(obj);
	case user_IoCongestState:
    case user_WriteWaitAllFinishState:
		break;
	}
}

void Client::OpenStateMachine(ThreadLocalData *tld)
{
	Message *msg = tld->m;
	Thread *t = tld->t;

	switch (tld->state) {
	case cl_OpenStartState:
		msg->SetWorker(t);
		msg->SetLength(1024);
		Send(msg);
		tld->state = cl_OpenLastState;
		break;
	case cl_OpenLastState: {
		Thread *w;

		w = ((Object *)msg->req)->waiter;
		w->Signal();
		if (!(tld->flags & TLD_FROM_POOL)) {
			delete t;
			delete tld;
		}
		break;
	}	
	}
}

void Client::WriteStateMachine(ThreadLocalData *tld)
{
	Processor *p = tld->p;
	Message *msg = tld->m;
	Thread *t = tld->t;
	IO *io = (IO *) msg->req;

	switch (tld->state) {
	case cl_WriteStartState:
        if (params.nrs.algo == NRS_ALGO_TBF)
            strcpy(io->jobid, jobid);
		msg->cid = id;
		msg->SetWorker(t);
        Print(NOW "%sOSC SEND write REQ@%d FID@%llu (%llu:%llu) to %s\n",
		      now, name, msg->GetId(), io->fid, io->off, io->count, GetNodeName(msg->dst));
		msg->SetLength(1024);
		Send(msg);
		tld->state = cl_WriteRecvGetState;
		break;
	case cl_WriteRecvGetState:
        Print(NOW"%sOSC RECV bulk GET REQ@%d, tranfering bulk data.\n",
		      now, name, msg->GetId());
		msg->SetLength(io->count);
		Send(msg);
		tld->state = cl_WriteCompletionState;
		break;
	case cl_WriteCompletionState: {
        Print(NOW"%sOSC FINI write REQ@%d (%llu:%llu).\n",
		      now, name, msg->GetId(), io->off, io->count);

		StatSetPerformance(io);
	}
	case cl_WriteLastState: {
		IO *pio = (IO *) io->parent;

		RPCLastPhase(msg);
		p->TaskCompletion(tld);

		if (io->flags & IO_FLG_IOPS) {
			Client *cl = (Client *)tld->n;
			rw_t *_rw = &cl->rw;

			_rw->fini_ios++;
			if (_rw->fini_ios == _rw->tot_ios && cl->state == user_WriteCompletionState) {
				/*LASSERTF(cl->state == user_WriteCompletionState ||
					cl->state == user_LastState,
					"IO priority %d offset %llu count %llu, state %s:%s\n",
					io->prio, io->off, io->count, cl->GetCurrentUserState(),
	 				cl->GetOldUserState());*/
				pio->waiter->Signal();
			}
		} else if (--pio->ref == 0)
			pio->waiter->Signal();

		delete msg;
		delete io;
		break;
	}
	}
}

void Client::ReadStateMachine(ThreadLocalData *tld)
{
	Processor *p = tld->p;
	Message *msg = tld->m;
	IO *io = (IO *)msg->req;
	Thread *t = tld->t;

	switch (tld->state) {
	case cl_ReadStartState:
        if (params.nrs.algo == NRS_ALGO_TBF)
            strcpy(io->jobid, jobid);
		msg->cid = id;
		msg->SetWorker(t);
		Print(NOW"%s SEND read REQ@%d FID@%llu %llu:%llu to %s.\n",
		      now, name, msg->GetId(), io->fid, io->off, io->count, GetNodeName(msg->dst));
		msg->SetLength(1024);
		Send(msg);
		tld->state = cl_ReadRecvPutState;
		break;
	case cl_ReadRecvPutState:
		Print(NOW "%s RECV PUT REQ@%d, SEND PUT ACK.\n",
		      now, name, msg->GetId());
		msg->SetLength(1024);
		Send(msg);
		tld->state = cl_ReadCompletionState;
		break;
	case cl_ReadCompletionState: {
		IO *p = io->parent;
		Print(NOW "%s FINI read REQ@%d (%llu:%llu).\n",
		      now, name, msg->GetId(), io->off, io->count);

		StatSetPerformance(io);
		if (io->flags & IO_FLG_IOPS) {
			Client *cl = (Client *)tld->n;
			rw_t *_rw = &cl->rw;

			if (++_rw->fini_ios >= _rw->tot_ios) {
				LASSERT(cl->state == user_ReadCompletionState);
				p->waiter->Signal();
			}
		} else if (--p->ref == 0) {
			p->waiter->Signal();
		}
	}
	case cl_ReadLastState:
		RPCLastPhase(msg);
		p->TaskCompletion(tld);
		delete msg;
		delete io;
		break;
	}
}

void Client::UserProcess(void *arg)
{
	Client *cl = (Client *) arg;

	cl->UserStateMachine();
}

void Client::Handle(void *arg)
{
	ThreadLocalData *tld = (ThreadLocalData *) arg;
	Client *cl = (Client *) tld->n;
	Message *msg = tld->m;

	switch (msg->GetType()) {
	case MSG_OPEN:
		cl->OpenStateMachine(tld);
		break;
	case MSG_READ:
		cl->ReadStateMachine(tld);
		break;
	case MSG_WRITE:
		cl->WriteStateMachine(tld);
		break;
	}
}

int Client::GetId()
{
	return id;
}

void Client::Init(setctl *sctl)
{
	set = sctl;
}

const char *Client::GetUserState(userstat_t s)
{
	switch(s) {
	case user_FirstState:
		return "user_FirstState";
	case user_OpenFileState:
		return "user_OpenFileState";
	case user_InitRWState:
		return "user_InitRWState";
	case user_WriteStartState:
		return "user_WriteStartState";
	case user_WriteIntervalState:
		return "user_WriteIntervalState";
	case user_WriteXferState:
		return "user_WriteXferState";
	case user_WriteCompletionState:
		return "user_WriteCompletionState";
	case user_WriteWaitAllFinishState:
		return "user_WriteWaitAllFinishState";
	case user_ReadStartState:
		return "user_ReadStartState";
	case user_ReadXferState:
		return "user_ReadXferState";
	case user_ReadCompletionState:
		return "user_ReadCompletionState";
	case user_CloseFileState:
		return "user_CloseFileState";
	case user_IoCongestState:
		return "user_IoCongestState";
	case user_LastState:
		return "user_LastState";
	}

	return "user_UNKNOW";
}

const char *Client::GetCurrentUserState()
{
	return GetUserState(state);
}

const char *Client::GetOldUserState()
{
	return GetUserState(oldstate);
}

void Client::StatSetPerformance(IO *io)
{
	cfs_duration_t ticks;
	int rw = io->cmd;

	ticks = now - set->stime[rw];
	set->tot[rw] += io->count;
	set->bytes[rw] += io->count;
	if (ticks > params.TimeUnit) {
		obd_count bw;
		bw = set->bytes[rw] * params.TimeUnit / ticks;
		set->stat->Record("%llu.%09llu	%llu.%03llu\n",
		   	now / params.TimeUnit, now % params.TimeUnit,
		   	bw / params.SizeUnit, (bw % params.SizeUnit) * 1000 / params.SizeUnit);
		set->bytes[rw] = 0;
		set->stime[rw] = now;
	}
}

obd_count Client::GetBandwidth(int type)
{

	if (rw.io.flags & IO_FLG_LOCK_CB) {
		return 0;
	}
	if (type == READ && (rw.type & RW_READ)) {
		return (obd_count)((double)rw.rbytes / (double)rw.rticks * params.TimeUnit);
	} if (type == WRITE && (rw.type & RW_WRITE))
		return (obd_count) ((double)rw.wbytes / (double)rw.wticks * params.TimeUnit);

	return 0;
}

cfs_duration_t Client::GetRWTime(int type)
{
	if (rw.io.flags & IO_FLG_LOCK_CB)
		return 0;
	return type ? rw.wticks : rw.rticks;
}

setctl *Client::GetSetctl()
{
	return vec[0]->set;
}

void Client::RecordAggregateBandwidth()
{
	Stat *st;
	obd_count bw, wbw, rbw;
	obd_count agvsize, blksize;
	double iotime;
	setctl *set;

    if (params.cluster.ClientSet > 1)
		return;

    printf("Start to calculate aggregate bandwidth.\n");
	set = GetSetctl();
	assert(set->writetime != 0);
    st = new Stat("agv.bw");

	st->Record("# Parameters:\n"
		   "# client count: %d\n"
		   "# ost count: %d\n"
		   "# mdt count: %d\n"
		   "# allocator: %s\n"
		   "# time of block allocatoin: %llu\n"
		   "# disk read bandwidth: ~%llu MB/s\n"
		   "# disk write bandwidth: ~%llu MB/s\n"
		   "# disk seek time: %llu\n"
		   "# disk seek random: %llu\n"
		   "# unplug delay: %llu\n"
		   "# access mode: %s\n"
		   "# test read: %s\n"
		   "# stripe count: %lu\n"
		   "# stripe size: %llu\n"
		   "# I/O count per client: %lluM\n"
		   "# xfersize: %llu\n"
		   "# Time skew: %d\n"
		   "# nrs algo: %s\n\n",
		   params.cluster.ClientCount,
		   params.cluster.OstCount,
		   params.cluster.MdtCount,
		   params.fs.AllocALGO ? "mballoc" : "streamalloc",
		   params.fs.AllocateBlockTime,
		   params.disk.ReadBandwidth * 10,
		   params.disk.WriteBandwidth * 10,
		   params.disk.SeekTicks,
		   params.disk.SeekRandom,
		   params.disk.UnplugDelay,
		   params.io.Mode ? "FPP access mode" : "Shared access mode",
		   params.io.TestRead ? "yes" : "no",
		   params.io.StripeCount,
		   params.io.StripeSize,
           params.io.IOCountFPP / 1048576,
		   params.io.XferSize,
		   params.cluster.TimeSkew,
		   Scheduler::NrsAlgoName(params.nrs.algo));
	
	assert(set != NULL);
	if (params.io.Mode == ACM_FPP) {
        blksize = params.io.IOCountFPP;
		
		if (params.nrs.Deadline == NRS_ALGO_DL_2L)
			agvsize = blksize * (params.cluster.ClientCount -d1nr);
		else
			agvsize = blksize * params.cluster.ClientCount;
	} else {
        agvsize = params.io.AgvFileSizeSF * 1048576;
		blksize = agvsize / params.io.WriterCount;
	}

	st->Record("# IOR Summary:\n"
		   "# api\t= POSIX\n"
		   "# test filename\t= /mnt/lustre/iordata\n"
		   "# access\t= %s\n"
		   "# pattern\t= segmented (1 segment)\n"
		   "# ordering\t= seguential offsets\n"
		   "# clients\t= %d (1 per node)\n"
		   "# repetitions\t= 1\n"
		   "# xfersize\t= %llu bytes\n"
		   "# stripesize\t= %llu bytes\n"
		   "# blocksize\t= %llu MiB\n"
		   "# aggregate size\t= %llu GiB\n"
           "# noize size\t= %llu MB\n"
           "# io mode\t= %s\n\n",
		   params.io.Mode ? "file-per-process" : "shared",
		   params.cluster.ClientCount,
		   params.io.XferSize,
		   params.io.StripeSize,
		   blksize / 1048576,
		   agvsize / (1048576 * 1024ULL),
           noisesize / 1048576,
           params.io.DirectIO ? "DirectIO" : "Buffered I/O");

	iotime = (double)set->writetime / (double)params.TimeUnit;
	wbw = (obd_count) (agvsize / iotime);
	iotime = ((double)set->readtime / (double)params.TimeUnit);
	rbw = (obd_count) (agvsize / iotime);

#define BW2MB(bw) (((bw) / params.SizeUnit), (((bw) % params.SizeUnit) * 100 / params.SizeUnit))
#define TM2SEC(tm) (((tm) / params.TimeUnit), (((tm) % params.TimeUnit) * 100 / params.TimeUnit))
#define IN_KB(val) ((val) / 1024)
#define IN_MB(val) ((val) / 1048576)

	st->Record("# access\tbw(MiB/s)\tblock(KiB)\txfer(KiB)\topen(s)\twr/rd(s)\tclose(s)\titer\n"
		   "# ------\t---------\t----------\t---------\t-------\t--------\t--------\t----\n"
		   "# write\t\t%llu.%02llu\t%llu\t%llu\t%llu.%02llu\t%llu.%02llu\t\t%llu.%02llu\t\t%d\n"
		   "# read\t\t%llu.%02llu\t\t%llu\t\t%llu\t\t%llu.%02llu\t%llu.%02llu\t\t%llu.%02llu\t\t%d\n\n"
		   "# Max Write: %llu.%02llu MiB/sec\n"
		   "# Max Read: %llu.%02llu MiB/sec\n\n",
		   wbw / params.SizeUnit, (wbw % params.SizeUnit) * 100 / params.SizeUnit,
		   IN_KB(blksize), IN_KB(params.io.XferSize),
		   set->opentime / params.TimeUnit, (set->opentime % params.TimeUnit) * 100 / params.TimeUnit,
		   set->writetime / params.TimeUnit, (set->writetime % params.TimeUnit) * 100 / params.TimeUnit, 
		   set->closetime / params.TimeUnit, (set->closetime % params.TimeUnit) * 100 / params.TimeUnit, 0,
		   rbw / params.SizeUnit, (wbw % params.SizeUnit) * 100 / params.SizeUnit,
		   IN_KB(blksize), IN_KB(params.io.XferSize),
		   set->opentime / params.TimeUnit, (set->opentime % params.TimeUnit) * 100 / params.TimeUnit,
		   set->readtime / params.TimeUnit, (set->readtime % params.TimeUnit) * 100 / params.TimeUnit, 
		   set->closetime / params.TimeUnit, (set->closetime % params.TimeUnit) * 100 / params.TimeUnit, 0,
		   wbw / params.SizeUnit, (wbw % params.SizeUnit) * 100 / params.SizeUnit,
		   rbw / params.SizeUnit, (wbw % params.SizeUnit) * 100 / params.SizeUnit);

	for (int i = WRITE; i >= 0; i--) {
		bw = 0;
		for (int j = 0; j < num; j++) {
			bw += vec[j]->GetBandwidth(i);
		}
		st->Record("# Aggregate %s bandwidth: %llu.%03llu MB/sec\n",
			i ? "WRITE" : "READ", bw / params.SizeUnit, 
			(bw % params.SizeUnit) * 1000 / params.SizeUnit);
	}

	for (int i = WRITE; i >= 0; i--) {
		st->Record("# %s bandwidh: [ID, bw, used time]\n", i ? "WRITE" : "READ");
		for (int j = 0; j < num; j++) {
			bw = vec[j]->GetBandwidth(i);
            st->Record("CLIENT@%d	%llu.%03llu	%llu.%03llu\n", j,
				bw / params.SizeUnit, (bw % params.SizeUnit) * 1000 / params.SizeUnit,
				vec[j]->GetRWTime(i) / params.TimeUnit,
				vec[j]->GetRWTime(i) % params.TimeUnit / 1000000);
		}
	}

    st->Record("===============================");
	delete st;
    Stat::Output("agv.bw");
}

void Client::InitRpcStat(const char *s1, const char *s2)
{
	rst = new rpc_stat [2];
	rst[0].st.Init(s1);
	rst[1].st.Init(s2);
	for (int i = 0; i < 2; i++) {
		rst[i].nr_rpc = 0;
		rst[i].tot_rtt = 0;
	}
}

void Client::FiniRpcStat()
{
	delete [] rst;
}

void Client::InitDeadline()
{
	if (!(params.nrs.Deadline == NRS_ALGO_DL_ONLY ||
	    params.nrs.Deadline == NRS_ALGO_DL_2L))
		return;

	InitRpcStat("DDL0.st", "DDL1.st");
	for (int i = 0; i < 2; i++)
		rst[i].st.Record(" # NOW\t\tRTT\t\tST\t\tDL\t\texpire time\n");

}

#include <nrsfrr.h>
void Client::FiniDeadline()
{
	if (!(params.nrs.Deadline == NRS_ALGO_DL_ONLY ||
	    params.nrs.Deadline == NRS_ALGO_DL_2L))
		return;

	for (int i = 0; i < 2; i++) {
		cfs_duration_t avgrtt;
		rpc_stat *dl;

		dl = &rst[i];
		if (dl->nr_rpc == 0)
			continue;
		avgrtt = dl->tot_rtt / dl->nr_rpc;
		dl->st.Record("# SUMMURY:\n"
			   "# average rtt %llu.%09llu\n"
			   "# max rtt %llu.%09llu\n"
			   "# max service time %llu.%09llu\n"
			   "# dynamical deadline expired nr: %llu\n"
			   "# mandatory deadline expire nr: %llu\n",
			   avgrtt / params.TimeUnit, avgrtt % params.TimeUnit,
			   RPC::GetMaxRTT() / params.TimeUnit, RPC::GetMaxRTT() % params.TimeUnit,
			   RPC::GetMaxServTime() / params.TimeUnit, RPC::GetMaxServTime() % params.TimeUnit,
			   NrsFRR::GetDynamicalExpiredNum() / params.TimeUnit,
			   NrsFRR::GetDynamicalExpiredNum() % params.TimeUnit,
			   NrsFRR::GetMandatoryExpiredNum() / params.TimeUnit,
			   NrsFRR::GetMandatoryExpiredNum() % params.TimeUnit); 
	}
	
	FiniRpcStat();
}

void Client::StatDeadline(Message *msg)
{
	cfs_duration_t rtt, st, art;
	IO *io = (IO *) msg->req;
	rpc_stat *sd;

	if (!(params.nrs.Deadline == NRS_ALGO_DL_ONLY ||
	    params.nrs.Deadline == NRS_ALGO_DL_2L))
		return;

	if (io->flags & IO_FLG_DYNAMIC_DEADLINE)
		sd = &rst[0];
	else if (io->flags & IO_FLG_MANDATORY_DEADLINE)
		sd = &rst[1];

	rtt = msg->GetRTT();
	st = msg->GetServTime();
	art = msg->GetArrivalTime();

	sd->tot_rtt += rtt;
	sd->nr_rpc++;
	sd->st.Record("%llu.%09llu\t%llu.%09llu	%llu.%09llu\t%llu.%09llu\t%llu.%09llu\n",
		now / params.TimeUnit, now % params.TimeUnit,
		rtt / params.TimeUnit, rtt % params.TimeUnit,
		st / params.TimeUnit, st % params.TimeUnit,
		io->deadline / params.TimeUnit, io->deadline % params.TimeUnit,
		(io->deadline - art) / params.TimeUnit,
		(io->deadline - art) % params.TimeUnit
		);
}

#include "nrsprio.h"
void Client::InitPriority()
{
	if (params.nrs.algo != NRS_ALGO_PRIO)
		return;

	InitRpcStat("PL0.st", "PL1.st");
	for (int i = 0; i < 2; i++)
		rst[i].st.Record(" # NOW\t\tRTT\t\tST\t\tDL\t\texpire time\n");
}

void Client::FiniPriority()
{
	if (params.nrs.algo != NRS_ALGO_PRIO)
		return;

	FiniRpcStat();
}

void Client::StatPriority(Message *msg)
{
	cfs_duration_t rtt, st, art;
	IO *io = (IO *) msg->req;
	rpc_stat *sd;

	if (params.nrs.algo != NRS_ALGO_PRIO)
		return;

	if ( io->prio == 0)
		sd = &rst[0];
	else if (io->prio == 1)
		sd = &rst[1];

	rtt = msg->GetRTT();
	st = msg->GetServTime();
	art = msg->GetArrivalTime();

	sd->tot_rtt += rtt;
	sd->nr_rpc++;
	sd->st.Record("%llu.%09llu\t%llu.%09llu	%llu.%09llu\t%llu.%09llu\t%llu.%09llu\n",
		now / params.TimeUnit, now % params.TimeUnit,
		rtt / params.TimeUnit, rtt % params.TimeUnit,
		st / params.TimeUnit, st % params.TimeUnit,
		io->deadline / params.TimeUnit, io->deadline % params.TimeUnit,
		(io->deadline - art) / params.TimeUnit,
		(io->deadline - art) % params.TimeUnit
	);
}

void Client::InitCongestControl()
{
	if (!params.cc.ON)
		return;

	InitRpcStat("Depth.st", "CC.st");
	iopst = new time_stat;
	iopst->st.Init("IOPS.st");
	iopst->start = 0;
	iopst->count = 0;
	rccst = new Stat("RCC.st");
}

void Client::FiniCongestControl()
{
	Stat *st = GetStat(1);
	cfs_duration_t avg_rtt;

	if (!params.cc.ON)
		return;

	avg_rtt = RPC::GetAvgRTT();
	st->Record("# Average RTT %llu.%09llu\n",
		avg_rtt / params.TimeUnit,
		avg_rtt % params.TimeUnit);

	FiniRpcStat();

	delete iopst;
	delete rccst;
}

void Client::StatCongestControl(Message *msg)
{
	Stat *st = GetStat(1);
	cfs_duration_t rtt, d;

	if (!params.cc.ON || !params.cc.CLON)
		return;

	rtt = msg->GetRTT();

	st->Record("%llu.%09llu\t%llu.%09llu\t%d\n",
		now / params.TimeUnit, now % params.TimeUnit,
		rtt / params.TimeUnit, rtt % params.TimeUnit,
		msg->GetRCC());

	iopst->count++;
	d = now - iopst->start;
	if (d >= params.TimeUnit) {
		iopst->st.Record("%llu.%09llu\t%d\n", 
			now / params.TimeUnit,
			now % params.TimeUnit,
			iopst->count);
		iopst->count = 0;
		iopst->start = now;
	}
}

void Client::RPCLastPhase(Message *msg)
{
	StatDeadline(msg);
	StatPriority(msg);
	StatCongestControl(msg);
}

void Client::InitClient()
{
	InitDeadline();
	InitPriority();
	InitCongestControl();
}

void Client::FiniClient()
{
	FiniDeadline();
	FiniPriority();
	FiniCongestControl();
}

Stat *Client::GetStat(int i)
{
	return &rst[i].st;
}

Stat *Client::GetRCCStat()
{
	return rccst;
}

void Client::ConfTbfTestSuit()
{
    snprintf(jobid, LUSTRE_JOBID_SIZE,
             "JOBID.%d", id);

    switch (params.nrs.tbf.TestCase) {
    case TBF_TEST_DIFF_IOSIZE:
        /**
         * Three rules: Default rule "*", "JOBID.0", "JOBID.1"
         */
        assert(params.cluster.ClientCount == 2 &&
               params.nrs.tbf.NumJobidRule == 3);
        /**
         * CLIENT0 IO size is 4096;
         * CLIENT1 IO size is 1MB;
         */
        if (id == 0) {
            rw.xfersize = 4096;
        }
        break;
    case TBF_TEST_BATCH_JOBID:
        assert(params.cluster.ClientCount == 3 &&
               params.nrs.tbf.NumJobidRule == 3);
        /**
         * Three rules: Default rule "*", "JOBID.0", "JOBID.1"
         * CLIENT0, CLIENT1 is set to "JOBID.0"
         * CLIENT2 is set to "JOBID.1"
         */
        if (id <= 1) {
            strcpy(jobid, "JOBID.0");
        } else if (id == 2) {
            strcpy(jobid, "JOBID.1");
        }
        break;
    case TBF_TEST_DEPRULE_STOP:
        assert(params.cluster.ClientCount == 2 &&
               params.nrs.tbf.NumJobidRule == 3);
        /**
         * CLIENT1 continues to generate I/O to the server;
         * CLIENT0 First send I/O and stop for a while, and then continue.
         */
        if (id == 0) {
            rw.stoptime = params.nrs.tbf.StopTime;
            rw.stopticks = params.nrs.tbf.StopTicks;
        }
        break;
    case TBF_TEST_DEPRULE_INTEVEL:
    case TBF_TEST_DEPRULE_OVERRATE:
        assert(params.cluster.ClientCount == 2 &&
               params.nrs.tbf.NumJobidRule == 3);
        /**
         * CLIENT1 continues to generate I/O to the server;
         * CLIENT0 Send a I/O with 1MB size in @params.nrs.tbf.Interval interval.
         */
        if (id == 0) {
            rw.interval = params.nrs.tbf.Interval;
        }
    default:
        break;
    }




}

void Client::Start()
{
	Print(NOW"%s is Starting...\n", now, name);
	Ptlrpc::Start();

	/* Setup and connect to data device */
	dt = new LOV;
	dt->Attach(this);
	dt->Setup();

	/* Setup and connect to metadata Device */
	if (params.cluster.MDT) {
		mt = new MDC;
		mt->Attach(this);
		mt->Setup();
	}

	/* User I/O */
	if (!params.test.PingOnly) {
		//cfs_duration_t skew;
		
		//skew = (id % 10) ;
		user.CreateThread(UserProcess, this);
		//user.RunAfter(skew * params.TimeUnit + Rand(skew * 100) * params.TimeUnit / 1000);
        user.RunAfter(5000 + Rand(params.cluster.TimeSkew ));
	}

    if (params.nrs.algo == NRS_ALGO_TBF) {
        ConfTbfTestSuit();
    }

	handler = Handle;
}
