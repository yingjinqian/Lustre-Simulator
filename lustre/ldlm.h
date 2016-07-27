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
#ifndef LDLM_H
#define LDLM_H

/**
	Distributed Lock Manager (mainly extent lock)
	@author yingjin.qian <yingjin.qian@sun.com>
*/

#include "obd.h"
#include "intervaltree.h"
#include "ptlrpc.h"

#define LDLM_FL_LOCK_CHANGED   0x000001 /* extent, mode, or resource changed */

/* If the server returns one of these flags, then the lock was put on that list.
 * If the client sends one of these flags (during recovery ONLY!), it wants the
 * lock added to the specified list, no questions asked. -p */
#define LDLM_FL_BLOCK_GRANTED  0x000002
#define LDLM_FL_BLOCK_CONV     0x000004
#define LDLM_FL_BLOCK_WAIT     0x000008

#define LDLM_FL_CBPENDING      0x000010 /* this lock is being destroyed */
#define LDLM_FL_AST_SENT       0x000020 /* blocking or cancel packet was sent */
#define LDLM_FL_WAIT_NOREPROC  0x000040 /* not a real flag, not saved in lock */
#define LDLM_FL_CANCEL         0x000080 /* cancellation callback already run */

/* Lock is being replayed.  This could probably be implied by the fact that one
 * of BLOCK_{GRANTED,CONV,WAIT} is set, but that is pretty dangerous. */
#define LDLM_FL_REPLAY         0x000100

#define LDLM_FL_INTENT_ONLY    0x000200 /* don't grant lock, just do intent */
#define LDLM_FL_LOCAL_ONLY     0x000400 /* see ldlm_cli_cancel_unused */

/* don't run the cancel callback under ldlm_cli_cancel_unused */
#define LDLM_FL_FAILED         0x000800

#define LDLM_FL_HAS_INTENT     0x001000 /* lock request has intent */
#define LDLM_FL_CANCELING      0x002000 /* lock cancel has already been sent */
#define LDLM_FL_LOCAL          0x004000 /* local lock (ie, no srv/cli split) */
#define LDLM_FL_WARN           0x008000 /* see ldlm_cli_cancel_unused */
#define LDLM_FL_DISCARD_DATA   0x010000 /* discard (no writeback) on cancel */

#define LDLM_FL_NO_TIMEOUT     0x020000 /* Blocked by group lock - wait
                                         * indefinitely */

/* file & record locking */
#define LDLM_FL_BLOCK_NOWAIT   0x040000 // server told not to wait if blocked
#define LDLM_FL_TEST_LOCK      0x080000 // return blocking lock

/* XXX FIXME: This is being added to b_size as a low-risk fix to the fact that
 * the LVB filling happens _after_ the lock has been granted, so another thread
 * can match`t before the LVB has been updated.  As a dirty hack, we set
 * LDLM_FL_LVB_READY only after we've done the LVB poop.
 * this is only needed on lov/osc now, where lvb is actually used and callers
 * must set it in input flags.
 *
 * The proper fix is to do the granting inside of the completion AST, which can
 * be replaced with a LVB-aware wrapping function for OSC locks.  That change is
 * pretty high-risk, though, and would need a lot more testing. */

#define LDLM_FL_LVB_READY      0x100000

/* A lock contributes to the kms calculation until it has finished the part
 * of it's cancelation that performs write back on its dirty pages.  It
 * can remain on the granted list during this whole time.  Threads racing
 * to update the kms after performing their writeback need to know to
 * exclude each others locks from the calculation as they walk the granted
 * list. */
#define LDLM_FL_KMS_IGNORE     0x200000

/* Don't drop lock covering mmapped file in LRU */
#define LDLM_FL_NO_LRU         0x400000

/* Immediatelly cancel such locks when they block some other locks. Send
   cancel notification to original lock holder, but expect no reply. */
#define LDLM_FL_CANCEL_ON_BLOCK 0x800000

/* Flags flags inherited from parent lock when doing intents. */
#define LDLM_INHERIT_FLAGS     (LDLM_FL_CANCEL_ON_BLOCK)

