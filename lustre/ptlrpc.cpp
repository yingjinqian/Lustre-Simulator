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
#include "ptlrpc.h"
#include "nrsrbtree.h"

#define AET_INVALID_TIME	~(0ULL)

unsigned int Ptlrpc::at_epmargin = params.ptlrpc.AtEpMargin;
unsigned int Ptlrpc::at_min = max(params.ptlrpc.AtMin, max((unsigned int)30, at_epmargin + 4));
unsigned int Ptlrpc::at_max = params.ptlrpc.AtMax;
unsigned int Ptlrpc::at_history = params.ptlrpc.AtHistWnd;
unsigned int Ptlrpc::at_subwndcnt = AT_WIND_CNT;
unsigned int Ptlrpc::at_extra = params.ptlrpc.AtExtra;
unsigned int Ptlrpc::at_poll_intv = 25;
unsigned int Ptlrpc::at_sched_base = 30;
unsigned int Ptlrpc::at_sched_rand = 40;
int Ptlrpc::at_srv_algo = AT_FLG_CURFIT;//AT_FLG_AET;
			 /*AT_FLG_HYBRID; *//*AT_FLG_CURFIT;*//*AT_FLG_MAX;*/
			 /*AT_FLG_QUEUED_RPC;*/ /*AT_FLG_FIXED;*/ /*AT_FLG_FIRST;*/
int Ptlrpc::at_cli_algo = AT_FLG_MAX; /*AT_FLG_FIRST;*/

#define INITIAL_CONNECT_TIMEOUT 50
#define RPCS_PER_SEC 200 /* 10ms */

#define min_t(type, x,y) (type)(x) < (type)(y) ? (x) : (y)
#define max_t(type, x, y) (type)(x) < (type)(y) ? (y) : (x)

#define TO_ON (params.ptlrpc.ToON)
#define AT_ON (TO_ON && params.ptlrpc.AtON && at_max != 0)
#define ATEP_ON (AT_ON && params.ptlrpc.AtEpON)
#define POLL_ON (params.ptlrpc.ToPoll)
#define SCHED_ON (params.ptlrpc.ToSched)

#define RPCID 0
#define RPC_DEBUG(rpc) do {                                       \
	printf(NOW"RPC %p:%d deadline %llu send_time %llu "       \
	       "arrival_time %llu handle_time %llu "              \
	       "service_time %llu execute_time %llu "             \
	       "timedout %d EP_timed %d EP_count %d, "            \
	       "msg %p phase %s\n ",                              \
	       now, rpc, rpc->GetId(), rpc->deadline,             \
	       rpc->sendTime,                                     \
	       rpc->arrivalTime, rpc->handleTime, rpc->servTime,  \
	       rpc->workTime, rpc->timedout, rpc->erp_timed,      \
	       rpc->erpcnt, rpc->msg, rpc->msg->GetPhase());      \
} while(0)

#define RPC_TRACE(rpc, id) do {                                    \
	if (id && rpc->GetId() == id) {                            \
		printf("%s:%d ", __FUNCTION__, __LINE__);          \
		RPC_DEBUG(rpc);                                    \
	}                                                          \
} while(0)

#define LASSERTF_RPC(cond, rpc) do {                               \
	LASSERTF((cond),"\n" NOW"RPC %p:%d deadline %llu "         \
		 "send_time %llu "                                 \
		 "arrival_time %llu handle_time %llu "             \
		 "service_time %llu execute_time %llu "            \
		 "timedout %d EP_timed %d EP_count %d, "           \
		 "phase %s\n",                                     \
		 now, rpc, rpc->GetId(), rpc->deadline,            \
		 rpc->sendTime,                                    \
		 rpc->arrivalTime, rpc->handleTime, rpc->servTime, \
		 rpc->workTime, rpc->timedout, rpc->erp_timed,     \
		 rpc->erpcnt, rpc->msg->GetPhase());               \
} while(0)

Ptlrpc::Ptlrpc()
 : Node()
{
	expire = 0;
	EST = NULL;
	epTimer = NULL;
	epQ = NULL;
	rpcnt = 0;
	ccDe = params.cc.Dmax;
	ccC = params.cluster.ClientCount;
	ccst = NULL;
	cwnd = NULL;
	iosize = 0;
}

Ptlrpc::~Ptlrpc()
{
	if (EST)
		delete EST;
	if (epQ)
		delete epQ;
	if (epTimer) {
		epTimer->DelTimer();
		delete epTimer;
	}
	if (ccst)
		delete ccst;

	if (cwnd)
		delete cwnd;
}

void Ptlrpc::PtlPrint(const char *fmt...)
{
	if (params.debug.Ptlrpc) {
		va_list args;

		va_start(args,fmt);
		vfprintf(stdout,fmt, args);
		va_end(args);
	}
}

/* 
 * Least squares curve fitting:
 * y = a0 + a1x
 */
