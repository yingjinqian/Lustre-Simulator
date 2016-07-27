/***************************************************************************
 *   Copyright (C) 2016 by DataDriect Networks, Inc.                       *
 *                                                                         *
 *   Yingjin Qian <qian@ddn.com>                                           *
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

#ifndef NRSTBF_H
#define NRSTBF_H

/**
    @author Yingjin Qian <yqian@ddn.com>
    Rule based Token Bucket Filter algorithm
    Performance Test and evaluation;
    Only supprt jobid TBF policy.
*/

#include "cfshash.h"
#include "heap.h"
#include "hash.h"
#include "scheduler.h"
#include "timer.h"
#include "stat.h"

#define MAX_TBF_NAME            16
#define NRS_TBF_TYPE_MAX_LEN    20
#define NSEC_PER_SEC            1000000000UL

/**
 * Rule with dependency.
 */
#define NRS_TBF_DEPENDENCY      1

enum nrs_tbf_flag {
    NRS_TBF_FLAG_JOBID	= 0x0000001,
    NRS_TBF_FLAG_NID	= 0x0000002,
    NRS_TBF_FLAG_OPCODE	= 0x0000004,
};

struct cfs_lstr {
    char *ls_str;
    int ls_len;
};

struct nrs_tbf_stat {
    __u64 nts_queue_depth;
    __u64 nts_tot_rpcs;
    __u64 nts_last_check;
    Stat *nts_stat;
};

class NrsTbf;
struct nrs_tbf_client {
    NrsTbf              *tc_tbf;
    /** Node in the hash table. */
    struct hlist_node    tc_hnode;
    /** Jobid of the client. */
    char                 tc_jobid[LUSTRE_JOBID_SIZE];
    /** Reference number of the client. */
    atomic_t             tc_ref;
    /** Lock to protect rule and linkage. */
    spinlock_t           tc_rule_lock;
    /** Linkage to rule. */
    struct list_head     tc_linkage;
    /** Pointer to rule. */
    struct nrs_tbf_rule *tc_rule;
    /** Generation of the rule matched. */
    __u64                tc_rule_generation;
    /** Limit of RPC rate. */
    __u64                tc_rpc_rate;
    /** Time to wait for next token. */
    __u64                tc_nsecs;
    /** RPC token number. */
    __u64				 tc_ntoken;
    /** Token bucket depth. */
    __u64				 tc_depth;
    /** Time check-point. */
    __u64				 tc_check_time;
    /** List of queued requests. */
//    struct list_head     tc_list;
    /** Node in binary heap. */
    struct cfs_binheap_node		 tc_node;
    /** Whether the client is in heap. */
    bool				 tc_in_heap;
    /** Sequence of the newest rule. */
    __u32				 tc_rule_sequence;
    /**
     * Linkage into LRU list. Protected bucket lock of
     * nrs_tbf_head::th_cli_hash.
     */
    struct list_head	 tc_lru;
    /** I/O queue in FIFO order. */
    Scheduler           *tc_queue;
    /** Time checkpoint for backlog. */
    __u64                tc_backlog_ckpt;
};

struct nrs_tbf_rule {
    /** Index of the rule */
    int                      tr_index;
    /** Name of the rule. */
    char                     tr_name[MAX_TBF_NAME];
    /** Head belongs to. */
    struct nrs_tbf_head		*tr_head;
    NrsTbf                  *tr_tbf;
    /** Likage to head. */
    struct list_head		 tr_linkage;
    /** Nid list of the rule. */
    struct list_head		 tr_nids;
    /** Nid list string of the rule.*/
    char                    *tr_nids_str;
    /** Jobid list of the rule. */
    struct list_head		 tr_jobids;
    /** Jobid list string of the rule.*/
    char                    *tr_jobids_str;
    /** Opcode bitmap of the rule. */
    struct cfs_bitmap		*tr_opcodes;
    /** Opcode list string of the rule.*/
    char                    *tr_opcodes_str;
    /** RPC/s limit. */
    __u64                    tr_rpc_rate;
    /** Time to wait for next token. */
    __u64                    tr_nsecs;
    /** Token bucket depth. */
    __u64                    tr_depth;
    /** Lock to protect the list of clients. */
    spinlock_t               tr_rule_lock;
    /** List of client. */
    struct list_head		 tr_cli_list;
    /** Flags of the rule. */
    __u32                    tr_flags;
    /** Usage Reference count taken on the rule. */
    atomic_t                 tr_ref;
    /** Generation of the rule. */
    __u64                    tr_generation;
    struct nrs_tbf_stat      tr_nts;
    /** Dependency Rule.*/
    struct nrs_tbf_rule     *tr_deprule;
    /** Upper rate limit. */
    __u64                    tr_upper_rate;
    /** Lower rate limit. */
    __u64                    tr_lower_rate;
    /** The number of increased rate. */
    __u64                    tr_speedup;
    /** The last round rate change. */
    __u64                    tr_last_round;
    /** Last Active time of the rule. */
    __u64                    tr_lastActiveTime;
    /** Last backlog time. */
    __u64                    tr_nsecsBacklog;
    /** Time to check whether change speed. */
    __u64                    tr_checkTime;
};