/* completion ast to be executed */
#define LDLM_FL_CP_REQD        0x1000000

/* cleanup_resource has already handled the lock */
#define LDLM_FL_CLEANED        0x2000000

/* optimization hint: LDLM can run blocking callback from current context
 * w/o involving separate thread. in order to decrease cs rate */
#define LDLM_FL_ATOMIC_CB      0x4000000

/* It may happen that a client initiate 2 operations, e.g. unlink and mkdir,
 * such that server send blocking ast for conflict locks to this client for
 * the 1st operation, whereas the 2nd operation has canceled this lock and
 * is waiting for rpc_lock which is taken by the 1st operation.
 * LDLM_FL_BL_AST is to be set by ldlm_callback_handler() to the lock not allow
 * ELC code to cancel it. 
 * LDLM_FL_BL_DONE is to be set by ldlm_cancel_callback() when lock cache is
 * droped to let ldlm_callback_handler() return EINVAL to the server. It is
 * used when ELC rpc is already prepared and is waiting for rpc_lock, too late
 * to send a separate CANCEL rpc. */
#define LDLM_FL_BL_AST          0x10000000
#define LDLM_FL_BL_DONE         0x20000000

/* measure lock contention and return -EUSERS if locking contention is high */
#define LDLM_FL_DENY_ON_CONTENTION 0x40000000

/* These are flags that are mapped into the flags and ASTs of blocking locks */
#define LDLM_AST_DISCARD_DATA  0x80000000 /* Add FL_DISCARD to blocking ASTs */

/* Flags sent in AST lock_flags to be mapped into the receiving lock. */
#define LDLM_AST_FLAGS         (LDLM_FL_DISCARD_DATA)

/* 
 * --------------------------------------------------------------------------
 * NOTE! Starting from this point, that is, LDLM_FL_* flags with values above
 * 0x80000000 will not be sent over the wire.
 * --------------------------------------------------------------------------
 */

/* Lock marked with this flag is going to fail to obtain cp_ast with emulating 
 * -EINTR while waiting. */
#define LDLM_FL_FAIL_LOC       0x100000000ULL

/* The blocking callback is overloaded to perform two functions.  These flags
 * indicate which operation should be performed. */
#define LDLM_CB_BLOCKING    1
#define LDLM_CB_CANCELING   2

typedef enum {
        ELDLM_OK = 0,

        ELDLM_LOCK_CHANGED = 300,
        ELDLM_LOCK_ABORTED = 301,
        ELDLM_LOCK_REPLACED = 302,
        ELDLM_NO_LOCK_DATA = 303,

        ELDLM_NAMESPACE_EXISTS = 400,
        ELDLM_BAD_NAMESPACE    = 401
} ldlm_error_t;

/*
 *
 * cluster name spaces
 *
 */

#define DLM_OST_NAMESPACE 1
#define DLM_MDS_NAMESPACE 2

typedef enum {
	LCK_MINMODE = 0,
	LCK_EX = 1,
	LCK_PW = 2,
	LCK_PR = 4,
	LCK_CW = 8,
	LCK_CR = 16,
	LCK_NL = 32,
	LCK_GROUP = 64,
	LCK_MAXMODE
} ldlm_mode_t;
#define LCK_MODE_NUM	7

typedef enum {
	LDLM_NAMESPACE_SERVER = 1 << 0,
	LDLM_NAMESPACE_CLIENT = 1 << 1
} ldlm_site_t;

struct ldlm_namespace {
	char		*ns_name;
	ldlm_site_t	ns_client;

	struct list_head *ns_hash; /* hash table for ns */
	__u32		ns_refcount; /* count of resources in the hash */
	struct list_head ns_root_list; /* all root resources in ns */
	
	struct list_head ns_unused_list; /* all root resources in ns */
	int		ns_nr_unused;

	atomic_t	ns_locks;
	__u64		ns_resources;

	/* If more than @ns_contended_locks found, the resource considered
	 * as contended */
	unsigned	ns_contended_locks;
	/* the resource remembers contended state during @ns_contention_time,
	 * in seconds */
	unsigned	ns_contention_time;
	/* limit size of nolock requests, in bytes */
	unsigned	ns_max_nolock_size;

