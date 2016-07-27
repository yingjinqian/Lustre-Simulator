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
#include "nrstbf.h"
#include "processor.h"
#include "message.h"
#include "nrsfifo.h"
#include <errno.h>
#include <string.h>

enum nrs_tbf_flag NrsTbf::type = NRS_TBF_FLAG_JOBID;

struct cfs_binheap_ops NrsTbf::nrs_tbf_heap_ops = {
    .hop_enter = NULL,
    .hop_exit = NULL,
    .hop_compare = tbf_cli_compare,
};

struct cfs_hash_ops NrsTbf::nrs_tbf_jobid_hash_ops = {
    .hs_hash	= nrs_tbf_jobid_hop_hash,
    .hs_key		= nrs_tbf_jobid_hop_key,
    .hs_keycpy  = NULL,
    .hs_keycmp	= nrs_tbf_jobid_hop_keycmp,
    .hs_object	= nrs_tbf_jobid_hop_object,
    .hs_get		= nrs_tbf_jobid_hop_get,
    .hs_put		= nrs_tbf_jobid_hop_put,
    .hs_put_locked	= nrs_tbf_jobid_hop_put,
    .hs_exit	= nrs_tbf_jobid_hop_exit,
};

int NrsTbf::tbf_jobid_cache_size = 8192;

int NrsTbf::tbf_cli_compare(struct cfs_binheap_node *e1, struct cfs_binheap_node *e2)
{
    struct nrs_tbf_client *cli1;
    struct nrs_tbf_client *cli2;

    cli1 = container_of(e1, struct nrs_tbf_client, tc_node);
    cli2 = container_of(e2, struct nrs_tbf_client, tc_node);

    if (cli1->tc_check_time + cli1->tc_nsecs <
        cli2->tc_check_time + cli2->tc_nsecs)
        return 1;
    else if (cli1->tc_check_time + cli1->tc_nsecs >
         cli2->tc_check_time + cli2->tc_nsecs)
        return 0;

    if (cli1->tc_check_time < cli2->tc_check_time)
        return 1;
    else if (cli1->tc_check_time > cli2->tc_check_time)
        return 0;

    /* Maybe need more comparasion, e.g. request number in the rules */
    return 1;
}

unsigned NrsTbf::nrs_tbf_jobid_hop_hash(struct cfs_hash *hs, const void *key,
                  unsigned mask)
{
    return cfs_hash_djb2_hash(key, strlen((const char *)key), mask);
}

int NrsTbf::nrs_tbf_jobid_hop_keycmp(const void *key, struct hlist_node *hnode)
{
    struct nrs_tbf_client *cli;

    cli = (struct nrs_tbf_client *)hlist_entry(hnode,
                                               struct nrs_tbf_client,
                                               tc_hnode);

    return (strcmp(cli->tc_jobid, (const char *)key) == 0);
}

void *NrsTbf::nrs_tbf_jobid_hop_key(struct hlist_node *hnode)
{
    struct nrs_tbf_client *cli = hlist_entry(hnode,
                             struct nrs_tbf_client,
                             tc_hnode);

    return cli->tc_jobid;
}

void *NrsTbf::nrs_tbf_jobid_hop_object(struct hlist_node *hnode)
{
    return hlist_entry(hnode, struct nrs_tbf_client, tc_hnode);
}

void NrsTbf::nrs_tbf_jobid_hop_get(struct cfs_hash *hs, struct hlist_node *hnode)
{
    struct nrs_tbf_client *cli = hlist_entry(hnode,
                             struct nrs_tbf_client,
                             tc_hnode);

    atomic_inc(&cli->tc_ref);
}

void NrsTbf::nrs_tbf_jobid_hop_put(struct cfs_hash *hs, struct hlist_node *hnode)
{
    struct nrs_tbf_client *cli = hlist_entry(hnode,
                                             struct nrs_tbf_client,
                                             tc_hnode);

    atomic_dec(&cli->tc_ref);
}

void NrsTbf::nrs_tbf_jobid_hop_exit(struct cfs_hash *hs, struct hlist_node *hnode)
{
    struct nrs_tbf_client *cli = hlist_entry(hnode,
                                             struct nrs_tbf_client,
                                             tc_hnode);

    LASSERT(atomic_read(&cli->tc_ref) == 0);

    nrs_tbf_cli_fini(cli);
}

NrsTbf::NrsTbf()
{
    algo = NRS_ALGO_TBF;
    nrs_tbf_start();
}

NrsTbf::~NrsTbf()
{
    nrs_tbf_stop();
}

void NrsTbf::Attach(Processor *p)
{
    tp = p;
}

void NrsTbf::WakeupThreadPool()
{
    tp->Wakeup();
}

int NrsTbf::nrs_tbf_startup()
{
   return nrs_tbf_jobid_startup();
}