void Ptlrpc::CurveFitting(adaptive_timeout *at)
{
	double X[AT_WIND_CNT];
	double Y[AT_WIND_CNT];
	double sum_xy = 0;
	double sum_x2 = 0;
	double sum_x = 0;
	double sum_y = 0;
	double avg_x;
	double avg_y;
	double a0;
	double a1;
	int i, n = 0;

	for (i = 0; i < AT_WIND_CNT; i++){
		//if (at->at_hist[i] != 0) {
			n++;
			X[i] = at->at_time[i];
			Y[i] = at->at_hist[i];
		//}
	}

	for (i = 0; i < n; i++) {
		sum_xy += X[i] * Y[i];
		sum_x += X[i];
		sum_y += Y[i];
		sum_x2 += X[i] * X[i];
	}

	avg_x = sum_x / n;
	avg_y = sum_y /n;
	
	a1 = (sum_xy - n * avg_x * avg_y) / (sum_x2 - n * avg_x * avg_x);
	a0 = avg_y - a1 * avg_x;

	at->a0 = a0;
	at->a1 = a1;
	/* y = a0 + a1 * x */
	at->at_estimate = (unsigned int)(a0 + a1 * (double)(now / params.TimeUnit));
}

void Ptlrpc::at_init(adaptive_timeout *at, int val, int flags)
{
	memset(at, 0, sizeof(*at));
	at->at_max = val;
	at->at_estimate = val;
	at->at_flags = flags;
}


cfs_duration_t Ptlrpc::at_get_aet(adaptive_timeout *at)
{
	LASSERT(at->at_flags & AT_FLG_AET);

	return at->at_max_aet > 0 ? at->at_max_aet : at->at_stw[0].aet;
}

/*
 * @val: time value.
 * @tm: arrival time.
 */
int Ptlrpc::at_add(adaptive_timeout *at, unsigned int val, cfs_time_t t)
{
	unsigned int old = at->at_estimate;
	cfs_time_t step = at_history / at_subwndcnt;
	cfs_time_t nowsecs;
	unsigned int tm;
	int changed = 0;

	if (at->at_flags == AT_FLG_QUEUED_RPC ||
	    at->at_flags == AT_FLG_FIXED)
		return 0;

	assert(at);
	nowsecs = now / params.TimeUnit;
	tm = t / params.TimeUnit;

	if (val == 0)
		return 0;

	if (at->at_start == 0) {
		at->at_max = val;
		at->at_hist[0] = val;
		at->at_time[0] = tm;
		at->at_start = nowsecs;
		at->at_estimate = val;
		
		if (at->at_flags & AT_FLG_AET) {
			at->at_stw[0].start = now;
			at->at_stw[0].finish = 0;
			at->at_stw[0].aet = 0;
			at->at_max_aet = 0;
		}
	} else if (nowsecs - at->at_start < step) {
		/* in the same sub time window of previous record. */
		if (val > at->at_hist[0]) {
			at->at_time[0] = tm; 
			at->at_hist[0] = max(val, at->at_hist[0]);
			at->at_max = max(val, at->at_max);
			changed = 1;
		}
		
		if (at->at_flags & AT_FLG_AET) {
			slide_time_wind *stw = &at->at_stw[0];

			if (stw->start == AET_INVALID_TIME) {
				stw->start = now;
			} else {
				LASSERTF(now >= stw->start, "now %llu stw start %llu",
					now, stw->start);
				stw->finish++;
				stw->vst += (now - stw->start);
				stw->start = now;
				stw->aet = stw->vst / stw->finish;
				//at->at_max_aet = max(stw->aet, at->at_max_aet);
			}
		}
	} else {
		int i, shift;
		unsigned int maxval = val;
		cfs_duration_t maxaet = 0;

		/* Move the time windows.*/
		shift = (nowsecs - at->at_start) / step;
		LASSERT(shift > 0);
		for (i = at_subwndcnt - 1; i >= 0; i--) {
			if (i >= shift) {
				at->at_hist[i] = at->at_hist[i - shift];
				at->at_time[i] = at->at_time[i - shift];
				maxval = max(maxval, at->at_hist[i]);

				/* Average Execute Time algorithm. */
				if (at->at_flags & AT_FLG_AET) {
					at->at_stw[i] = at->at_stw[i - shift];
					maxaet = max(maxaet, at->at_stw[i].aet);
				}
			} else {
				at->at_hist[i] = 0;
				memset(&at->at_stw[i], 0, sizeof(at->at_stw[i]));
				//at->at_time[i] = 0;
			}
		}

		changed = 1;
		at->at_hist[0] = val;
		at->at_time[0] = tm;
		at->at_max = maxval;
		at->at_start += shift * step;

		/* AET */
		if (at->at_flags & AT_FLG_AET) {
			at->at_max_aet = maxaet;
			at->at_stw[0].start = now;
			at->at_stw[0].finish = 0;
			at->at_stw[0].aet = 0;
		}
	}

	if (at->at_flags == AT_FLG_MAX)
		at->at_estimate = at->at_max;
	else if ((at->at_flags == AT_FLG_CURFIT ||
		 at->at_flags == AT_FLG_HYBRID) && changed)
		CurveFitting(at);
	else if (at->at_flags == AT_FLG_AET) {
		obd_count qlen;
		cfs_duration_t aet;

		LASSERT(IsServer());
		qlen = GetQueuedRPCNum();
		aet = at_get_aet(at);
		at->at_estimate = ((double)aet / (double)params.TimeUnit) * qlen;
		if (rpcnt == 0)
			at->at_stw[0].start = AET_INVALID_TIME;
	}

	at->at_est_van = at->at_estimate;
	if (at_max > 0)
		at->at_estimate = min(at->at_estimate, at_max);
	//at->at_estimate = max(at->at_estimate, at_min);

	/* if we changed, report the old value */
	old = (at->at_estimate != old) ? old : 0;

	return old;
}