	OBD	*ns_obd;
};

typedef enum {
	LDLM_PLAIN = 10,
	LDLM_EXTENT = 11,
	LDLM_FLOCK = 12,
	LDLM_IBITS = 13,
	LDLM_MAX_TYPE
} ldlm_type_t;

#define LDLM_MIN_TYPE LDLM_PLAIN

struct ldlm_extent {
	__u64 start;
	__u64 end;
	__u64 gid;
};

struct ldlm_inodebits {
	__u64 bits;
};

struct ldlm_flock {
	__u64 start;
	__u64 end;
	__u64 blocking_export; /* not actually used over the wire */
	__u32 blocking_pid; /* not actually used over the wire */
	__u32 pid;
};

/* it's important that the fields of the ldlm_extent structure match
 * the first fields of the ldlm_flock structure because there is only
 * one ldlm_swab routine to process the ldlm_policy_data_t union. if
 * this ever changes we will need to swab the union differently based
 * on the resource type. */

typedef union {
	struct ldlm_extent l_extent;
	struct ldlm_flock  l_flock;
	struct ldlm_inodebits l_inodebits;
} ldlm_policy_data_t;

struct ldlm_intent {
	__u64 opc;
};

/*
 *   LDLM requests:
 */
/* opcodes -- MUST be distinct from OST/MDS opcodes */
typedef enum {
        LDLM_ENQUEUE     = 101,
        LDLM_CONVERT     = 102,
        LDLM_CANCEL      = 103,
        LDLM_BL_CALLBACK = 104,
        LDLM_CP_CALLBACK = 105,
        LDLM_GL_CALLBACK = 106,
        LDLM_LAST_OPC
} ldlm_cmd_t;
#define LDLM_FIRST_OPC LDLM_ENQUEUE

#define RES_NAME_SIZE	4
struct ldlm_res_id {
	__u64 name[RES_NAME_SIZE];
};

//struct ldlm_interval_tree;
/* Interval node data for each LDLM_EXTENT lock */
struct ldlm_interval {
        struct interval_node li_node;   /* node for tree mgmt */
        struct list_head     li_group;  /* the locks which have the same 
                                         * policy - group of the policy */
};
#define to_ldlm_interval(n) container_of(n, struct ldlm_interval, li_node)

/* the interval tree must be accessed inside the resource lock. */
struct ldlm_interval_tree {
        /* tree size, this variable is used to count
         * granted PW locks in ldlm_extent_policy()*/
        int                   lit_size;
        ldlm_mode_t           lit_mode; /* lock mode */
        struct interval_node *lit_root; /* actually ldlm_interval */
};

struct ldlm_resource {
	struct ldlm_namespace 	*lr_namespace;

	struct list_head	lr_hash;
	struct ldlm_resource	*lr_parent; /* 0 for a root resource */
	struct list_head	lr_children; /* list head for child resources */
	struct list_head	lr_childof; /* part of ns_root_list if root res,*/
					    /* part of lr_children if child. */

	struct list_head	lr_granted;
	struct list_head	lr_converting;
	struct list_head	lr_waiting;
	ldlm_mode_t		lr_most_restr;
	ldlm_type_t		lr_type; /*LDLM_{PLAIN, EXTENT, FLOCK} */
	struct ldlm_res_id	lr_name;
	atomic_t		lr_refcount;

	struct ldlm_interval_tree lr_itree[LCK_MODE_NUM];  /* interval trees*/

	__u32			lr_lvb_len;
	void			*lr_lvb_data;

	/* when the resource was considered as contended */
	cfs_time_t		lr_contention_time;
};

struct ldlm_lock;

typedef int (*ldlm_blocking_callback)(struct ldlm_lock *lock,
                                      struct ldlm_lock_desc *newl, void *data,
                                      int flag);
typedef int (*ldlm_completion_callback)(struct ldlm_lock *lock, int flags,
                                        void *data);
typedef int (*ldlm_glimpse_callback)(struct ldlm_lock *lock, void *data);