struct nrs_tbf_client *NrsTbf::nrs_tbf_cli_find(struct IO *ioreq)
{
    return nrs_tbf_jobid_cli_find(ioreq);
}

struct nrs_tbf_client *NrsTbf::nrs_tbf_cli_findadd(struct nrs_tbf_client *cli)
{
    return nrs_tbf_jobid_cli_findadd(cli);
}

void NrsTbf::nrs_tbf_cli_put(struct nrs_tbf_client *cli)
{
    nrs_tbf_jobid_cli_put(cli);
}

void NrsTbf::nrs_tbf_policy_cli_init(struct nrs_tbf_client *cli, struct IO *ioreq)
{
    nrs_tbf_jobid_cli_init(cli, ioreq);
}

int NrsTbf::nrs_tbf_policy_rule_init(nrs_tbf_rule *rule)
{
    return nrs_tbf_jobid_rule_init(rule);
}

int NrsTbf::nrs_tbf_policy_rule_match(struct nrs_tbf_rule *rule, struct nrs_tbf_client *cli)
{
    return nrs_tbf_jobid_rule_match(rule, cli);
}

void NrsTbf::nrs_tbf_policy_rule_fini(struct nrs_tbf_rule *rule)
{
    nrs_tbf_jobid_rule_fini(rule);
}

#define NRS_TBF_DEFAULT_RULE "default"

struct nrs_tbf_rule * NrsTbf::nrs_tbf_rule_match(struct nrs_tbf_client *cli)
{
    struct nrs_tbf_rule *rule = NULL;
    struct nrs_tbf_rule *tmp_rule;

    spin_lock(&head.th_rule_lock);
    /* Match the newest rule in the list */
    list_for_each_entry(tmp_rule, &head.th_list, tr_linkage) {
        //LASSERT((tmp_rule->tr_flags & NTRS_STOPPING) == 0);
        if (nrs_tbf_policy_rule_match(tmp_rule, cli)) {
            rule = tmp_rule;
            break;
        }
    }

    if (rule == NULL)
        rule = head.th_rule; // Default rule.

    nrs_tbf_rule_get(rule);
    spin_unlock(&head.th_rule_lock);
    return rule;
}

void NrsTbf::nrs_tbf_cli_rule_put(struct nrs_tbf_client *cli)
{
    LASSERT(!list_empty(&cli->tc_linkage));
    LASSERT(cli->tc_rule);
    spin_lock(&cli->tc_rule->tr_rule_lock);
    list_del_init(&cli->tc_linkage);
    spin_unlock(&cli->tc_rule->tr_rule_lock);
    nrs_tbf_rule_put(cli->tc_rule);
    cli->tc_rule = NULL;
}

void NrsTbf::__nrs_tbf_cli_reset_value(struct nrs_tbf_client *cli, bool keep)

{
    struct nrs_tbf_rule *rule = cli->tc_rule;

    cli->tc_rpc_rate = rule->tr_rpc_rate;
    cli->tc_nsecs = rule->tr_nsecs;
    cli->tc_depth = rule->tr_depth;
    cli->tc_rule_sequence = atomic_read(&head.th_rule_sequence);
    cli->tc_rule_generation = rule->tr_generation;

    if (keep == false) {
        cli->tc_ntoken = rule->tr_depth;
        cli->tc_check_time = Event::Clock(); // set checkpoint time with current time
    }

    if (cli->tc_in_heap)
        cfs_binheap_relocate(head.th_binheap,
                     &cli->tc_node);
}

void NrsTbf::nrs_tbf_cli_reset_value(nrs_tbf_client *cli)
{
    __nrs_tbf_cli_reset_value(cli, false);
}

void NrsTbf::nrs_tbf_cli_reset(struct nrs_tbf_rule *rule, struct nrs_tbf_client *cli)
{
    spin_lock(&cli->tc_rule_lock);
    if (cli->tc_rule != NULL && !list_empty(&cli->tc_linkage)) {
        LASSERT(rule != cli->tc_rule);
        nrs_tbf_cli_rule_put(cli);
    }
    LASSERT(cli->tc_rule == NULL);
    LASSERT(list_empty(&cli->tc_linkage));
    /* Rule's ref is added before called */
    cli->tc_rule = rule;
    spin_lock(&rule->tr_rule_lock);
    list_add_tail(&cli->tc_linkage, &rule->tr_cli_list);
    spin_unlock(&rule->tr_rule_lock);
    spin_unlock(&cli->tc_rule_lock);
    nrs_tbf_cli_reset_value(cli);
}

void NrsTbf::nrs_tbf_cli_init(struct nrs_tbf_client *cli, struct IO *ioreq)
{
    struct nrs_tbf_rule *rule;

    memset(cli, 0, sizeof(*cli));
    cli->tc_in_heap = false;
    nrs_tbf_policy_cli_init(cli, ioreq);
    //INIT_LIST_HEAD(&cli->tc_list);
    cli->tc_queue = new NrsFifo;
    INIT_LIST_HEAD(&cli->tc_linkage);
    spin_lock_init(&cli->tc_rule_lock);
    atomic_set(&cli->tc_ref, 1);
    rule = nrs_tbf_rule_match(cli);
    nrs_tbf_cli_reset(rule, cli);
}

