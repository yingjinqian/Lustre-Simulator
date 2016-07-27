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
#ifndef PTLRPC_H
#define PTLRPC_H

#include <node.h>

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/

/* Adaptive Timeout implementation.   */
/* Server side I/O congestion control */

#define AT_FLG_NONE 0
#define AT_FLG_MAX 1
#define AT_FLG_CURFIT 2
#define AT_FLG_HYBRID 4
#define AT_FLG_FIRST  8
#define AT_FLG_QUEUED_RPC  16
#define AT_FLG_FIXED 32
#define AT_FLG_AET 64

enum rq_phase {
        RQ_PHASE_NEW            = 0xebc0de00,
        RQ_PHASE_RPC            = 0xebc0de01,
        RQ_PHASE_BULK           = 0xebc0de02,
        RQ_PHASE_INTERPRET      = 0xebc0de03,
        RQ_PHASE_COMPLETE       = 0xebc0de04,
        RQ_PHASE_UNREGISTERING  = 0xebc0de05,
        RQ_PHASE_UNDEFINED      = 0xebc0de06
};

struct ptlrpc_request {
        int rq_type; /* one of PTL_RPC_MSG_* */
        struct list_head rq_list;
        struct list_head rq_timed_list;         /* server-side early replies */
        struct list_head rq_history_list;       /* server-side history */
        struct list_head rq_exp_list;           /* server-side per-export list */

	RPC *rpc;
};

struct ptlrpc_request_set;
typedef int (*set_interpreter_func)(struct ptlrpc_request_set *, void *, int);

struct ptlrpc_request_set {
        int               set_remaining; /* # uncompleted requests */
        //cfs_waitq_t       set_waitq;
        //cfs_waitq_t      *set_wakeup_ptr;
        struct list_head  set_requests;
        struct list_head  set_cblist; /* list of completion callbacks */
        set_interpreter_func    set_interpret; /* completion callback */
        void              *set_arg; /* completion context */
        /* locked so that any old caller can communicate requests to
         * the set holder who can then fold them into the lock-free set */
        //spinlock_t        set_new_req_lock;
        struct list_head  set_new_requests;
};

class Ptlrpc : public Node
{
	static unsigned int at_min;
	static unsigned int at_max;
	static unsigned int at_history;
	static unsigned int at_subwndcnt;
	static unsigned int at_epmargin;
	static unsigned int at_extra;
	static unsigned int at_poll_intv;
	static unsigned int at_sched_base;
	static unsigned int at_sched_rand;
	static int at_srv_algo;
	static int at_cli_algo;
protected:
	#define NODE_CLIENT 1
	#define NODE_SERVER 2

	/* 
	 * value stored in slibing time window. 
	 */
	struct slide_time_wind {
		int flags;
		obd_count finish; /* the count of finish rpcs in this time window. */
		cfs_time_t start;
		cfs_duration_t vst; /* valid service time in this time window. */
		cfs_duration_t aet;
	};

	#define AT_WIND_CNT 8
	struct adaptive_timeout {
		cfs_time_t at_start;
		unsigned int at_flags;
		unsigned int at_max;
		unsigned int at_estimate;
		unsigned int at_est_van;
		cfs_duration_t at_max_aet; /* Max. RPC average execute time. */
		unsigned int at_hist[AT_WIND_CNT];
		unsigned int at_time[AT_WIND_CNT];
		slide_time_wind at_stw[AT_WIND_CNT];

		/*curfit LLS algo*/
		double a0;
		double a1;
	};
	#define IMP_AT_MAX_PORTALS 8
	struct imp_at {
		int iat_portal[IMP_AT_MAX_PORTALS];
		struct adaptive_timeout iat_net_latency;
		struct adaptive_timeout iat_service_estimate[IMP_AT_MAX_PORTALS];
	};

	/* Server side - */
	struct adaptive_timeout *EST; /* estimate service time */
	/* Early reply */
	cfs_time_t expire;
	Timer *epTimer;
	Scheduler *epQ;
	obd_count rpcnt;

	/* Add history record to adaptive timeout sliding window */
	void at_init(adaptive_timeout *at, int val, int flags);	
	int at_add(adaptive_timeout *at, unsigned int val, cfs_time_t t);
	cfs_duration_t at_get_aet(adaptive_timeout *at);
	int at_get(adaptive_timeout *at);
	int at_get_max(adaptive_timeout *at);
	int at_get_van(adaptive_timeout *at);
	void init_imp_at(imp_at *at);
	cfs_time_t ptlrpc_at_get_net_latency(RPC *rpc);
	cfs_time_t ptlrpc_at_get_servtime(RPC *rpc);
	void ptlrpc_at_adj_net_latency(RPC *rpc);
	void ptlrpc_at_adj_servtime(RPC *rpc, unsigned int serv_est);
	void ptlrpc_at_set_timer();
	void ptlrpc_at_check_timed();
	void ptlrpc_at_add_timed(RPC *rpc, int retimed);
	int ptlrpc_at_send_early_reply(RPC *rpc, unsigned int extra_time);

	void ClientSend(Message *msg);
	void ServerSend(Message *msg);
	void ClientRecv(Message *msg);
	void ServerRecv(Message *msg);
	void AddESTStat(RPC *rpc);
	void AddRPCRecord(RPC *rpc);
	int IsClient();
	int IsServer();

	virtual obd_count GetQueuedRPCNum() { return 0; };
	/* Formula to calcuate the estimate server time */
	static void CurveFitting(adaptive_timeout *at);
	
	static void SetRPCHandleTime(Message *msg, cfs_time_t t);
	static int DeadlineCompare(void *a, void *b);
	static void TimeoutEvent(void *arg);
	static void PollEvent(void *arg);
	static void CheckEarlyReply(void *arg);
	static void PtlPrint(const char *fmt...);

	/* I/O Congestion Control. */
	static float coef;
	unsigned int ccDe; 
	int ccC;
	Stat *ccst;
	obd_count iosize;
	struct time_control_wind {
		cfs_time_t start;
		cfs_duration_t len;
		obd_count max_depth;
	} *cwnd;

	void CalcGrantRCC(RPC *rpc);
public:
	Ptlrpc();
	~Ptlrpc();
	
	void Start();
	void Send(Message *msg);
	virtual void Recv(Message *msg);
	
	void ConnectImport(obd_import *imp);
	void DisconnectImport(obd_import *imp);

	static char *GetAtSrvAlgo();

	/*
	 * Ptlrpc Request set.
	 */
	static struct ptlrpc_request_set *ptlrpc_prep_set();
	static void ptlrpc_set_destroy(struct ptlrpc_request_set *set);
	static int ptlrpc_set_wait(struct ptlrpc_request_set *set);
	static int ptlrpc_set_wait_f1();
};

#endif