struct sl_insert_point {
        struct list_head *res_link;
        struct list_head *mode_link;
        struct list_head *policy_link;
};

struct ldlm_lock {
	atomic_t		l_refc;
	struct ldlm_resource	*l_resource;
	struct list_head	l_lru;
	struct list_head	l_res_link;
	struct hlist_node	l_exp_hash;

	struct ldlm_interval	*l_tree_node;

	ldlm_mode_t		l_req_mode;
	ldlm_mode_t		l_granted_mode;
	
	ldlm_completion_callback l_completion_ast;
        ldlm_blocking_callback   l_blocking_ast;
        ldlm_glimpse_callback    l_glimpse_ast;
	
	ldlm_policy_data_t	l_policy_data;
	__u64			l_flags;
	__u32			l_readers;
	__u32			l_writers;
	__u8			l_destroyed;

	cfs_time_t		l_last_activity; /* seconds */
	cfs_time_t		l_last_used; /* jiffies */
	struct ldlm_extent	l_req_extent;

	/* Client-side-only members */
	__u32			l_lvb_len;
	void			*l_lvb_data;
	void			*l_ast_data;
	struct list_head	l_extents_list;

	struct list_head	l_pending_chain; /* callbacks pending */
	cfs_time_t		l_callback_timeout; /* jiffies */

	__u32			l_pid; /* pid which created this lock */

	struct list_head	l_bl_ast;
	struct list_head	l_cp_ast;
	struct ldlm_lock	*l_blocking_lock;
	int			l_bl_ast_run;

	struct list_head	l_sl_mode;
	struct list_head	l_sl_policy;

	OBD			*l_export;
};

/* compatibility matrix */
#define LCK_COMPAT_EX  LCK_NL
#define LCK_COMPAT_PW  (LCK_COMPAT_EX | LCK_CR)
#define LCK_COMPAT_PR  (LCK_COMPAT_PW | LCK_PR)
#define LCK_COMPAT_CW  (LCK_COMPAT_PW | LCK_CW)
#define LCK_COMPAT_CR  (LCK_COMPAT_CW | LCK_PR | LCK_PW)
#define LCK_COMPAT_NL  (LCK_COMPAT_CR | LCK_EX | LCK_GROUP)
#define LCK_COMPAT_GROUP  (LCK_GROUP | LCK_NL)

typedef int (*ldlm_processing_policy)(struct ldlm_lock *lock, int *flags,
                                      int first_enq, ldlm_error_t *err,
                                      struct list_head *work_list);

/* Number of blocking/completion callbacks that will be sent in
 * parallel (see bug 11301). */
#define PARALLEL_AST_LIMIT      200

struct ldlm_cb_set_arg {
        struct ptlrpc_request_set *set;
        atomic_t restart;
        __u32 type; /* LDLM_BL_CALLBACK or LDLM_CP_CALLBACK */
};

struct ldlm_resource_desc {
        ldlm_type_t lr_type;
        __u32 lr_padding;       /* also fix lustre_swab_ldlm_resource_desc */
        struct ldlm_res_id lr_name;
};

struct ldlm_lock_desc {
        struct ldlm_resource_desc l_resource;
        ldlm_mode_t l_req_mode;
        ldlm_mode_t l_granted_mode;
        ldlm_policy_data_t l_policy_data;
};

/* ldlm_enqueue parameters common */
struct ldlm_enqueue_info {
        __u32 ei_type;   /* Type of the lock being enqueued. */
        __u32 ei_mode;   /* Mode of the lock being enqueued. */
        void *ei_cb_bl;  /* Different callbacks for lock handling (blocking, */
        void *ei_cb_cp;  /* completion, glimpse) */
        void *ei_cb_gl;
        void *ei_cbdata; /* Data to be passed into callbacks. */
};

class LDLM{

	static int ref;
	static ldlm_mode_t lck_compat_array[LCK_MAXMODE];
	static ldlm_processing_policy ldlm_processing_policy_table[LDLM_MAX_TYPE];
	struct ldlm_namespace ns;