void NrsTbf::nrs_tbf_rule_fini(struct nrs_tbf_rule *rule)
{
    NrsTbf *tbf = rule->tr_tbf;
    LASSERT(atomic_read(&rule->tr_ref) == 0);
    LASSERT(list_empty(&rule->tr_cli_list));
    LASSERT(list_empty(&rule->tr_linkage));

    nrs_tbf_policy_rule_fini(rule);
    OBD_FREE_PTR(rule);
}

inline void NrsTbf::nrs_tbf_rule_get(struct nrs_tbf_rule *rule)
{
    atomic_inc(&rule->tr_ref);
}

void NrsTbf::nrs_tbf_rule_put(struct nrs_tbf_rule *rule)
{
    if (atomic_dec_and_test(&rule->tr_ref))
        nrs_tbf_rule_fini(rule);
}

void NrsTbf::nrs_tbf_policy_rule_dump(struct nrs_tbf_rule *rule)
{
    nrs_tbf_jobid_rule_dump(rule);
}

void NrsTbf::nrs_tbf_rule_dump_all()
{
    struct nrs_tbf_rule *rule;

    printf(NOW "Dump all rules:\n", Event::Clock());
    /* List the rules from newest to oldest */
    list_for_each_entry(rule, &head.th_list, tr_linkage) {
        nrs_tbf_policy_rule_dump(rule);
    }
}


#define NRS_TBF_JOBID_BKT_BITS 10
#define NRS_TBF_JOBID_HASH_FLAGS (CFS_HASH_SPIN_BKTLOCK | \
                  CFS_HASH_NO_ITEMREF | \
                  CFS_HASH_DEPTH)

int NrsTbf::nrs_tbf_jobid_hash_order(void)
{
    int bits;

    for (bits = 1; (1 << bits) < tbf_jobid_cache_size; ++bits)
        ;

    return bits;
}

int NrsTbf::nrs_tbf_jobid_startup()
{
    struct nrs_tbf_bucket *bkt;
    int          bits;
    int			 i;
    int			 rc = 0;
    struct cfs_hash_bd	 bd;

    bits = nrs_tbf_jobid_hash_order();
    if (bits < NRS_TBF_JOBID_BKT_BITS)
        bits = NRS_TBF_JOBID_BKT_BITS;
    head.th_cli_hash = cfs_hash_create("nrs_tbf_hash",
                                        bits,
                                        bits,
                                        NRS_TBF_JOBID_BKT_BITS,
                                        sizeof(*bkt),
                                        0,
                                        0,
                                        &nrs_tbf_jobid_hash_ops,
                                        NRS_TBF_JOBID_HASH_FLAGS);
    if (head.th_cli_hash == NULL)
        return -ENOMEM;

    cfs_hash_for_each_bucket(head.th_cli_hash, &bd, i) {
        bkt = (struct nrs_tbf_bucket *) cfs_hash_bd_extra_get(head.th_cli_hash, &bd);
        INIT_LIST_HEAD(&bkt->ntb_lru);
    }
    return rc;
}

#define NRS_TBF_JOBID_NULL ""

void NrsTbf::nrs_tbf_cli_fini(struct nrs_tbf_client *cli)
{
    LASSERT(cli->tc_queue->Empty());
    LASSERT(!cli->tc_in_heap);
    LASSERT(atomic_read(&cli->tc_ref) == 0);
    spin_lock(&cli->tc_rule_lock);
    nrs_tbf_cli_rule_put(cli);
    spin_unlock(&cli->tc_rule_lock);
    OBD_FREE_PTR(cli);
}

struct nrs_tbf_client * NrsTbf::
nrs_tbf_jobid_hash_lookup(struct cfs_hash *hs,
                          struct cfs_hash_bd *bd,
                          const char *jobid)
{
    struct hlist_node *hnode;
    struct nrs_tbf_client *cli;

    /* cfs_hash_bd_peek_locked is a somehow "internal" function
     * of cfs_hash, it doesn't add refcount on object. */
    hnode = cfs_hash_bd_peek_locked(hs, bd, (void *)jobid);
    if (hnode == NULL)
        return NULL;

    cfs_hash_get(hs, hnode);
    cli = container_of0(hnode, struct nrs_tbf_client, tc_hnode);
    if (!list_empty(&cli->tc_lru))
        list_del_init(&cli->tc_lru);
    return cli;
}