/**
 * Private data structure for the TBF policy
 */
struct nrs_tbf_head {
    /**
     * List of rules.
     */
    struct list_head		 th_list;
    /**
     * Lock to protect the list of rules.
     */
    spinlock_t			 th_rule_lock;
    /**
     * Generation of rules.
     */
    atomic_t			 th_rule_sequence;
    /**
     * Default rule.
     */
    struct nrs_tbf_rule		*th_rule;
    /**
     * Timer for next token.
     */
    Timer               th_timer;
    /**
     * Deadline of the timer.
     */
    __u64				 th_deadline;
    /**
     * Sequence of requests.
     */
    __u64				 th_sequence;
    /**
     * Heap of queues.
     */
    struct cfs_binheap		*th_binheap;
    /**
     * Hash of clients.
     */
    struct cfs_hash			*th_cli_hash;
    /**
     * Type of TBF policy.
     */
    char				 th_type[NRS_TBF_TYPE_MAX_LEN + 1];
    /**
     * Rule operations.
     */
    struct nrs_tbf_ops		*th_ops;
    /**
     * Flag of type.
     */
    __u32				 th_type_flag;
    /**
     * Index of bucket on hash table while purging.
     */
    int				 th_purge_start;
};

class NrsTbf : public Scheduler
{
    struct nrs_tbf_jobid {
        char *tj_id;
        struct list_head tj_linkage;
    };

    struct nrs_tbf_bucket {
        /**
         * LRU list, updated on each access to client. Protected by
         * bucket lock of nrs_tbf_head::th_cli_hash.
         */
        struct list_head	ntb_lru;
    };

    enum {
        OP_ENQUEUE,
        OP_DEQUEUE,
        OP_FINISH,
    };
private:
    struct nrs_tbf_head head;
    struct nrs_tbf_stat nts;
    Processor *tp; /* Thread pool */
    Stat *rast;

    static enum nrs_tbf_flag type;
    static struct cfs_binheap_ops nrs_tbf_heap_ops;
    //static struct nrs_tbf_ops nrs_tbf_jobid_ops;

    int nrs_tbf_start();
    void nrs_tbf_stop();
    void WakeupThreadPool();
    static void NrsTbfTimerCallback(void *arg);

    /* Override this virtual interface to impelment
     * various TBF policies such as NID, OPCode,
     * currently we only support JOBID policy.
     */
    int nrs_tbf_configure_rules();
    virtual int nrs_tbf_startup();
    virtual struct nrs_tbf_client *nrs_tbf_cli_find(struct IO *ioreq);
    virtual struct nrs_tbf_client *nrs_tbf_cli_findadd(struct nrs_tbf_client *cli);
    virtual void nrs_tbf_cli_put(struct nrs_tbf_client *cli);
    virtual void nrs_tbf_policy_cli_init(struct nrs_tbf_client *cli, struct IO *ioreq);
    virtual int nrs_tbf_policy_rule_init(struct nrs_tbf_rule *rule);
    virtual int nrs_tbf_policy_rule_match(struct nrs_tbf_rule *rule, struct nrs_tbf_client *cli);
    static void nrs_tbf_policy_rule_fini(struct nrs_tbf_rule *rule);
    virtual void nrs_tbf_policy_rule_dump(struct nrs_tbf_rule *rule);