int Ptlrpc::at_get(adaptive_timeout *at)
{
	if (at->at_flags == AT_FLG_QUEUED_RPC) {
		assert(IsServer());
		return GetQueuedRPCNum() / RPCS_PER_SEC;
	} else if (at->at_flags == AT_FLG_FIXED)
		return params.ptlrpc.ObdTimeout;
	else if (at->at_flags == AT_FLG_FIRST)
		return at->at_hist[0];
	else if (at->at_flags == AT_FLG_HYBRID)
		return min(at->at_estimate, (unsigned int)(GetQueuedRPCNum() / RPCS_PER_SEC));
	return at->at_estimate;
}

int Ptlrpc::at_get_max(adaptive_timeout *at)
{
	//if (at->at_flags == AT_FLG_FIXED)
	//	return params.ptlrpc.ObdTimeout;

	return at->at_max;
}

int Ptlrpc::at_get_van(adaptive_timeout *at)
{
	return at->at_est_van;
}

void Ptlrpc::init_imp_at(imp_at *at)
{
	int i;

	at_init(&at->iat_net_latency, 0, 0);
	for (i = 0; i < IMP_AT_MAX_PORTALS; i++)
		at_init(&at->iat_service_estimate[i], INITIAL_CONNECT_TIMEOUT, at_cli_algo);
}

cfs_time_t Ptlrpc::ptlrpc_at_get_servtime(RPC *rpc)
{
	int idx = 0;
	unsigned int serv_est;
	struct imp_at *iat;

	LASSERT(rpc->imp);
	if (!AT_ON || at_srv_algo == AT_FLG_FIXED)
		return params.ptlrpc.ObdTimeout * params.TimeUnit;

	iat = (imp_at *)rpc->imp->imp_at;
	LASSERT(iat);

	/* FIXME: only one portal for I/O. */
	serv_est = at_get(&iat->iat_service_estimate[idx]);
	return ((serv_est + (serv_est >> 2) + 5) * params.TimeUnit);
}

void Ptlrpc::ptlrpc_at_adj_servtime(RPC *rpc, unsigned int serv_est)
{
        int idx = 0;
        unsigned int oldse;
        struct imp_at *iat;

        LASSERT(rpc->imp);
        iat = (imp_at *)rpc->imp->imp_at;

        //idx = import_at_get_index(req->rq_import, req->rq_request_portal);
        /* max service estimates are tracked on the server side,
           so just keep minimal history here. */
        oldse = at_add(&iat->iat_service_estimate[idx], serv_est, now);
	if (oldse != 0)
		PtlPrint("The RPC service estimate has changed from %d to %d\n",
			oldse, at_get(&iat->iat_service_estimate[idx]));
}

cfs_time_t Ptlrpc::ptlrpc_at_get_net_latency(RPC *rpc)
{
	imp_at *iat;

	iat = (imp_at *)rpc->imp->imp_at;
	return AT_ON ? (at_get(&iat->iat_net_latency) * params.TimeUnit) : 0;
}

/* Adjust expected network latency */
void Ptlrpc::ptlrpc_at_adj_net_latency(RPC *rpc)
{
	cfs_duration_t ticks;
        unsigned int nl, oldnl;
        struct imp_at *iat;

        LASSERT(rpc->imp);
        iat = (imp_at *)rpc->imp->imp_at;

        /* Network latency is total time less server processing time */
	/*__DEBUG__ */
	if (now <= rpc->arrivalTime + rpc->servTime) {
		printf(NOW"RPC %p, early reply %s, arrival time %llu, service time %llu\n", 
		       now, rpc, rpc->GetPhase() == RPC_PHASE_EARLY_REPLY ? "yes" : "no",
		       rpc->arrivalTime, rpc->servTime);
	}
	assert(now > rpc->arrivalTime + rpc->servTime);
	ticks = now - rpc->arrivalTime - rpc->servTime;
	rpc->max_netlatency = max(rpc->max_netlatency, ticks);
        nl = max_t(int, ticks / params.TimeUnit, 0) + 1;
        if (rpc->servTime > now - rpc->sendTime + 3 * params.TimeUnit /* bz16408 */)
                PtlPrint("Reported service time %u > total measured time %llu\n", 
			rpc->servTime / params.TimeUnit, now - rpc->sendTime);

        oldnl = at_add(&iat->iat_net_latency, nl, now);
        if (oldnl != 0)
                Print("The network latency"
                       "has changed from %d to %d\n",
                       oldnl, at_get(&iat->iat_net_latency));
}

void Ptlrpc::ptlrpc_at_set_timer()
{
	RPC *r;

	LASSERT(ATEP_ON);

	r = (RPC *) epQ->First();
	LASSERTF_RPC(r->deadline > at_epmargin * params.TimeUnit &&
	       r->deadline - at_epmargin * params.TimeUnit >= now, r);
	expire = r->deadline - at_epmargin * params.TimeUnit;
	epTimer->ModTimer(expire);
}