struct nrs_tbf_client *NrsTbf::
nrs_tbf_jobid_cli_find(struct IO *ioreq)
{
    const char *jobid;
    struct nrs_tbf_client *cli;
    struct cfs_hash *hs = head.th_cli_hash;
    struct cfs_hash_bd bd;

    jobid = ioreq->jobid;
    if (jobid == NULL)
        jobid = NRS_TBF_JOBID_NULL;
    cfs_hash_bd_get_and_lock(hs, (void *)jobid, &bd, 1);
    cli = nrs_tbf_jobid_hash_lookup(hs, &bd, jobid);
    cfs_hash_bd_unlock(hs, &bd, 1);

    return cli;
}

struct nrs_tbf_client * NrsTbf::
nrs_tbf_jobid_cli_findadd(struct nrs_tbf_client *cli)
{
    const char *jobid;
    struct nrs_tbf_client *ret;
    struct cfs_hash *hs = head.th_cli_hash;
    struct cfs_hash_bd bd;

    jobid = cli->tc_jobid;
    cfs_hash_bd_get_and_lock(hs, (void *)jobid, &bd, 1);
    ret = nrs_tbf_jobid_hash_lookup(hs, &bd, jobid);
    if (ret == NULL) {
        cfs_hash_bd_add_locked(hs, &bd, &cli->tc_hnode);
        ret = cli;
    }
    cfs_hash_bd_unlock(hs, &bd, 1);

    return ret;
}

void NrsTbf::nrs_tbf_jobid_cli_put(struct nrs_tbf_client *cli)
{
    struct cfs_hash_bd bd;
    struct cfs_hash *hs = head.th_cli_hash;
    struct nrs_tbf_bucket *bkt;
    __u32 hw;
    struct list_head zombies;

    INIT_LIST_HEAD(&zombies);
    cfs_hash_bd_get(hs, &cli->tc_jobid, &bd);
    bkt = (struct nrs_tbf_bucket *)cfs_hash_bd_extra_get(hs, &bd);
    if (!cfs_hash_bd_dec_and_lock(hs, &bd, &cli->tc_ref))
        return;
    LASSERT(list_empty(&cli->tc_lru));
    list_add_tail(&cli->tc_lru, &bkt->ntb_lru);

    /*
     * Check and purge the LRU, there is at least one client in the LRU.
     */
    hw = tbf_jobid_cache_size >>
         (hs->hs_cur_bits - hs->hs_bkt_bits);
    while (cfs_hash_bd_count_get(&bd) > hw) {
        if (unlikely(list_empty(&bkt->ntb_lru)))
            break;
        cli = list_entry(bkt->ntb_lru.next,
                     struct nrs_tbf_client,
                     tc_lru);
        LASSERT(atomic_read(&cli->tc_ref) == 0);
        cfs_hash_bd_del_locked(hs, &bd, &cli->tc_hnode);
        list_move(&cli->tc_lru, &zombies);
    }
    cfs_hash_bd_unlock(head.th_cli_hash, &bd, 1);

    while (!list_empty(&zombies)) {
        cli = container_of0(zombies.next,
                    struct nrs_tbf_client, tc_lru);
        list_del_init(&cli->tc_lru);
        nrs_tbf_cli_fini(cli);
    }
}

void NrsTbf::nrs_tbf_jobid_cli_init(struct nrs_tbf_client *cli, struct IO *ioreq)
{
    char *jobid = ioreq->jobid;

    if (jobid == NULL)
        jobid = NRS_TBF_JOBID_NULL;
    LASSERT(strlen(jobid) < LUSTRE_JOBID_SIZE);
    INIT_LIST_HEAD(&cli->tc_lru);
    memcpy(cli->tc_jobid, jobid, strlen(jobid));
}

void NrsTbf::nrs_tbf_jobid_list_free(struct list_head *jobid_list)
{
   struct nrs_tbf_jobid *jobid, *n;

   list_for_each_entry_safe(jobid, n, jobid_list, tj_linkage) {
       OBD_FREE(jobid->tj_id, strlen(jobid->tj_id) + 1);
       list_del(&jobid->tj_linkage);
       OBD_FREE(jobid, sizeof(struct nrs_tbf_jobid));
   }
}
int NrsTbf::nrs_tbf_jobid_rule_init(struct nrs_tbf_rule *rule)
{
    char jobid[LUSTRE_JOBID_SIZE];
    int rc = 0;

    if (rule->tr_index == 0) { // defaul rule
        strcpy(jobid, "*");
    } else {
        assert(rule->tr_index > 0);
        sprintf(jobid, "JOBID.%d", rule->tr_index - 1);
    }

    if (params.nrs.tbf.TbfRuleStat == true &&
        rule->tr_index < params.nrs.tbf.MaxNumRuleStats) {
        char name[MAX_NAME_LEN];

        if (rule->tr_index == 0) {
            sprintf(name, "RuleDefault.st");
        } else {
            sprintf(name, "Rule%s.st", jobid);
        }
        rule->tr_nts.nts_stat = new Stat(name);
    }

    OBD_ALLOC(rule->tr_jobids_str, strlen(jobid) + 1);
    if (rule->tr_jobids_str == NULL)
        return -ENOMEM;

    memcpy(rule->tr_jobids_str, jobid, strlen(jobid));

    INIT_LIST_HEAD(&rule->tr_jobids);
    if (rule->tr_index != 0) {
        rc = nrs_tbf_jobid_list_parse(rule->tr_jobids_str,
                              strlen(rule->tr_jobids_str),
                              &rule->tr_jobids);
        if (rc)
            Print("jobids {%s} illegal\n", rule->tr_jobids_str);
    }
    if (rc)
        OBD_FREE(rule->tr_jobids_str, strlen(jobid) + 1);

    return rc;
}

