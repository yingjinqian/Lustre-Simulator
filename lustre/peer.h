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
#ifndef PEER_H
#define PEER_H

#include <node.h>

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/

/* class used to verify and benchmark the Network model. */
#define SENDER 1
#define RECEIVER 2
#define READER 4
#define WRITER 8
#define SERVER 16

struct nctl {
	int mt;
	int snr; /* number of sender.*/
	int dnr; /* number of receiver. */
	int factor; /* X2 */
	int repeat;
	int launch;
	int finished;
	obd_count msz;
	obd_count maxsz;
	obd_count totcnt;
	cfs_time_t stime;
	cfs_duration_t ticks;

	// Network
	int nodisk;
	int wtnr;
	int rdnr;
	int srvnr;
	obd_count rmsz;
	obd_count wmsz;
	Stat *stat;
};

class Peer : public Node
{
	static uint32_t num;

	int type;
	int nr;
	int id;
	Node *tgts;
	nctl *ctl;

	int state;
	int finished;
	Message *msg;
	Thread t;

	enum {
		peer_SendStartingState,
		peer_SendMessageState,
		peer_SendFiniRoundTripState,
		peer_SendLastState,
	};
	enum {
		peer_ReadStartingState,
		peer_ReadBulkDataState,
		peer_ReadFinishState,
		peer_ReadLastState,
	};
	enum {
		peer_WriteStartingState,
		peer_WriteBulkDataState,
		peer_WriteFinishState,
		peer_WriteLastState,
	};
	enum {
		peer_WriteNdStartState,
		peer_WriteNdRecvGetState,
		peer_WriteNdCompletionState,
		peer_WriteNdLastState,
	};
	enum {
		peer_ReadNdStartState,
		peer_ReadNdRecvPutState,
		peer_ReadNdCompletionState,
		peer_ReadNdLastState,
	};

	void LatencyLauncher();
	static void LatencyStateMachine(void *arg);

	void Reader();
	void ReadNodisk();
	static void ReadStateMachine(void *arg);

	void Writer();
	void WriteNodisk();
	static void WriteStateMachine(void *arg);

	void Recv(Message *msg);
	void RecordLatency();
	void RecordPerformance();
	void BandwidthLaunch();

	void ProcessNetReadWrite(Message *msg);
public:
	Peer();
	~Peer();

	void InitPeer(int t, int nr, Node *targets, nctl *ctl);
	void Start();
};

#endif