void Ptlrpc::ptlrpc_at_add_timed(RPC *rpc, int retimed)
{
	LASSERT(ATEP_ON); 
	LASSERT(rpc->erp_timed == 0);
	rpc->erp_timed = 1;
	RPC_TRACE(rpc, RPCID);
	epQ->Enqueue(rpc);
	LASSERTF_RPC(rpc->deadline > at_epmargin * params.TimeUnit &&
	       rpc->deadline - at_epmargin * params.TimeUnit >= now, rpc);
	if (retimed)
		ptlrpc_at_set_timer();
}

int Ptlrpc::ptlrpc_at_send_early_reply(RPC *rpc, unsigned int extra_time)
{
	RPC *r;
	Message *m, *erpm;
	cfs_time_t newdl;

	if (!ATEP_ON)
		return 0;

	if (extra_time) {
		/* Fake our processing time into the future to ask the
		 * clients for some extra amount of time */
		extra_time += (now - rpc->arrivalTime) / params.TimeUnit;
		at_add(EST, extra_time, rpc->arrivalTime);
	}

	newdl = rpc->arrivalTime + at_get_max(EST) * params.TimeUnit;

	if (at_srv_algo != AT_FLG_FIXED)
	LASSERTF(newdl > rpc->deadline && newdl > at_epmargin * params.TimeUnit + now,
	         NOW "RPC %p:%d new dealine %llu old deadline %llu arrival time %llu\n",
		 now, rpc, rpc->GetId(), newdl, rpc->deadline, rpc->arrivalTime);

	m = rpc->msg;
	erpm = new Message;
	/* FIXME: XXX
	 * @m->dst: service node NID.
	 * @m->src: client node NID.
	 */
	erpm->Init(MSG_EARLY_REPLY, m->dst, m->src);
	r = erpm->GetRPC();
	r->Copy(rpc);
	/* Send the light-weight early reply. */
	erpm->phase = RPC_PHASE_EARLY_REPLY;
	erpm->SetLength(DEFAULT_MSG_SIZE); /* 4k */
	Send(erpm);

	if (rpc->GetId() == RPCID)
		printf(NOW"RPC %p deadline %llu new deadline %llu\n", 
		       now, rpc, rpc->deadline, newdl);
	/* set new deadline for the RPC */
	
	if (at_srv_algo == AT_FLG_FIXED) {
		rpc->deadline += at_extra * params.TimeUnit;
		r->srvdl = rpc->deadline;
	} else
		rpc->deadline = newdl;

	rpc->erpcnt++;

	RPC_TRACE(rpc, RPCID);
	return 0;
}

int Ptlrpc::IsClient()
{
	return (__type == NODE_CLIENT);
}

int Ptlrpc::IsServer()
{
	return (__type == NODE_SERVER);
}

int Ptlrpc::DeadlineCompare(void *a, void *b)
{
	RPC *r1, *r2;

	r1 = ((RPC *)a);
	r2 = ((RPC *)b);

	return (r1->deadline > r2->deadline) ? 1: -1;
}

void Ptlrpc::ptlrpc_at_check_timed()
{
	RPC *rpc;
	struct list_head work_list;

#if 0
	/* __DEBUG__ */
	rpc = (RPC *)(epQ->Dequeue());
	if (rpc == NULL)
		return;
	printf(NOW"Server Send early reply\n", now);
	if (ptlrpc_at_send_early_reply(rpc, at_extra))
			ptlrpc_at_add_timed(rpc);
	return;
#endif

	rpc = (RPC *)(epQ->First());
	if (rpc == NULL)
		return;

	assert(rpc->deadline >= now);
	if (rpc->deadline - now > at_epmargin * params.TimeUnit) {
		ptlrpc_at_set_timer();
		return;
	}

	/* We're close to a timeout, and we don't know how much longer
	 * the server will take. Send early replies to everyone expiring 
	 * soon. */
	while ((rpc = (RPC *)(epQ->First())) != NULL) {
		if (rpc->deadline > now + at_epmargin * params.TimeUnit)
			break;
		LASSERT(rpc->erp_timed == 1);
		rpc->erp_timed = 0;
		RPC_TRACE(rpc, RPCID);
		epQ->Erase(rpc);
		if (ptlrpc_at_send_early_reply(rpc, at_extra) == 0)
			ptlrpc_at_add_timed(rpc, 0);
	}
	ptlrpc_at_set_timer();
}

void Ptlrpc::CheckEarlyReply(void *arg)
{
	Ptlrpc *ptl = (Ptlrpc *)arg;

	printf(NOW "Early reply timer expires\n", now);
	ptl->ptlrpc_at_check_timed();
}

void Ptlrpc::TimeoutEvent(void *arg)
{
	RPC *rpc = (RPC *)arg;

	PtlPrint(NOW "RPC %p timed out\n", now, rpc);

	rpc->rto++;
	rpc->numrto++;
	rpc->tot_toval += (rpc->deadline - rpc->sendTime);
	assert(rpc->numrpc >= rpc->numrto);
	RPC_TRACE(rpc, RPCID);
	if ((now - rpc->startime >= params.TimeUnit) && rpc->st) {
		cfs_duration_t avg_toval;

		avg_toval = rpc->tot_toval / rpc->rto / params.TimeUnit;
		rpc->st->Record("%llu.%09llu\t%lu\t%lu\t%lu %llu\n",
			now / params.TimeUnit, now % params.TimeUnit,
			rpc->rto, rpc->numrto, rpc->numrpc, avg_toval);
		rpc->rto = 0;
		rpc->tot_toval = 0;
		rpc->startime = now;
	}
	rpc->timedout = 1;
}