void NrsTbf::nrs_tbf_jobid_rule_fini(struct nrs_tbf_rule *rule)
{
    if (!list_empty(&rule->tr_jobids))
        nrs_tbf_jobid_list_free(&rule->tr_jobids);
    LASSERT(rule->tr_jobids_str != NULL);
    OBD_FREE(rule->tr_jobids_str, strlen(rule->tr_jobids_str) + 1);
    delete rule->tr_nts.nts_stat;
}

int NrsTbf::nrs_tbf_jobid_list_match(struct list_head *jobid_list, char *id)
{
    struct nrs_tbf_jobid *jobid;

    list_for_each_entry(jobid, jobid_list, tj_linkage) {
        if (strcmp(id, jobid->tj_id) == 0)
            return 1;
    }
    return 0;
}

int NrsTbf::nrs_tbf_jobid_list_add(const struct cfs_lstr *id, struct list_head *jobid_list)
{
    struct nrs_tbf_jobid *jobid;

    OBD_ALLOC(jobid, sizeof(struct nrs_tbf_jobid));
    if (jobid == NULL)
        return -ENOMEM;

    OBD_ALLOC(jobid->tj_id, id->ls_len + 1);
    if (jobid->tj_id == NULL) {
        OBD_FREE(jobid, sizeof(struct nrs_tbf_jobid));
        return -ENOMEM;
    }

    memcpy(jobid->tj_id, id->ls_str, id->ls_len);
    list_add_tail(&jobid->tj_linkage, jobid_list);
    return 0;
}

int NrsTbf::nrs_tbf_jobid_list_parse(char *str, int len, struct list_head *jobid_list)
{
    struct cfs_lstr src;
    int rc = 0;
    ENTRY;

    src.ls_str = str;
    src.ls_len = len;
    INIT_LIST_HEAD(jobid_list);

    rc = nrs_tbf_jobid_list_add(&src, jobid_list);
    if (rc)
        nrs_tbf_jobid_list_free(jobid_list);
    RETURN(rc);
}

int NrsTbf::nrs_tbf_jobid_rule_match(struct nrs_tbf_rule *rule,
                                     struct nrs_tbf_client *cli)
{
    return nrs_tbf_jobid_list_match(&rule->tr_jobids, cli->tc_jobid);
}

void NrsTbf::nrs_tbf_jobid_rule_dump(struct nrs_tbf_rule *rule)
{
    printf("        %s {%s} %llu, ref %d\n", rule->tr_name,
           rule->tr_jobids_str, rule->tr_rpc_rate,
           atomic_read(&rule->tr_ref) - 1);
}

struct nrs_tbf_rule *NrsTbf::nrs_tbf_rule_find(const char *name)
{
    struct nrs_tbf_rule *rule;

    list_for_each_entry(rule, &head.th_list, tr_linkage) {
        //LASSERT((rule->tr_flags & NTRS_STOPPING) == 0);
        if (strcmp(rule->tr_name, name) == 0) {
            nrs_tbf_rule_get(rule);
            return rule;
        }
    }
    return NULL;
}

int NrsTbf::nrs_tbf_start()
{
    const char *name;

    if (type == NRS_TBF_FLAG_JOBID) {
        name = "tbf";
    } else {
        LASSERTF(0, "TBF policy type not support!\n");
        return -ENOTSUP;
    }

    memcpy(head.th_type, name, strlen(name));

    head.th_binheap = cfs_binheap_create(&nrs_tbf_heap_ops,
                                         CBH_FLAG_ATOMIC_GROW,
                                         4096, NULL, NULL, 0);
    if (head.th_binheap == NULL) {
        Print("Failed to allocate memory for the TBF binheap\n");
        return -ENOMEM;
    }

    atomic_set(&head.th_rule_sequence, 0);
    INIT_LIST_HEAD(&head.th_list);
    head.th_timer.SetupTimer(NrsTbfTimerCallback, this);
    nrs_tbf_startup();
    nrs_tbf_configure_rules();
    nrs_tbf_stat_init();
    return 0;
}