	static int ldlm_process_plain_lock(struct ldlm_lock *lock, int *flags, int first_enq,
                            ldlm_error_t *err, struct list_head *work_list);
	static int ldlm_process_flock_lock(struct ldlm_lock *req, int *flags, int first_enq,
                        ldlm_error_t *err, struct list_head *work_list);
	static int ldlm_process_inodebits_lock(struct ldlm_lock *lock, int *flags,
                                int first_enq, ldlm_error_t *err,
                                struct list_head *work_list);
	static int ldlm_process_extent_lock(struct ldlm_lock *lock, int *flags, int first_enq,
                             ldlm_error_t *err, struct list_head *work_list);

/*
 * Iterators.
 */

	#define LDLM_ITER_CONTINUE 1 /* keep iterating */
	#define LDLM_ITER_STOP     2 /* stop iterating */

	typedef int (*ldlm_iterator_t)(struct ldlm_lock *, void *);
	typedef int (*ldlm_res_iterator_t)(struct ldlm_resource *, void *);

	static int ldlm_resource_foreach(struct ldlm_resource *res, ldlm_iterator_t iter,
                          void *closure);
/*
int ldlm_namespace_foreach(struct ldlm_namespace *ns, ldlm_iterator_t iter,
                           void *closure);
int ldlm_namespace_foreach_res(struct ldlm_namespace *ns,
                               ldlm_res_iterator_t iter, void *closure);
*/

	void ldlm_resource_iterate(struct ldlm_namespace *ns, struct ldlm_res_id *res_id,
                           ldlm_iterator_t iter, void *data);

	static inline void lockmode_verify(ldlm_mode_t mode)
	{
		LASSERT(mode > LCK_MINMODE && mode < LCK_MAXMODE);
	}

	static inline int lockmode_compat(ldlm_mode_t exist, ldlm_mode_t newmode)
	{
		return (lck_compat_array[exist] & newmode);
	}


/*
 * namespace operations
 */

	static void ldlm_namespace_get(struct ldlm_namespace *ns);
	static void ldlm_namespace_put(struct ldlm_namespace *ns);

	static inline int ns_is_client(struct ldlm_namespace *ns)
	{
        	LASSERT(ns != NULL);
        	LASSERT(!(ns->ns_client & ~(LDLM_NAMESPACE_CLIENT |
                                    LDLM_NAMESPACE_SERVER)));
        	LASSERT(ns->ns_client == LDLM_NAMESPACE_CLIENT ||
                	ns->ns_client == LDLM_NAMESPACE_SERVER);
        	return ns->ns_client == LDLM_NAMESPACE_CLIENT;
	}

	static inline int ns_is_server(struct ldlm_namespace *ns)
	{
        	LASSERT(ns != NULL);
        	LASSERT(!(ns->ns_client & ~(LDLM_NAMESPACE_CLIENT |
                                    LDLM_NAMESPACE_SERVER)));
        	LASSERT(ns->ns_client == LDLM_NAMESPACE_CLIENT ||
                	ns->ns_client == LDLM_NAMESPACE_SERVER);
        	return ns->ns_client == LDLM_NAMESPACE_SERVER;
	}

/*
 * resource operations
 */
	static struct ldlm_resource* ldlm_resource_new();

	static struct ldlm_resource * ldlm_resource_find(struct ldlm_namespace *ns, 
			struct ldlm_res_id name, __u32 hash);

	static struct ldlm_resource *ldlm_resource_add(struct ldlm_namespace *ns, 
			struct ldlm_resource *parent, struct ldlm_res_id name, __u32 hash, 
			ldlm_type_t type);

	#define RES_HASH_BITS 10
	#define RES_HASH_SIZE (1UL << RES_HASH_BITS)
	#define RES_HASH_MASK (RES_HASH_SIZE - 1)
	static __u32 ldlm_hash_fn(struct ldlm_resource *parent, struct ldlm_res_id name);

	static struct ldlm_resource *ldlm_resource_getref(struct ldlm_resource *res);
	static struct ldlm_resource *ldlm_resource_get(struct ldlm_namespace *ns, 
			struct ldlm_resource *parent, struct ldlm_res_id name, 
			ldlm_type_t type, int create);