void Ptlrpc::PollEvent(void *arg)
{
	RPC *rpc = (RPC *)arg;
	Ptlrpc *ptl = (Ptlrpc *)rpc->data;
	Message *msg = rpc->msg;

	LASSERT(POLL_ON);

	printf(NOW "client sends secondary polling RPC %p request.\n", now, rpc);
	
	//RPC_DEBUG(rpc);
	msg->phase = RPC_PHASE_POLLING;
	msg->SetLength(DEFAULT_MSG_SIZE);
	ptl->Send(msg);	
}

void Ptlrpc::ClientSend(Message *msg)
{
	LASSERT(IsClient());

	if (msg->phase == RPC_PHASE_NEW) {
		RPC *rpc = msg->GetRPC();
		Timer *timer = &rpc->timer;

		rpc->numrpc++;
		rpc->sendTime = now;

		if (!TO_ON)
			return;

		if (SCHED_ON) {
			rpc->deadline = rpc->sendTime + at_sched_base * params.TimeUnit 
					+ Rand(at_sched_rand * params.TimeUnit);
		} else {
			// NORMAL PATH START.
		rpc->deadline = rpc->sendTime + ptlrpc_at_get_servtime(rpc) +
			ptlrpc_at_get_net_latency(rpc);

		if (rpc->deadline < at_min)
			rpc->deadline = at_min;
			// NORMAL PATH END.
		}

		rpc->srvdl = rpc->deadline;
		if (ATEP_ON)
			LASSERTF_RPC(rpc->srvdl > now + at_epmargin * params.TimeUnit, rpc);

		RPC_TRACE(rpc, RPCID); 
		timer->SetupTimer(TimeoutEvent, (void *)rpc);
		timer->ModTimer(rpc->deadline);
		PtlPrint(NOW"RPC %p setting deadline %llu timeout val %llu netlatency %llu\n", 
			now, rpc, rpc->deadline, rpc->servTime, ptlrpc_at_get_net_latency(rpc));

		if (POLL_ON) {
			Message *pm;
			Timer *poller;
			RPC *r;

			pm = new Message;
			pm->Init(MSG_POLL, msg->src, msg->dst);

			r = pm->GetRPC();
			r->data = this;
			pm->phase = RPC_PHASE_POLL_INTERVAL;
			poller = &r->poller;
			poller->SetupTimer(PollEvent, (void *)r);
			poller->ModTimer(now + at_poll_intv * params.TimeUnit);
			rpc->pm = pm;
		}
	}
}

void Ptlrpc::ServerSend(Message *msg)
{
	RPC *rpc = msg->GetRPC();

	if (msg->phase == RPC_PHASE_COMPLETE) {
		rpc->servTime = now - rpc->arrivalTime;
		rpc->workTime = now - rpc->handleTime;
		rpc->max_servtime = max(rpc->max_servtime, rpc->servTime);
		rpc->max_worktime = max(rpc->max_worktime, rpc->workTime);
	}

	if (params.cc.ON && msg->phase == RPC_PHASE_COMPLETE) {
		at_add(EST, max_t(int, rpc->servTime / params.TimeUnit, 1), rpc->arrivalTime);
		if (msg->GetType() == MSG_READ || msg->GetType() == MSG_WRITE) {
			IO *io = (IO *)msg->req;
			LASSERT(io->count != 0);
			iosize -= io->count; 
			CalcGrantRCC(rpc);
		}
	}

	if (!TO_ON || !AT_ON || msg->phase == RPC_PHASE_RPC)
		return;

	RPC_TRACE(rpc, RPCID);
	if (msg->phase == RPC_PHASE_COMPLETE) {
		rpcnt--;
		LASSERT(rpcnt >= 0);
		at_add(EST, max_t(int, rpc->servTime / params.TimeUnit, 1), rpc->arrivalTime);
		AddESTStat(rpc);
		if (ATEP_ON) {
			/*LASSERTF(rpc->erp_timed, 
				NOW"RPC %p timed:%d arrival time %llu handle time %llu "
				"service time %llu\n",
				now, rpc, rpc->erp_timed, rpc->arrivalTime, rpc->handleTime,
				rpc->servTime);*/
			LASSERTF_RPC(rpc->erp_timed, rpc);
			rpc->erp_timed = 0;
			epQ->Erase(rpc);
			ptlrpc_at_check_timed();
		}
		rpc->estServTime = at_get(EST);
	} else if (msg->phase == RPC_PHASE_EARLY_REPLY) {
		assert(ATEP_ON);
		rpc->servTime = now - rpc->arrivalTime;
		rpc->estServTime = at_get_max(EST);
	}
}

void Ptlrpc::Send(Message *msg)
{
	//if (TO_ON) {
		if (IsClient())
			ClientSend(msg);
		else if (IsServer())
			ServerSend(msg);
	//}

	Node::Send(msg);
}