void NrsTbf::nrs_tbf_stop()
{
    struct nrs_tbf_rule *rule, *n;

    LASSERT(head.th_cli_hash != NULL);

    cfs_hash_putref(head.th_cli_hash);
    list_for_each_entry_safe(rule, n, &head.th_list, tr_linkage) {
        list_del_init(&rule->tr_linkage);
        nrs_tbf_rule_put(rule);
    }
    LASSERT(list_empty(&head.th_list));
    LASSERT(head.th_binheap != NULL);
    LASSERT(cfs_binheap_is_empty(head.th_binheap));
    cfs_binheap_destroy(head.th_binheap);
    throttling = 0;
    nrs_tbf_stat_fini();
}

void NrsTbf::nrs_tbf_stat_perf(struct nrs_tbf_stat *nts, int op, __u64 value)
{
    switch (op) {
    case OP_ENQUEUE:
        nts->nts_queue_depth++;
        break;
    case OP_DEQUEUE:
        nts->nts_queue_depth--;
        LASSERT(nts->nts_queue_depth >= 0);
        break;
    case OP_FINISH: {
        __u64 passed;
        __u64 now;

        nts->nts_tot_rpcs++;
        now = Event::Clock();
        passed = now - nts->nts_last_check;
        if (passed >= params.TimeUnit) {
            nts->nts_stat->Record("%llu.%09llu   %llu    %llu   %llu\n",
                                  now / params.TimeUnit,
                                  now % params.TimeUnit,
                                  nts->nts_tot_rpcs,
                                  nts->nts_queue_depth,
                                  value);
            nts->nts_tot_rpcs = 0;
            nts->nts_last_check = now;
        }
        break;
    }
    default:
        LBUG();
    }

}

void NrsTbf::nrs_tbf_rule_stat_init(nrs_tbf_rule *rule)
{

}

void NrsTbf::nrs_tbf_rule_stat_fini(nrs_tbf_rule *rule)
{

}

void NrsTbf::nrs_tbf_stat_rule(struct nrs_tbf_rule *rule, int op)
{
    struct nrs_tbf_stat *nts = &rule->tr_nts;

    if (params.nrs.tbf.TbfRuleStat == false || nts->nts_stat == NULL)
        return;

    nrs_tbf_stat_perf(nts, op, rule->tr_rpc_rate);
}

void NrsTbf::nrs_tbf_stat_init()
{
    if (params.nrs.tbf.TbfStat == false)
        return;

    memset(&nts, 0, sizeof(nts));
    nts.nts_stat = new Stat("tbf.st");

    rast = new Stat("deprate.st");
}

void NrsTbf::nrs_tbf_stat_fini()
{
    if (params.nrs.tbf.TbfStat == false)
        return;

    delete nts.nts_stat;
    delete rast;
}

void NrsTbf::nrs_tbf_stat(int op, struct IO *ioreq)
{
    struct nrs_tbf_client *cli = (struct nrs_tbf_client *)ioreq->ptr;
    struct nrs_tbf_rule *rule;

    if (params.nrs.tbf.TbfStat == false)
        return;

    nrs_tbf_stat_perf(&nts, op, 0);

    LASSERT(cli != NULL);
    rule = cli->tc_rule;
    nrs_tbf_stat_rule(rule, op);
}

void NrsTbf::nrs_tbf_update_rule_rate(nrs_tbf_client *cli)
{
    struct nrs_tbf_rule *rule;
    struct nrs_tbf_rule *deprule;
    __u64 now = Event::Clock();
    __u64 passed;
    __u64 oldrate;

    rule = cli->tc_rule;
    LASSERT(rule != NULL);
    if (!(rule->tr_flags & NRS_TBF_DEPENDENCY))
        return;

    deprule = rule->tr_deprule;
    if (now - rule->tr_checkTime < deprule->tr_nsecs)
        return;

    passed = now - deprule->tr_lastActiveTime;
    oldrate = rule->tr_rpc_rate;
    rule->tr_checkTime = now;
    /**
     * Increase the RPC rate.
     */
    if (passed > params.nrs.tbf.Alpha * deprule->tr_nsecs / 100 &&
        deprule->tr_nsecsBacklog < params.nrs.tbf.Beta * deprule->tr_nsecs / 100) {
        if (rule->tr_last_round == 0) {
            rule->tr_last_round = 1;
        } else {
            rule->tr_last_round <<= 1;
            rule->tr_last_round = min(rule->tr_last_round,
                                      rule->tr_upper_rate -
                                      rule->tr_lower_rate -
                                      rule->tr_speedup);
        }
        rule->tr_speedup += rule->tr_last_round;
    } else {
        /**
         * Decrease the RPC rate accordingly.
         */
        if (rule->tr_last_round > 0) {
            LASSERT(rule->tr_speedup >= rule->tr_last_round);
            rule->tr_speedup -= rule->tr_last_round;
            rule->tr_last_round = 0;
        } else {
            LASSERT(rule->tr_last_round == 0);
            rule->tr_speedup >>= 1;
        }
    }

    rule->tr_rpc_rate = rule->tr_speedup + rule->tr_lower_rate;
    rule->tr_rpc_rate = min(rule->tr_upper_rate, rule->tr_rpc_rate);

    // For debug: Fix rule RPC rate...
    //rule->tr_rpc_rate = 49;

    rast->Record("%llu.%09llu   %llu    %llu    %llu %llu   %llu\n",
                 now / params.TimeUnit,
                 now % params.TimeUnit,
                 rule->tr_rpc_rate,
                 passed / 1000000,
                 params.nrs.tbf.Alpha * deprule->tr_nsecs / 100 / 1000000,
                 deprule->tr_nsecsBacklog / 1000000,
                 params.nrs.tbf.Beta * deprule->tr_nsecs / 100 / 1000000);
    if(oldrate != rule->tr_rpc_rate) {
        rule->tr_nsecs = NSEC_PER_SEC / rule->tr_rpc_rate;
        __nrs_tbf_cli_reset_value(cli, true);
    }
}