	static void __ldlm_resource_putref_final(struct ldlm_resource *res);
	static int ldlm_resource_putref(struct ldlm_resource *res);

	static void ldlm_resource_unlink_lock(struct ldlm_lock *lock);
	static void ldlm_resource_add_lock(struct ldlm_resource *res, struct list_head *head,
                            struct ldlm_lock *lock);
	static void ldlm_resource_insert_lock_after(struct ldlm_lock *original, struct ldlm_lock *newlock);
	static void ldlm_res2desc(struct ldlm_resource *res, struct ldlm_resource_desc *desc);

/*
 * lock operations
 */

	/*
	static struct ldlm_lock *__ldlm_handle2lock(struct lustre_handle *handle, int flags);
	static inline struct ldlm_lock *ldlm_handle2lock(struct lustre_handle *h)
	{
		return __ldlm_handle2lock(h,0);
	}
	*/

	static struct ldlm_lock *ldlm_lock_get(struct ldlm_lock *lock);
	static void ldlm_lock_put(struct ldlm_lock *lock);

	static void ldlm_lock_free(struct ldlm_lock *lock, size_t size);

	static int ldlm_lock_remove_from_lru(struct ldlm_lock *lock);
	static void ldlm_lock_add_to_lru(struct ldlm_lock *lock);
	static void ldlm_lock_addref_internal(struct ldlm_lock *lock, __u32 mode);
	static void ldlm_lock_touch_in_lru(struct ldlm_lock *lock);
	/*
	 * usage: pass in a resource on which you have done ldlm_resource_get
	 *        pass in a parent lock on which you have done a ldlm_lock_get
	 *        after return, ldlm_*_put the resource and parent
	 * returns: lock with refcount 2 - one for current caller and one for remote
	 */
	static struct ldlm_lock *ldlm_lock_new(struct ldlm_resource *resource);

	static struct ldlm_lock *ldlm_lock_create(struct ldlm_namespace *ns,
                                   struct ldlm_res_id res_id, ldlm_type_t type,
                                   ldlm_mode_t mode,
                                   ldlm_blocking_callback blocking,
                                   ldlm_completion_callback completion,
                                   ldlm_glimpse_callback glimpse,
                                   void *data, __u32 lvb_len);

	/* returns a referenced lock or NULL.  See the flag descriptions below, in the
	 * comment above ldlm_lock_match */
	static struct ldlm_lock *search_queue(struct list_head *queue,
                                      ldlm_mode_t *mode,
                                      ldlm_policy_data_t *policy,
                                      struct ldlm_lock *old_lock, int flags);

	static ldlm_mode_t ldlm_lock_match(struct ldlm_namespace *ns, int flags,
                            struct ldlm_res_id *res_id, ldlm_type_t type,
                            ldlm_policy_data_t *policy, ldlm_mode_t mode,
                            struct lustre_handle *lockh);

	static void ldlm_unlink_lock_skiplist(struct ldlm_lock *req);

	ldlm_error_t ldlm_lock_enqueue(struct ldlm_namespace *ns,
                               struct ldlm_lock **lockp,
                               void *cookie, int *flags);

	static int ldlm_lock_destroy_internal(struct ldlm_lock *lock);
	static void ldlm_lock_destroy(struct ldlm_lock *lock);

	static void ldlm_granted_list_add_lock(struct ldlm_lock *lock,
                                       struct sl_insert_point *prev);
	static void search_granted_lock(struct list_head *queue,
                                struct ldlm_lock *req,
                                struct sl_insert_point *prev);
	static void ldlm_grant_lock_with_skiplist(struct ldlm_lock *lock);
	static void ldlm_grant_lock(struct ldlm_lock *lock, struct list_head *work_list);

	static void ldlm_add_cp_work_item(struct ldlm_lock *lock, struct list_head *work_list);
	static void ldlm_add_bl_work_item(struct ldlm_lock *lock, struct ldlm_lock *newlock,
                           struct list_head *work_list);
	static void ldlm_add_ast_work_item(struct ldlm_lock *lock, struct ldlm_lock *newlock,
                                struct list_head *work_list);
	static int ldlm_run_bl_ast_work(struct list_head *rpc_list);