void Ptlrpc::AddRPCRecord(RPC *rpc)
{
	int i;
	cfs_time_t s;

	assert(rpc->GetPhase() == RPC_PHASE_COMPLETE);

	/* RPC RTT log */
	s = rpc->rtt / params.TimeUnit;
	if (s < rpc->rttbegin)
		i = 0;
	else {
		i = (s - rpc->rttbegin) / rpc->rttstep + 1;
		if (i >= MAX_RPC_SLOTS)
			i = MAX_RPC_SLOTS - 1;
	}
	rpc->rtt_hist[i]++;

	/* RPC work time. */
	s = rpc->workTime;
	if (s < rpc->wktbegin)
		i = 0;
	else {
		i = (s - rpc->wktbegin) / rpc->wktstep + 1;
		if (i >= MAX_RPC_SLOTS)
			i = MAX_RPC_SLOTS - 1;
		LASSERT(i > 0 && i < MAX_RPC_SLOTS);
	}
	rpc->wkt_hist[i]++;
}

void Ptlrpc::AddESTStat(RPC *rpc)
{
	if (!rpc->est)
		return;

	rpc->est->Record("%llu.%09llu\t%lu\t%llu.%09llu\t%llu.%09llu %lu %llu %llu\n",
		now / params.TimeUnit, now % params.TimeUnit,
		at_get(EST), rpc->servTime / params.TimeUnit,
		rpc->servTime % params.TimeUnit,
		rpc->arrivalTime / params.TimeUnit,
		rpc->arrivalTime % params.TimeUnit,
		at_get_van(EST), at_get_aet(EST), rpcnt);

	/*
	 * Snapshot the current status.
	 */
	if (rpc->snaptime > 0 && rpc->snaptime <= now &&
	    at_srv_algo == AT_FLG_CURFIT) {
		rpc->snaptime = 0;
		rpc->est->Record("# SNAPSHOT START ------------------->\n");
		for (int i = 0; i < AT_WIND_CNT; i++) {
			rpc->est->Record("# %lu	%lu\n", EST->at_time[i], EST->at_hist[i]);
		}

		rpc->est->Record("# a0:%lf a1: %lf\n", EST->a0, EST->a1);
		rpc->est->Record("# NOW: %llu, EST: %lu\n", now / params.TimeUnit, EST->at_estimate);
		rpc->est->Record("# SNAPSHOT END --------------------->\n");
	}
}

void Ptlrpc::ClientRecv(Message *msg)
{
	RPC *rpc = msg->GetRPC();

	LASSERT(IsClient());

	/* The reply of client polling message. */
	if (msg->GetType() == MSG_POLL) {
		printf(NOW"client receives polling reply message %p rpc %p.\n",
		       now, msg, msg->GetRPC());
		if (rpc->poll_end)
			delete msg;
		else {
			/* Send next polling message. */
			msg->phase = RPC_PHASE_POLL_INTERVAL;
			rpc->poller.ModTimer(now + params.TimeUnit * at_poll_intv);
		}
		return;
	}

	if (msg->phase == RPC_PHASE_COMPLETE) {
		rpc->rtt = now - rpc->sendTime;
		rpc->max_rtttime = max(rpc->max_rtttime, rpc->rtt);
		rpc->tot_rtt += rpc->rtt;

		if (!TO_ON)
			goto Indicate;

		AddRPCRecord(rpc);
		if (!rpc->timedout)
			rpc->timer.DelTimer();

		if (POLL_ON) {
			Message *pm = rpc->pm;
			RPC *r = pm->GetRPC();

			if (pm->phase == RPC_PHASE_POLL_INTERVAL) {
				r->poller.DelTimer();
				delete pm;
			} else 
				r->poll_end = 1;
		}
	}

	if (!TO_ON)
		goto Indicate;

	if (!AT_ON || (msg->phase != RPC_PHASE_COMPLETE &&
	    msg->phase != RPC_PHASE_EARLY_REPLY))
		goto Indicate;

	ptlrpc_at_adj_servtime(rpc, rpc->estServTime);
	ptlrpc_at_adj_net_latency(rpc);

	/* FIXME: handle early reply. */
	if (ATEP_ON && msg->phase == RPC_PHASE_EARLY_REPLY) {
		RPC *r = rpc->orig;

		assert(r != NULL);
		
		PtlPrint(NOW"Client Receives Early Reply\n", now);
		if (!rpc->timedout) {
			cfs_time_t olddl;
			
			olddl = r->deadline;
			rpc->deadline = now + ptlrpc_at_get_servtime(r) + 
				ptlrpc_at_get_net_latency(r);

			if (at_srv_algo == AT_FLG_FIXED)
				rpc->deadline = rpc->srvdl;

			LASSERT(rpc->srvdl <= rpc->deadline);
			r->timer.ModTimer(r->deadline);
			PtlPrint(NOW "RPC %p receive Early reply %p."
			      " New deadline %llu (%llu) srvdl(%llu) \n",
			      now, r, rpc, r->deadline, olddl, r->srvdl);
		} else
			PtlPrint(NOW"The RPC %p already expired when "
			       "receive the  early reply %p.\n",
			       now, r, rpc);
		delete msg;
		return;
	}

Indicate:
	/* Indicate receive a message. */
	msg->Notify();
}