void NrsTbf::NrsTbfTimerCallback(void *arg)
{
    NrsTbf *tbf = (NrsTbf *)arg;

    tbf->throttling = false;
    /* Wakeup I/O service thread pool to dispatch */
    tbf->Print(NOW "(Tbf Timer) expired, wakeup I/O service threads.\n", Event::Clock());
    tbf->WakeupThreadPool();
}

int NrsTbf::Enqueue(void *e)
{
    struct nrs_tbf_client *cli;
    struct nrs_tbf_client *tmp;
    Message *msg = (Message *)e;
    struct IO *ioreq = (struct IO *)msg->req;
    Scheduler *queue;
    __u64 now;
    int rc = 0;

    now = Event::Clock();
    Print(NOW "(TBF) Enqueue a ioreq@%p %d:%llu:%llu %s\n",
          now, ioreq, ioreq->cmd, ioreq->off,
          ioreq->count, ioreq->jobid);

    cli = nrs_tbf_cli_find(ioreq);
    if (cli == NULL) {
        OBD_ALLOC(cli, sizeof(*cli));
        if (cli == NULL)
            return -ENOMEM;

        nrs_tbf_cli_init(cli, ioreq);
        tmp = nrs_tbf_cli_findadd(cli);
        if (tmp != cli) {
            atomic_dec(&cli->tc_ref);
            nrs_tbf_cli_fini(cli);
            cli = tmp;
        }
    }

    /**
     * Update the RPC rate of the rule with dependency.
     */
    nrs_tbf_update_rule_rate(cli);

    queue = cli->tc_queue;
    if (queue->Empty()) {
        LASSERT(!cli->tc_in_heap);
        /**
         * For cached class, the @cli->tc_backlog_ckpt is old value,
         * It needs to update here.
         */
        cli->tc_backlog_ckpt = Event::Clock();
        rc = cfs_binheap_insert(head.th_binheap, &cli->tc_node);
        if (rc == 0) {
            cli->tc_in_heap = true;
            queue->Enqueue(msg);
            if (throttling) {
                __u64 deadline = cli->tc_check_time + cli->tc_nsecs;
                if (deadline < now) {
                    //NrsTbfTimerCallback(this);
                    //LASSERT(0);
                } else if ((head.th_deadline > deadline)) {
                    LASSERTF(deadline > now, NOW "timer deadline %llu, class deadline %llu "
                             "class check time %llu, class nsecs: %llu",
                             now, head.th_deadline, deadline, cli->tc_check_time,
                             cli->tc_nsecs);
                    head.th_deadline = deadline;
                    head.th_timer.ModTimer(deadline);
                }
            }
        }
    } else {
        LASSERT(cli->tc_in_heap);
        queue->Enqueue(msg);
    }
    ioreq->ptr = cli;
    ioreq->arrivalTime = now;
    nrs_tbf_stat(OP_ENQUEUE, ioreq);
    return rc;
}