    void nrs_tbf_cli_init(struct nrs_tbf_client *cli, struct IO *ioreq);
    void nrs_tbf_cli_reset_value(struct nrs_tbf_client *cli);
    void __nrs_tbf_cli_reset_value(struct nrs_tbf_client *cli, bool keep);
    void nrs_tbf_cli_reset(struct nrs_tbf_rule *rule, struct nrs_tbf_client *cli);

    void nrs_tbf_rule_dump_all();
    static void nrs_tbf_cli_rule_put(struct nrs_tbf_client *cli);

    /* Rule operations */
    struct nrs_tbf_rule *nrs_tbf_rule_match(struct nrs_tbf_client *cli);
    inline void nrs_tbf_rule_get(struct nrs_tbf_rule *rule);
    static void nrs_tbf_rule_put(struct nrs_tbf_rule *rule);
    static void nrs_tbf_rule_fini(struct nrs_tbf_rule *rule);
    static void nrs_tbf_cli_fini(struct nrs_tbf_client *cli);
    struct nrs_tbf_rule *nrs_tbf_rule_find(const char *name);

    /* JOBID policy data structure and operations */
    static int tbf_jobid_cache_size;
    static struct cfs_hash_ops nrs_tbf_jobid_hash_ops;
    static unsigned nrs_tbf_jobid_hop_hash(struct cfs_hash *hs, const void *key, unsigned mask);
    static int nrs_tbf_jobid_hop_keycmp(const void *key, struct hlist_node *hnode);
    static void *nrs_tbf_jobid_hop_key(struct hlist_node *hnode);
    static void *nrs_tbf_jobid_hop_object(struct hlist_node *hnode);
    static void nrs_tbf_jobid_hop_get(struct cfs_hash *hs, struct hlist_node *hnode);
    static void nrs_tbf_jobid_hop_put(struct cfs_hash *hs, struct hlist_node *hnode);
    static void nrs_tbf_jobid_hop_exit(struct cfs_hash *hs, struct hlist_node *hnode);
    int nrs_tbf_jobid_startup();
    static int nrs_tbf_jobid_hash_order();
    static void nrs_tbf_jobid_list_free(struct list_head *jobid_list);
    struct nrs_tbf_client *nrs_tbf_jobid_cli_find(struct IO *ioreq);
    struct nrs_tbf_client *nrs_tbf_jobid_hash_lookup(struct cfs_hash *hs,
                                                     struct cfs_hash_bd *bd,
                                                     const char *jobid);
    struct nrs_tbf_client *nrs_tbf_jobid_cli_findadd(struct nrs_tbf_client *cli);
    void nrs_tbf_jobid_cli_put(struct nrs_tbf_client *cli);
    void nrs_tbf_jobid_cli_init(struct nrs_tbf_client *cli, struct IO *ioreq);
    int nrs_tbf_jobid_rule_init(struct nrs_tbf_rule *rule);
    static void nrs_tbf_jobid_rule_fini(struct nrs_tbf_rule *rule);
    int nrs_tbf_jobid_list_match(struct list_head *jobid_list, char *id);
    int nrs_tbf_jobid_rule_match(struct nrs_tbf_rule *rule, struct nrs_tbf_client *cli);
    int nrs_tbf_jobid_list_parse(char *str, int len, struct list_head *jobid_list);
    int nrs_tbf_jobid_list_add(const struct cfs_lstr *id, struct list_head *jobid_list);
    void nrs_tbf_jobid_rule_dump(struct nrs_tbf_rule *rule);

    /** Performance statistics */
    void nrs_tbf_rule_stat_init(struct nrs_tbf_rule *rule);
    void nrs_tbf_rule_stat_fini(struct nrs_tbf_rule *rule);
    void nrs_tbf_stat_rule(struct nrs_tbf_rule *rule, int op);
    void nrs_tbf_stat_init();
    void nrs_tbf_stat_fini();
    void nrs_tbf_stat(int op, struct IO *ioreq);
    void nrs_tbf_stat_perf(struct nrs_tbf_stat *nts, int op, __u64 value);

    /** Rules with dependency */
    void nrs_tbf_update_rule_rate(struct nrs_tbf_client *cli);

    static int tbf_cli_compare(struct cfs_binheap_node *e1,
                               struct cfs_binheap_node *e2);

protected:
    void Attach(Processor *p);
    virtual int Enqueue(void *e);
    virtual void *Dequeue();
    virtual void Finish(void *e);

public:
    NrsTbf();
    ~NrsTbf();
};

#endif // NRSTBF_H