void Ptlrpc::ServerRecv(Message *msg)
{
	RPC *rpc = msg->GetRPC();

	if (msg->GetType() == MSG_POLL) {
		printf(NOW "server sends poll reply rpc %p.\n", now, rpc);
		msg->SetLength(DEFAULT_MSG_SIZE);
		Node::Send(msg);
		return;
	}

	if (msg->GetType() == MSG_PING) {
		printf(NOW "server sends ping reply message %p.\n", now, rpc);
		msg->SetLength(DEFAULT_MSG_SIZE);
		Node::Send(msg);
		return;
	}

	if (params.cc.ON && (msg->GetType() == MSG_READ ||
	    msg->GetType() == MSG_WRITE) && msg->phase == RPC_PHASE_NEW) {
		IO *io = (IO *)msg->req;

		iosize += io->count;
		cwnd->max_depth = max(cwnd->max_depth, GetQueuedRPCNum());
	}
		
	RPC_TRACE(rpc, RPCID);
	if (msg->phase == RPC_PHASE_NEW) {
		rpcnt++;
		rpc->arrivalTime = now;

		if (!TO_ON)
			return;

		/* check whether AT is on */
		/* TODO: add the rpc to early reply list. */
		if (ATEP_ON)
			ptlrpc_at_add_timed(rpc, 1);
	}
}

void Ptlrpc::Recv(Message *msg)
{
	if (IsClient())
		ClientRecv(msg);
	else
		ServerRecv(msg);
}

void Ptlrpc::SetRPCHandleTime(Message *msg, cfs_time_t t)
{
	RPC *rpc = msg->GetRPC();

	rpc->handleTime = t;
}

void Ptlrpc::ConnectImport(obd_import *imp)
{
	imp_at *iat;

	if (!TO_ON)
		return;

	iat = new imp_at;
	init_imp_at(iat);
	imp->imp_at = iat;
}

void Ptlrpc::DisconnectImport(obd_import *imp)
{
	imp_at *iat;

	if (!TO_ON)
		return;

	iat = (imp_at *)imp->imp_at;
	delete iat;	
}


/* Server-side congestion control */
#define METHOD 5
float Ptlrpc::coef = 0.1;
void Ptlrpc::CalcGrantRCC(RPC *rpc)
{
	unsigned int depth;
	unsigned int st, arc;

	depth = GetQueuedRPCNum();

/* Method 1: */
if (METHOD == 1) {
	st = rpc->servTime / params.TimeUnit;
	if (st < params.cc.Lmax) {
		ccDe = (1 + coef) * ccDe;
	} else {
		ccDe = params.cc.Dmax * ((float)params.cc.Lmax/(float)st);
	}
}

/* Method 2: */
if (METHOD == 2) {
	st = at_get(EST);
	if (st < params.cc.Lmax) {
		ccDe = params.cc.Dmax;
	} else {
		ccDe = params.cc.Dmax * ((float)params.cc.Lmax/(float)st);
	}
}

/* Method 3: */
if (METHOD == 3) {
	st = depth / 300;
	//st = (iosize / 1048576) / 300;
	if (st < params.cc.Lmax) {
		ccDe = params.cc.Dmax;
	} else {
		ccDe = ccDe * ((float)params.cc.Lmax/(float)st);
	}
}

/* Method 4: */
if (METHOD == 4) {
	//st = (iosize / 1048576) / 300;
	st = depth / 300;
	if (now - cwnd->start >= cwnd->len) {
		if (st < params.cc.Lmax && ccDe < params.cc.Dmax) {
			//if (depth <= cwnd->max_depth)
				ccDe += ccDe / 10;
		} else {
			ccDe = ccDe * ((float)params.cc.Lmax/(float)st);
		}

		cwnd->max_depth = 0;
		cwnd->start = now;
	}
}

/*Method 5: */
if (METHOD == 5) {
	int la;
	float c = 0.1;

	st = iosize / 1048576 / 300;
	la = rpc->servTime / params.TimeUnit;
	st =  0.8 * st + 0.2 * la;

	if (st < params.cc.Lmax && ccDe < params.cc.Dmax) {
			//if (depth <= cwnd->max_depth)
				ccDe += ccDe / 10;
	} else {
		ccDe = ccDe * ((float)params.cc.Lmax/(float)st);
	}

	cwnd->max_depth = 0;
	cwnd->start = now;
}

	ccDe = max(ccDe, (unsigned int)ccC);
	ccDe = min((unsigned int)params.cc.Dmax, ccDe);

	arc = ccDe / ccC;

	if (depth < params.cc.Dmin) {
		unsigned int best = max(rpc->dc, params.cc.Cbest);

		rpc->rcc = max(min(params.cc.Dmin - depth, best), (unsigned int)rpc->rcc);
	} else if (depth > params.cc.Dmax) {
		rpc->rcc = params.cc.Cmin;
	} else {
		rpc->rcc = arc;
	}

	rpc->rcc = min(rpc->rcc, params.cc.Cmax);
	rpc->rcc = max(rpc->rcc, params.cc.Cmin);
	
	if (ccst) {
		int rpcst = rpc->servTime / params.TimeUnit;

		ccst->Record("%llu.%09llu\t%d\t%d\t%d\t%d\t%d\t%llu\n",
			now / params.TimeUnit,
			now % params.TimeUnit,
			ccDe, depth, st, rpcst, rpc->rcc, iosize);
	}
}

