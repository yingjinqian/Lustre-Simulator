/***************************************************************************
 *   Copyright (C) 2008 by qian                                            *
 *                                                                         *
 *   Storage Team in NUDT                                                  *
 *   Yingjin Qian <yingjin.qian@sun.com>                                   *
 *                                                                         *
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
#ifndef MESSAGE_H
#define MESSAGE_H

#include <event.h>
#include <timer.h>
#include "stat.h"

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/

#define NET_PKT_TX 1
#define NET_PKT_RX 2
#define NET_PKT_BLK 4
#define NET_PKT_TXDONE 8
#define NET_PKT_RXDONE 16
#define NET_PKT_RETX 16
#define NET_PKT_MSG 32 /* Last packet of the message */

class Packet: public Event
{
public:
	int id;
	int state;
	obd_count size;
	int src;
	int dst;
	void *data;
public:
	Packet() 
      : Event(), id(0), state(0), size(0), data(NULL)
		{ src = dst = -1; }
	Packet(int s, int d)
      : Event(), id(0), state(0), size(0), data(NULL)
		{ src = s; dst = d;}
	~Packet() {}	
};

/* Message or operation Type */
#define MSG_NONE  0
#define MSG_PING   1
#define MSG_LOCK  2
#define MSG_READ  3
#define MSG_WRITE 4

/* For network banchmark */
#define MSG_LATENCY    5
#define MSG_BANDWIDTH 6
#define MSG_NICPERF     7
#define MSG_READ_NET  8  /* Network Read */
#define MSG_WRITE_NET 9  /* Network Write */
#define MSG_NETWORK 10

#define MSG_OPEN 11
#define MSG_EARLY_REPLY 12
#define MSG_POLL 13

#define IMP_AT_MAX_PORTALS 8
#include "hash.h"

struct obd_import {
	struct list_head imp_replay_list;
	struct list_head imp_sending_list;
	struct list_head imp_delayed_list;
	
	/* Adaptive timeout data structure */
	void *imp_at;
};

enum rpc_phase {
	RPC_PHASE_NEW	= 0xebc0de00,
	RPC_PHASE_RPC	= 0xebc0de01,
	RPC_PHASE_BULK	= 0xebc0de02,
	RPC_PHASE_INTERPRET	= 0xebc0de03,
	RPC_PHASE_COMPLETE	= 0xebc0de04,
	RPC_PHASE_EARLY_REPLY	= 0xebc0de05,
	RPC_PHASE_POLL_INTERVAL	= 0xebc0de06,
	RPC_PHASE_POLLING	= 0xebc0de07,
};

class Message;
#define MAX_RPC_SLOTS 20
class RPC {
	rb_node rbnode; /* It must be the first member. */

public:
	static cfs_time_t startime;
	static uint32_t rto; // timed-out rpcs in 1 sec.
	static uint32_t numrpc; // total rpcs
	static uint32_t numrto;  // total timeout rpcs
	static cfs_duration_t tot_toval; // total timeout value in a sec
	static cfs_duration_t tot_rtt; // total rtt, used to calculate average rtt
	static Stat *st; // timeout stat file
	static Stat *est; // stat for estimate service time.
	static cfs_duration_t max_servtime; /* include wait and executing time.*/
	static cfs_duration_t max_worktime; /* executing time */
	static cfs_duration_t max_rtttime;
	static cfs_duration_t max_netlatency;
	static cfs_duration_t wktbegin;
	static cfs_duration_t wktstep;
	static cfs_time_t snaptime;
	static unsigned int rttbegin;
	static unsigned int rttstep;
	static unsigned int rtt_hist[MAX_RPC_SLOTS];
	static unsigned int wkt_hist[MAX_RPC_SLOTS];

	unsigned long intr:1, replied:1, err:1, timedout:1, 
		resend:1, restart:1, reply:1, waiting:1, 
		receiving_reply:1, net_err:1, early:1,
		poll_end:1,
		/* server-side flags */
		packed_final:1, sent_final:1, erp_timed:1;

	cfs_time_t sendTime; /* in nanosecond */
	cfs_time_t arrivalTime;
	cfs_time_t handleTime;
	cfs_time_t deadline;
	cfs_time_t srvdl;
	cfs_time_t __deadline; /* server side */
	cfs_duration_t servTime;
	cfs_duration_t workTime; 
	cfs_duration_t rtt;
	unsigned int estServTime; /* in second */
	int erpcnt; /* early reply count. */
	Timer timer; /* timer for RPC timeout. */
	Timer poller; /* timer for polling interval. */
	obd_import *imp;
	Message *msg;
	Message *pm; /* Polling Message. */
	RPC *orig; /* original RPC of the early reply RPC. */
	void *data;

	/* Congestion Control data members */
	int dc;
	int rcc; /* rcc and feedback credits */

	struct list_head list;

public:
	RPC();
	~RPC();

	int GetPhase();
	void SetPhase(int phase);
	void Copy(RPC *rpc);
	int GetId();
	static cfs_duration_t GetMaxRTT();
	static cfs_duration_t GetMaxServTime();
    static void InitRPCTimeout(const char *str);
	static void FiniRPCTimeout();
	static cfs_duration_t GetAvgRTT();
};

#define DEFAULT_MSG_SIZE 4096
class Message : public Packet
{
	/*
	 * Class Packet -> Class Event ->rb_node.
	 */

	static uint32_t num;

	int id;
	int type;
	int flags;
	int slice; /* No of packet */
	Thread *worker[2];
	Thread *waiter;
	rb_node rbn;

	RPC rpc;
	void InitRPC();

public:
	unsigned int r:1;
	int nt:1;
	int phase;
	obd_count left;
	obd_count msz;
	obd_count key;

	IO io;
	void *req;

	int sid; /* Server ID */
	int cid; /* Client ID */

public:
	Message();
	Message(int t, int s, int d);
	~Message();

	void Init(int t, int s, int d);
	void SetLength(obd_count len);
	void SetIO(obd_id id, int cmd, obd_off off, obd_count count);
	void SetRecvTime(cfs_time_t t);
	void SetWorker(Thread *t);
	cfs_time_t GetRecvTime();
	int GetType();
	int GetId();
	obd_count GetIOCount();
	obd_count GetKey();
	Packet *GetPacket();

	void ReverseChannel();
	void Notify();

	/* RPC */
	RPC *GetRPC();
	void SetImport(obd_import *imp);
	char *GetPhase();
	cfs_time_t GetSendTime();

	void Copy(Message *msg);
	cfs_duration_t GetRTT();
	cfs_duration_t GetServTime();
	cfs_time_t GetArrivalTime();
	int GetRCC();
	char *GetTypeStr();
	static rb_node *GetKey(void *e);
};

#endif