void *NrsTbf::Dequeue()
{
    struct nrs_tbf_client     *cli;
    struct cfs_binheap_node	  *node;
    struct nrs_tbf_rule *rule;
    __u64 now = Event::Clock();
    __u64 passed;
    __u64 ntoken;
    __u64 deadline;
    Message *msg = NULL;

    if (throttling)
        return NULL;

    node = cfs_binheap_root(head.th_binheap);
    if (unlikely(node == NULL))
        return NULL;

    cli = container_of(node, struct nrs_tbf_client, tc_node);
    LASSERT(cli->tc_in_heap);

    deadline = cli->tc_check_time + cli->tc_nsecs;
    LASSERT(now >= cli->tc_check_time);
    passed = now - cli->tc_check_time;
    ntoken = passed / cli->tc_nsecs;
    ntoken += cli->tc_ntoken;

    // for rule with dependency.
    rule = cli->tc_rule;
    LASSERT(rule != NULL);
    rule->tr_lastActiveTime = now;
    //rule->tr_nsecsBacklog = now - cli->tc_backlog_ckpt;

    if (ntoken > cli->tc_depth)
        ntoken = cli->tc_depth;
    if (ntoken > 0) {
        Scheduler *queue = cli->tc_queue;
        struct IO *ioreq;

        msg = (Message *)queue->Dequeue();
        LASSERT(msg != NULL);
        ioreq = (struct IO *)msg->req;
        nrs_tbf_stat(OP_DEQUEUE, ioreq);
        rule->tr_nsecsBacklog = now - cli->tc_backlog_ckpt;

        Print(NOW "(TBF) Dequeue a ioreq@%x %d:%llu:%llu %s\n",
              Event::Clock(), ioreq, ioreq->cmd, ioreq->off,
              ioreq->count, ioreq->jobid);
        LASSERT(ioreq->ptr == cli);
        ntoken--;
        cli->tc_ntoken = ntoken;
        cli->tc_check_time = now;
        cli->tc_backlog_ckpt = now;
        if (queue->Empty()) {
            cfs_binheap_remove(head.th_binheap,&cli->tc_node);
            cli->tc_in_heap = false;
        } else {
            cfs_binheap_relocate(head.th_binheap, &cli->tc_node);
        }
    } else {
         throttling = true;
         head.th_deadline = deadline;
         head.th_timer.ModTimer(deadline);
         Print(NOW "(TBF Timer) is set, throttle I/O. class check time %llu"
               " nsecs %llu, deadline %llu\n",
               Event::Clock(), cli->tc_check_time, cli->tc_nsecs,
               cli->tc_check_time + cli->tc_nsecs);
    }

    return msg;
}

void NrsTbf::Finish(void *e)
{
    Message *msg = (Message *)e;
    IO *ioreq = (IO *)msg->req;
    struct nrs_tbf_client *cli;
    struct nrs_tbf_rule *rule;

    cli = (struct nrs_tbf_client *)ioreq->ptr;
    rule = cli->tc_rule;

    //rule->tr_nsecsBacklog = Event::Clock() - ioreq->arrivalTime;
    nrs_tbf_stat(OP_FINISH, ioreq);
    nrs_tbf_cli_put(cli);
    ioreq->ptr = NULL;
}

int NrsTbf::nrs_tbf_configure_rules()
{
    char name[MAX_TBF_NAME];
    int num = params.nrs.tbf.NumJobidRule;
    int rc = 0;

    for (int i = 0; i < num; i++) {
        struct nrs_tbf_rule *rule;
        int depid = params.nrs.tbf.DepInfo[i].DepIndex;

        rule = new nrs_tbf_rule;
        memset(rule, 0 , sizeof(*rule));
        snprintf(name, MAX_TBF_NAME, "Rule%d", i);
        memcpy(rule->tr_name, name, strlen(name));
        rule->tr_index = i;
        rule->tr_rpc_rate = params.nrs.tbf.Rate[i];
        rule->tr_nsecs = NSEC_PER_SEC;
        do_div(rule->tr_nsecs, rule->tr_rpc_rate);
        rule->tr_depth = params.nrs.tbf.TbfDepth;
        atomic_set(&rule->tr_ref, 1);
        INIT_LIST_HEAD(&rule->tr_cli_list);
        //INIT_LIST_HEAD(&rule->tr_nids);
        INIT_LIST_HEAD(&rule->tr_linkage);
        spin_lock_init(&rule->tr_rule_lock);
        rule->tr_head = &head;
        rc = nrs_tbf_policy_rule_init(rule);
        list_add(&rule->tr_linkage, &head.th_list);
        atomic_inc(&head.th_rule_sequence);


        if (i == 0) { // Default Rule
            LASSERT(head.th_rule == NULL && depid < 0);
            head.th_rule = rule;
        }

        /**
         * Configure the rules with dependency.
         * Skip the default rule 'Rule0'.
         */
        if( depid >= 0) {
            struct nrs_tbf_rule *deprule;
            char depname[MAX_TBF_NAME];

            LASSERT(depid < i);
            rule->tr_upper_rate = params.nrs.tbf.DepInfo[i].UpperRate;
            rule->tr_lower_rate = params.nrs.tbf.DepInfo[i].LowerRate;
            rule->tr_rpc_rate = rule->tr_lower_rate;
            rule->tr_flags |= NRS_TBF_DEPENDENCY;

            snprintf(depname, MAX_TBF_NAME, "Rule%d", depid);

            deprule = nrs_tbf_rule_find(depname);
            LASSERT(deprule != NULL);
            rule->tr_deprule = deprule;
            nrs_tbf_rule_put(deprule);
        }
    }

    if (params.nrs.tbf.DumpRules) {
        nrs_tbf_rule_dump_all();
    }
    return 0;
}