char *Ptlrpc::GetAtSrvAlgo()
{
	if (!params.ptlrpc.AtON)
		return "NONE, AtOFF";

	switch (at_srv_algo) {
	case AT_FLG_MAX:
		return "AT_MAX";
	case AT_FLG_CURFIT:
		return "AT_CURFIT";
	case AT_FLG_HYBRID:
		return "AT_HYBRID";
	case AT_FLG_FIRST:
		return "AT_FLG_FIRST";
	case AT_FLG_QUEUED_RPC:
		return "AT_FLG_QUEUED_RPC";
	case AT_FLG_FIXED:
		return "AT_FLG_FIXED";
	case AT_FLG_AET:
		return "AT_FLG_AET";
	}

	return "AT_UNKNOW";
}

struct ptlrpc_request_set *Ptlrpc::ptlrpc_prep_set(void)
{
        struct ptlrpc_request_set *set;

        ENTRY;
        OBD_ALLOC(set, sizeof *set);
        if (!set)
                RETURN(NULL);
        CFS_INIT_LIST_HEAD(&set->set_requests);
        //cfs_waitq_init(&set->set_waitq);
        set->set_remaining = 0;
        //spin_lock_init(&set->set_new_req_lock);
        CFS_INIT_LIST_HEAD(&set->set_new_requests);
        CFS_INIT_LIST_HEAD(&set->set_cblist);

        RETURN(set);
}

void Ptlrpc::ptlrpc_set_destroy(struct ptlrpc_request_set *set)
{
	LASSERT(set->set_remaining == 0);

        OBD_FREE(set, sizeof(*set));
        EXIT;
}

#define RUN_STATE_MACHINE(fn,f)    \
do {                                 \
	IN_ ## fn ## _lable:                    \
	## f ## ;                  \
	goto FUNTION_END;            \
	OUT_ ## fn ## _lable:                   \
}while(0)

int Ptlrpc::ptlrpc_set_wait_f1()
{
	Thread *t = Thread::Current();

	t->RunAfter(4);
	RETURN(0);
}

int Ptlrpc::ptlrpc_set_wait(struct ptlrpc_request_set *set)
{
	struct list_head      *tmp;
        struct ptlrpc_request *req;
        //struct l_wait_info     lwi;
        int                    rc, timeout;
        ENTRY;

        if (list_empty(&set->set_requests))
                RETURN(0);
/*
        list_for_each(tmp, &set->set_requests) {
                req = list_entry(tmp, struct ptlrpc_request, rq_set_chain);
                if (req->rq_phase == RQ_PHASE_NEW)
                        (void)ptlrpc_send_new_req(req);
        }
*/
        do {
	#if 0
                timeout = ptlrpc_set_next_timeout(set);

                /* wait until all complete, interrupted, or an in-flight
                 * req times out */
                CDEBUG(D_RPCTRACE, "set %p going to sleep for %d seconds\n",
                       set, timeout);
		lwi = LWI_TIMEOUT_INTR(cfs_time_seconds(timeout ? timeout : 1),
                                       ptlrpc_expired_set,
                                       ptlrpc_interrupted_set, set);
                rc = l_wait_event(set->set_waitq, ptlrpc_check_set(set), &lwi);
	#endif
		//RUN_STATE_MACHINE(ptlrpc_set_wait_f1, ptlrpc_set_wait_f1());
                LASSERT(rc == 0 || rc == -EINTR || rc == -ETIMEDOUT);

                /* -EINTR => all requests have been flagged rq_intr so next
                 * check completes.
                 * -ETIMEOUTD => someone timed out.  When all reqs have
                 * timed out, signals are enabled allowing completion with
                 * EINTR.
                 * I don't really care if we go once more round the loop in
                 * the error cases -eeb. */
        } while (rc != 0 || set->set_remaining != 0);

        LASSERT(set->set_remaining == 0);

        rc = 0;
#if 0
        list_for_each(tmp, &set->set_requests) {
                req = list_entry(tmp, struct ptlrpc_request, rq_set_chain);

                LASSERT(req->rq_phase == RQ_PHASE_COMPLETE);
                if (req->rq_status != 0)
                        rc = req->rq_status;
        }

        if (set->set_interpret != NULL) {
                int (*interpreter)(struct ptlrpc_request_set *set,void *,int) =
                        set->set_interpret;
                rc = interpreter (set, set->set_arg, rc);
        } else {
                struct ptlrpc_set_cbdata *cbdata, *n;
                int err;
		 list_for_each_entry_safe(cbdata, n,
                                         &set->set_cblist, psc_item) {
                        list_del_init(&cbdata->psc_item);
                        err = cbdata->psc_interpret(set, cbdata->psc_data, rc);
                        if (err && !rc)
                                rc = err;
                        OBD_SLAB_FREE(cbdata, ptlrpc_cbdata_slab,
                                        sizeof(*cbdata));
                }
        }
#endif
FUNTION_END:
        RETURN(rc);
}

void Ptlrpc::Start()
{
	Node::Start();

	if (IsServer()) {
		if (TO_ON || params.cc.ON) {
			EST = new struct adaptive_timeout;
			at_init(EST, 0, at_srv_algo);
			if (ATEP_ON) {
				epTimer = new Timer;
				epQ = new NrsRbtree(DeadlineCompare);
				epTimer->SetupTimer(CheckEarlyReply, this);
			}
			if (params.cc.ON) {
				ccst = new Stat("Dsrv.St");
				cwnd = new time_control_wind;
				cwnd->start = 0;
				cwnd->len = 5000000;
				cwnd->max_depth = 0;
			}
		} 
	}
}