	static void ldlm_lock2desc(struct ldlm_lock *lock, struct ldlm_lock_desc *desc);

	static void ldlm_send_and_maybe_create_set(struct ldlm_cb_set_arg *arg, int do_create);
/*
 * extent lock
 */
	#define LDLM_MAX_GROWN_EXTENT (32 * 1024 * 1024 - 1)

	static void ldlm_interval_attach(struct ldlm_interval *n, struct ldlm_lock *l);
	static struct ldlm_interval *ldlm_interval_detach(struct ldlm_lock *l);
	static void ldlm_interval_free(struct ldlm_interval *node);

	static inline int lock_mode_to_index(ldlm_mode_t mode)
	{
        	int index, tmp = (int)mode;

        	LASSERT(tmp != 0);
        	//LASSERT(IS_PO2(mode));
        	for (index = -1; tmp; index++, tmp >>= 1) ;
        	LASSERT(index < LCK_MODE_NUM);
        	return index;
	}

	static inline struct ldlm_extent *
	ldlm_interval_extent(struct ldlm_interval *node)
	{
        	struct ldlm_lock *lock;
        	LASSERT(!list_empty(&node->li_group));

        	lock = list_entry(node->li_group.next, struct ldlm_lock, l_sl_policy);
        	return &lock->l_policy_data.l_extent;
	}
	static void ldlm_extent_unlink_lock(struct ldlm_lock *lock);
	static void ldlm_extent_add_lock(struct ldlm_resource *res,
                          struct ldlm_lock *lock);
	static int ldlm_extent_compat_queue(struct list_head *queue, struct ldlm_lock *req,
                         int *flags, ldlm_error_t *err,
                         struct list_head *work_list, int *contended_locks);

	static enum interval_iter ldlm_extent_compat_cb(struct interval_node *n,
                                                void *data);

	static int ldlm_check_contention(struct ldlm_lock *lock, int contended_locks);

	static inline int ldlm_extent_overlap(struct ldlm_extent *ex1,
                                      struct ldlm_extent *ex2)
	{
        	return (ex1->start <= ex2->end) && (ex2->start <= ex1->end);
	}
	static void ldlm_extent_internal_policy_fixup(struct ldlm_lock *req,
                                              struct ldlm_extent *new_ex,
                                              int conflicting);
	static void ldlm_extent_internal_policy_granted(struct ldlm_lock *req,
                                                struct ldlm_extent *new_ex);
	static void ldlm_extent_internal_policy_waiting(struct ldlm_lock *req,
                                    struct ldlm_extent *new_ex);
	static void ldlm_extent_policy(struct ldlm_resource *res, struct ldlm_lock *lock, int *flags);

	static void discard_bl_list(struct list_head *bl_list);

	#define spin_lock_init(lock)
	#define spin_lock(lock)
	#define spin_unlock(lock)
	#define lock_res(res)
	#define unlock_res(res)
	#define lock_res_and_lock(lock)
	#define unlock_res_and_lock(lock)
	#define check_res_locked(res)

	#define ldlm_lock_addref_internal_nolock(lock, match) ldlm_lock_addref_internal(lock, match)
	#define ldlm_lock_remove_from_lru_nolock(lock) ldlm_lock_remove_from_lru(lock)
	#define ldlm_lock_add_to_lru_nolock(lock) ldlm_lock_add_to_lru(lock)
	#define ldlm_namespace_put_locked(ns, wakeup) ldlm_namespace_put(ns)
	#define ldlm_lock_destroy_nolock(lock) ldlm_lock_destroy(lock)
public:
	LDLM();

	~LDLM();

	int init_namespace(OBD *obd, char *name, ldlm_site_t client);

	static void ldlm_test();
	static void ldlm_test_SM(void *data);
	static void ldlm_test_1(void *data);
	//static void ldlm_test_1(int a, int *b);
};

#endif
