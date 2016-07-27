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
#include "ldlm.h"

ldlm_mode_t LDLM::lck_compat_array[LCK_MAXMODE];
ldlm_processing_policy LDLM::ldlm_processing_policy_table[LDLM_MAX_TYPE];
int LDLM::ref;

LDLM::LDLM()
{
	++ref;
	if (ref == 1) {
		lck_compat_array[LCK_EX] = (ldlm_mode_t)LCK_COMPAT_EX;
        	lck_compat_array[LCK_PW] = (ldlm_mode_t)LCK_COMPAT_PW;
        	lck_compat_array[LCK_PR] = (ldlm_mode_t)LCK_COMPAT_PR;
        	lck_compat_array[LCK_CW] = (ldlm_mode_t)LCK_COMPAT_CW;
        	lck_compat_array[LCK_CR] = (ldlm_mode_t)LCK_COMPAT_CR;
        	lck_compat_array[LCK_NL] = (ldlm_mode_t)LCK_COMPAT_NL;
        	lck_compat_array[LCK_GROUP] = (ldlm_mode_t)LCK_COMPAT_GROUP;

		ldlm_processing_policy_table[LDLM_PLAIN] = ldlm_process_plain_lock;
		ldlm_processing_policy_table[LDLM_EXTENT] = ldlm_process_extent_lock;
		ldlm_processing_policy_table[LDLM_FLOCK] = ldlm_process_flock_lock;
		ldlm_processing_policy_table[LDLM_IBITS] = ldlm_process_inodebits_lock;
	}
}


LDLM::~LDLM()
{
}

int LDLM::init_namespace(OBD *obd, char *name, ldlm_site_t client)
{
	int rc, namelen;

	namelen = strlen(name);
	ns.ns_name = (char *) malloc(namelen + 1);
	strcpy(ns.ns_name, name);
}

int LDLM::ldlm_resource_foreach(struct ldlm_resource *res, ldlm_iterator_t iter,
                          void *closure)
{
	struct list_head *tmp, *next;
        struct ldlm_lock *lock;
        int rc = LDLM_ITER_CONTINUE;

        ENTRY;

        if (!res)
                RETURN(LDLM_ITER_CONTINUE);

        lock_res(res);
        list_for_each_safe(tmp, next, &res->lr_granted) {
                lock = list_entry(tmp, struct ldlm_lock, l_res_link);

                if (iter(lock, closure) == LDLM_ITER_STOP)
                        GOTO(out, rc = LDLM_ITER_STOP);
        }

        list_for_each_safe(tmp, next, &res->lr_converting) {
                lock = list_entry(tmp, struct ldlm_lock, l_res_link);

                if (iter(lock, closure) == LDLM_ITER_STOP)
                        GOTO(out, rc = LDLM_ITER_STOP);
        }

        list_for_each_safe(tmp, next, &res->lr_waiting) {
                lock = list_entry(tmp, struct ldlm_lock, l_res_link);

                if (iter(lock, closure) == LDLM_ITER_STOP)
                        GOTO(out, rc = LDLM_ITER_STOP);
        }
out:
        unlock_res(res);
        RETURN(rc);
}

/*
int LDLM::ldlm_namespace_foreach(struct ldlm_namespace *ns, ldlm_iterator_t iter,
                           void *closure)
{

}
int LDLM::ldlm_namespace_foreach_res(struct ldlm_namespace *ns,
                               ldlm_res_iterator_t iter, void *closure)
{

}
*/

void LDLM::ldlm_resource_iterate(struct ldlm_namespace *ns, struct ldlm_res_id *res_id,
                           ldlm_iterator_t iter, void *data)
{
	
}

void LDLM::ldlm_namespace_get(struct ldlm_namespace *ns)
{
	ns->ns_refcount++;
	/*if (ns->ns_refcount == 0 && wakeup)
                wake_up(&ns->ns_waitq);
	*/
}

void LDLM::ldlm_namespace_put(struct ldlm_namespace *ns)
{
	ns->ns_refcount--;
}

void LDLM::ldlm_res2desc(struct ldlm_resource *res, struct ldlm_resource_desc *desc)
{
        desc->lr_type = res->lr_type;
        desc->lr_name = res->lr_name;
}

struct ldlm_resource* LDLM::ldlm_resource_new()
{
 	struct ldlm_resource *res;
        int idx;

        /*OBD_SLAB_ALLOC(res, ldlm_resource_slab, CFS_ALLOC_IO, sizeof *res);
        if (res == NULL)
                return NULL;*/

	res = new ldlm_resource;

        memset(res, 0, sizeof(*res));

        CFS_INIT_LIST_HEAD(&res->lr_children);
        CFS_INIT_LIST_HEAD(&res->lr_childof);
        CFS_INIT_LIST_HEAD(&res->lr_granted);
        CFS_INIT_LIST_HEAD(&res->lr_converting);
        CFS_INIT_LIST_HEAD(&res->lr_waiting);

#if 0
        /* initialize interval trees for each lock mode*/
        for (idx = 0; idx < LCK_MODE_NUM; idx++) {
                res->lr_itree[idx].lit_size = 0;
                res->lr_itree[idx].lit_mode = 1 << idx;
                res->lr_itree[idx].lit_root = NULL;
        }
#endif

        atomic_set(&res->lr_refcount, 1);
//        spin_lock_init(&res->lr_lock);

        /* one who creates the resource must unlock
         * the semaphore after lvb initialization */
//        init_MUTEX_LOCKED(&res->lr_lvb_sem);

        return res;
}

struct ldlm_resource *LDLM::ldlm_resource_find(struct ldlm_namespace *ns, 
			struct ldlm_res_id name, __u32 hash)
{
	struct list_head *bucket, *tmp;
        struct ldlm_resource *res;

        //LASSERT_SPIN_LOCKED(&ns->ns_hash_lock);
        bucket = ns->ns_hash + hash;

        list_for_each(tmp, bucket) {
                res = list_entry(tmp, struct ldlm_resource, lr_hash);
                if (memcmp(&res->lr_name, &name, sizeof(res->lr_name)) == 0)
                        return res;
        }

        return NULL;
}

struct ldlm_resource *LDLM::ldlm_resource_add(struct ldlm_namespace *ns, 
		struct ldlm_resource *parent, struct ldlm_res_id name, __u32 hash, 
		ldlm_type_t type)
{
	struct list_head *bucket;
        struct ldlm_resource *res, *old_res;
        ENTRY;

        LASSERTF(type >= LDLM_MIN_TYPE && type < LDLM_MAX_TYPE,
                 "type: %d\n", type);

        res = ldlm_resource_new();
        if (!res)
                RETURN(NULL);

        res->lr_name = name;
        res->lr_namespace = ns;
        res->lr_type = type;
        res->lr_most_restr = LCK_NL;

#if 0
        //spin_lock(&ns->ns_hash_lock);
        old_res = ldlm_resource_find(ns, name, hash);
        if (old_res) {
                /* someone won the race and added the resource before */
                ldlm_resource_getref(old_res);
                spin_unlock(&ns->ns_hash_lock);
                OBD_SLAB_FREE(res, ldlm_resource_slab, sizeof *res);
                /* synchronize WRT resource creation */
                if (ns->ns_lvbo && ns->ns_lvbo->lvbo_init) {
                        down(&old_res->lr_lvb_sem);
                        up(&old_res->lr_lvb_sem);
                }
                RETURN(old_res);
        }
#endif

	 /* we won! let's add the resource */
        bucket = ns->ns_hash + hash;
        list_add(&res->lr_hash, bucket);
        ns->ns_resources++;
        ldlm_namespace_get(ns);

        if (parent == NULL) {
                list_add(&res->lr_childof, &ns->ns_root_list);
        } else {
                res->lr_parent = parent;
                list_add(&res->lr_childof, &parent->lr_children);
        }
        //spin_unlock(&ns->ns_hash_lock);

#if 0
        if (ns->ns_lvbo && ns->ns_lvbo->lvbo_init) {
                int rc;

                OBD_FAIL_TIMEOUT(OBD_FAIL_LDLM_CREATE_RESOURCE, 2);
                rc = ns->ns_lvbo->lvbo_init(res);
                if (rc)
                        CERROR("lvbo_init failed for resource "
                               LPU64": rc %d\n", name.name[0], rc);
                /* we create resource with locked lr_lvb_sem */
                up(&res->lr_lvb_sem);
        }
#endif

        RETURN(res);
}

__u32 LDLM::ldlm_hash_fn(struct ldlm_resource *parent, struct ldlm_res_id name)
{
	__u32 hash = 0;
        int i;

        for (i = 0; i < RES_NAME_SIZE; i++)
                hash += name.name[i];

        hash += (__u32)((unsigned long)parent >> 4);

        return (hash & RES_HASH_MASK);
}

struct ldlm_resource *LDLM::ldlm_resource_getref(struct ldlm_resource *res)
{
	LASSERT(res != NULL);
        //LASSERT(res != LP_POISON);
        atomic_inc(&res->lr_refcount);

        return res;
}

struct ldlm_resource *LDLM::
ldlm_resource_get(struct ldlm_namespace *ns, struct ldlm_resource *parent,
                  struct ldlm_res_id name, ldlm_type_t type, int create)
{
	__u32 hash = ldlm_hash_fn(parent, name);
        struct ldlm_resource *res = NULL;
        ENTRY;

        LASSERT(ns != NULL);
        LASSERT(ns->ns_hash != NULL);
        LASSERT(name.name[0] != 0);

        spin_lock(&ns->ns_hash_lock);
        res = ldlm_resource_find(ns, name, hash);
        if (res) {
                ldlm_resource_getref(res);
                spin_unlock(&ns->ns_hash_lock);
		#if 0
                /* synchronize WRT resource creation */
                if (ns->ns_lvbo && ns->ns_lvbo->lvbo_init) {
                        down(&res->lr_lvb_sem);
                        up(&res->lr_lvb_sem);
                }
		#endif
                RETURN(res);
        }
        spin_unlock(&ns->ns_hash_lock);

        if (create == 0)
                RETURN(NULL);

        res = ldlm_resource_add(ns, parent, name, hash, type);
        RETURN(res);
}

void LDLM::__ldlm_resource_putref_final(struct ldlm_resource *res)
{
	struct ldlm_namespace *ns = res->lr_namespace;

        //LASSERT_SPIN_LOCKED(&ns->ns_hash_lock);

        if (!list_empty(&res->lr_granted)) {
                //ldlm_resource_dump(D_ERROR, res);
                LBUG();
        }

        if (!list_empty(&res->lr_converting)) {
                //ldlm_resource_dump(D_ERROR, res);
                LBUG();
        }

        if (!list_empty(&res->lr_waiting)) {
                //ldlm_resource_dump(D_ERROR, res);
                LBUG();
        }

        if (!list_empty(&res->lr_children)) {
                //ldlm_resource_dump(D_ERROR, res);
                LBUG();
        }

        /* Pass 0 as second argument to not wake up ->ns_waitq yet, will do it
         * later. */
        ldlm_namespace_put_locked(ns, 0);
        list_del_init(&res->lr_hash);
        list_del_init(&res->lr_childof);

        ns->ns_resources--;
/*
        if (ns->ns_resources == 0)
                wake_up(&ns->ns_waitq);
*/
}

int LDLM::ldlm_resource_putref(struct ldlm_resource *res)
{
	struct ldlm_namespace *ns = res->lr_namespace;
        int rc = 0;
        ENTRY;
/*
        CDEBUG(D_INFO, "putref res: %p count: %d\n", res,
               atomic_read(&res->lr_refcount) - 1);
        LASSERTF(atomic_read(&res->lr_refcount) > 0, "%d",
                 atomic_read(&res->lr_refcount));
        LASSERTF(atomic_read(&res->lr_refcount) < LI_POISON, "%d",
                 atomic_read(&res->lr_refcount));
*/

        if (atomic_dec_and_lock(&res->lr_refcount, &ns->ns_hash_lock)) {
                __ldlm_resource_putref_final(res);
                spin_unlock(&ns->ns_hash_lock);
                if (res->lr_lvb_data)
                        OBD_FREE(res->lr_lvb_data, res->lr_lvb_len);
                OBD_FREE(res, sizeof *res);
                rc = 1;
        }

        RETURN(rc);
}

#define LDLM_LOCK_GET(lock)     \
({                              \
	ldlm_lock_get(lock);    \
	lock;                   \
})

#define LDLM_LOCK_PUT(lock)     \
do {                            \
	ldlm_lock_put(lock);    \
} while(0)

#if 0
struct ldlm_lock *LDLM::__ldlm_handle2lock(struct lustre_handle *handle, int flags)
{
        struct ldlm_namespace *ns;
        struct ldlm_lock *lock = NULL, *retval = NULL;
        ENTRY;

        LASSERT(handle);

        //lock = class_handle2object(handle->cookie);
        if (lock == NULL)
                RETURN(NULL);

        LASSERT(lock->l_resource != NULL);
        ns = lock->l_resource->lr_namespace;
        LASSERT(ns != NULL);

        lock_res_and_lock(lock);

        /* It's unlikely but possible that someone marked the lock as
         * destroyed after we did handle2object on it */
        if (lock->l_destroyed) {
                unlock_res_and_lock(lock);
                //CDEBUG(D_INFO, "lock already destroyed: lock %p\n", lock);
                LDLM_LOCK_PUT(lock);
                GOTO(out, retval);
        }

        if (flags && (lock->l_flags & flags)) {
                unlock_res_and_lock(lock);
                LDLM_LOCK_PUT(lock);
                GOTO(out, retval);
        }

        if (flags)
                lock->l_flags |= flags;

        unlock_res_and_lock(lock);
        retval = lock;
	EXIT;
 out:
        return retval;
}
#endif

struct ldlm_lock *LDLM::ldlm_lock_get(struct ldlm_lock *lock)
{
	atomic_inc(&lock->l_refc);
        return lock;	
}

void LDLM::ldlm_lock_put(struct ldlm_lock *lock)
{
	ENTRY;

        //LASSERT(lock->l_resource != LP_POISON);
        LASSERT(atomic_read(&lock->l_refc) > 0);
        if (atomic_dec_and_test(&lock->l_refc)) {
                struct ldlm_resource *res;

                //LDLM_DEBUG(lock, "final lock_put on destroyed lock, freeing it.");

                res = lock->l_resource;
                LASSERT(lock->l_destroyed);
                LASSERT(list_empty(&lock->l_res_link));
                LASSERT(list_empty(&lock->l_pending_chain));

                atomic_dec(&res->lr_namespace->ns_locks);
                ldlm_resource_putref(res);
                lock->l_resource = NULL;
		#if 0
                if (lock->l_export) {
                        class_export_put(lock->l_export);
                        lock->l_export = NULL;
                }
		#endif

                if (lock->l_lvb_data != NULL)
                        OBD_FREE(lock->l_lvb_data, lock->l_lvb_len);

                //ldlm_interval_free(ldlm_interval_detach(lock));
                OBD_FREE_RCU_CB(lock, sizeof(*lock), &lock->l_handle,
                                ldlm_lock_free);
        }

        EXIT;
}

void LDLM::ldlm_lock_free(struct ldlm_lock *lock, size_t size)
{
	LASSERT(size == sizeof(*lock));
        OBD_FREE(lock, sizeof(*lock));
}

struct ldlm_lock *LDLM::ldlm_lock_new(struct ldlm_resource *resource)
{

	struct ldlm_lock *lock;
        ENTRY;

        if (resource == NULL)
                LBUG();

        OBD_ALLOC(lock, sizeof(*lock));
	if (lock == NULL)
                RETURN(NULL);

#if 0
	spin_lock_init(&lock->l_lock);
        lock->l_resource = ldlm_resource_getref(resource);

        atomic_set(&lock->l_refc, 2);
        CFS_INIT_LIST_HEAD(&lock->l_res_link);
        CFS_INIT_LIST_HEAD(&lock->l_lru);
        CFS_INIT_LIST_HEAD(&lock->l_pending_chain);
        CFS_INIT_LIST_HEAD(&lock->l_bl_ast);
        CFS_INIT_LIST_HEAD(&lock->l_cp_ast);
        //cfs_waitq_init(&lock->l_waitq);
        lock->l_blocking_lock = NULL;
        CFS_INIT_LIST_HEAD(&lock->l_sl_mode);
        CFS_INIT_LIST_HEAD(&lock->l_sl_policy);
        CFS_INIT_HLIST_NODE(&lock->l_exp_hash);

        atomic_inc(&resource->lr_namespace->ns_locks);
        CFS_INIT_LIST_HEAD(&lock->l_handle.h_link);
        class_handle_hash(&lock->l_handle, lock_handle_addref);

        CFS_INIT_LIST_HEAD(&lock->l_extents_list);
        spin_lock_init(&lock->l_extents_list_lock);
        CFS_INIT_LIST_HEAD(&lock->l_cache_locks_list);
        lock->l_callback_timeout = 0;
#endif
        RETURN(lock);
}

struct ldlm_lock *LDLM::ldlm_lock_create(struct ldlm_namespace *ns,
                                   struct ldlm_res_id res_id, ldlm_type_t type,
                                   ldlm_mode_t mode,
                                   ldlm_blocking_callback blocking,
                                   ldlm_completion_callback completion,
                                   ldlm_glimpse_callback glimpse,
                                   void *data, __u32 lvb_len)
{
	struct ldlm_lock *lock;
        struct ldlm_resource *res;
        ENTRY;

        res = ldlm_resource_get(ns, NULL, res_id, type, 1);
        if (res == NULL)
                RETURN(NULL);

        lock = ldlm_lock_new(res);
        ldlm_resource_putref(res);

        if (lock == NULL)
                RETURN(NULL);

        lock->l_req_mode = mode;
        lock->l_ast_data = data;
        lock->l_blocking_ast = blocking;
        lock->l_completion_ast = completion;
        lock->l_glimpse_ast = glimpse;
        lock->l_pid = cfs_curproc_pid();
#if 0
        lock->l_tree_node = NULL;
        /* if this is the extent lock, allocate the interval tree node */
        if (type == LDLM_EXTENT) {
                if (ldlm_interval_alloc(lock) == NULL)
                        GOTO(out, 0);
        }
#endif
        if (lvb_len) {
                lock->l_lvb_len = lvb_len;
                OBD_ALLOC(lock->l_lvb_data, lvb_len);
                if (lock->l_lvb_data == NULL)
                        GOTO(out, 0);
        }

        RETURN(lock);
out:
        if (lock->l_lvb_data)
                OBD_FREE(lock->l_lvb_data, lvb_len);
        //ldlm_interval_free(ldlm_interval_detach(lock));
        //OBD_SLAB_FREE(lock, ldlm_lock_slab, sizeof(*lock));
	OBD_FREE(lock, sizeof(*lock));
        return NULL;
}

void LDLM::ldlm_lock_add_to_lru(struct ldlm_lock *lock)
{
	struct ldlm_namespace *ns = lock->l_resource->lr_namespace;
        lock->l_last_used = cfs_time_current();
        LASSERT(list_empty(&lock->l_lru));
        LASSERT(lock->l_resource->lr_type != LDLM_FLOCK);
        list_add_tail(&lock->l_lru, &ns->ns_unused_list);
        LASSERT(ns->ns_nr_unused >= 0);
        ns->ns_nr_unused++;
}

int LDLM::ldlm_lock_remove_from_lru(struct ldlm_lock *lock)
{
	int rc = 0;
        if (!list_empty(&lock->l_lru)) {
                struct ldlm_namespace *ns = lock->l_resource->lr_namespace;
                LASSERT(lock->l_resource->lr_type != LDLM_FLOCK);
                list_del_init(&lock->l_lru);
                LASSERT(ns->ns_nr_unused > 0);
                ns->ns_nr_unused--;
                rc = 1;
        }
        return rc;
}

void LDLM::ldlm_lock_addref_internal(struct ldlm_lock *lock, __u32 mode)
{
	ldlm_lock_remove_from_lru(lock);
        if (mode & (LCK_NL | LCK_CR | LCK_PR))
                lock->l_readers++;
        if (mode & (LCK_EX | LCK_CW | LCK_PW | LCK_GROUP))
                lock->l_writers++;
        LDLM_LOCK_GET(lock);
}

void LDLM::ldlm_lock_touch_in_lru(struct ldlm_lock *lock)
{
	struct ldlm_namespace *ns = lock->l_resource->lr_namespace;
        ENTRY;
        spin_lock(&ns->ns_unused_lock);
        if (!list_empty(&lock->l_lru)) {
                ldlm_lock_remove_from_lru_nolock(lock);
                ldlm_lock_add_to_lru_nolock(lock);
        }
        spin_unlock(&ns->ns_unused_lock);
        EXIT;
}

struct ldlm_lock *LDLM::
search_queue(struct list_head *queue, ldlm_mode_t *mode, ldlm_policy_data_t *policy,
             	struct ldlm_lock *old_lock, int flags)
{
	struct ldlm_lock *lock;
        struct list_head *tmp;

        list_for_each(tmp, queue) {
                ldlm_mode_t match;

                lock = list_entry(tmp, struct ldlm_lock, l_res_link);

                if (lock == old_lock)
                        break;

                /* llite sometimes wants to match locks that will be
                 * canceled when their users drop, but we allow it to match
                 * if it passes in CBPENDING and the lock still has users.
                 * this is generally only going to be used by children
                 * whose parents already hold a lock so forward progress
                 * can still happen. */
                if (lock->l_flags & LDLM_FL_CBPENDING &&
                    !(flags & LDLM_FL_CBPENDING))
                        continue;
                if (lock->l_flags & LDLM_FL_CBPENDING &&
                    lock->l_readers == 0 && lock->l_writers == 0)
                        continue;

                if (!(lock->l_req_mode & *mode))
                        continue;

                match = lock->l_req_mode;
                if (lock->l_resource->lr_type == LDLM_EXTENT &&
                    (lock->l_policy_data.l_extent.start >
                     policy->l_extent.start ||
                     lock->l_policy_data.l_extent.end < policy->l_extent.end))
                        continue;

		if (unlikely(match == LCK_GROUP) &&
                    lock->l_resource->lr_type == LDLM_EXTENT &&
                    lock->l_policy_data.l_extent.gid != policy->l_extent.gid)
                        continue;

                /* We match if we have existing lock with same or wider set
                   of bits. */
                if (lock->l_resource->lr_type == LDLM_IBITS &&
                     ((lock->l_policy_data.l_inodebits.bits &
                      policy->l_inodebits.bits) !=
                      policy->l_inodebits.bits))
                        continue;

                if (lock->l_destroyed || (lock->l_flags & LDLM_FL_FAILED))
                        continue;

                if ((flags & LDLM_FL_LOCAL_ONLY) &&
                    !(lock->l_flags & LDLM_FL_LOCAL))
                        continue;

                if (flags & LDLM_FL_TEST_LOCK) {
                        LDLM_LOCK_GET(lock);
                        ldlm_lock_touch_in_lru(lock);
                } else {
                        ldlm_lock_addref_internal_nolock(lock, match);
                }
                *mode = match;
                return lock;
        }

        return NULL;
}

/* Can be called in two ways:
 *
 * If 'ns' is NULL, then lockh describes an existing lock that we want to look
 * for a duplicate of.
 *
 * Otherwise, all of the fields must be filled in, to match against.
 *
 * If 'flags' contains LDLM_FL_LOCAL_ONLY, then only match local locks on the
 *     server (ie, connh is NULL)
 * If 'flags' contains LDLM_FL_BLOCK_GRANTED, then only locks on the granted
 *     list will be considered
 * If 'flags' contains LDLM_FL_CBPENDING, then locks that have been marked
 *     to be canceled can still be matched as long as they still have reader
 *     or writer refernces
 * If 'flags' contains LDLM_FL_TEST_LOCK, then don't actually reference a lock,
 *     just tell us if we would have matched.
 *
 * Returns 1 if it finds an already-existing lock that is compatible; in this
 * case, lockh is filled in with a addref()ed lock
 */
ldlm_mode_t LDLM::ldlm_lock_match(struct ldlm_namespace *ns, int flags,
                            struct ldlm_res_id *res_id, ldlm_type_t type,
                            ldlm_policy_data_t *policy, ldlm_mode_t mode,
                            struct lustre_handle *lockh)
{
	struct ldlm_resource *res;
        struct ldlm_lock *lock, *old_lock = NULL;
        int rc = 0;
        ENTRY;

	LASSERT(ns != NULL);
	#if 0
        if (ns == NULL) {
                old_lock = ldlm_handle2lock(lockh);
                LASSERT(old_lock);

                ns = old_lock->l_resource->lr_namespace;
                res_id = &old_lock->l_resource->lr_name;
                type = old_lock->l_resource->lr_type;
                mode = old_lock->l_req_mode;
        }
	#endif

        res = ldlm_resource_get(ns, NULL, *res_id, type, 0);
        if (res == NULL) {
                LASSERT(old_lock == NULL);
                RETURN((ldlm_mode_t)0);
        }

        lock_res(res);

        lock = search_queue(&res->lr_granted, &mode, policy, old_lock, flags);
        if (lock != NULL)
                GOTO(out, rc = 1);
        if (flags & LDLM_FL_BLOCK_GRANTED)
                GOTO(out, rc = 0);
        lock = search_queue(&res->lr_converting, &mode, policy, old_lock, flags);
        if (lock != NULL)
                GOTO(out, rc = 1);
        lock = search_queue(&res->lr_waiting, &mode, policy, old_lock, flags);
        if (lock != NULL)
                GOTO(out, rc = 1);

	 EXIT;
 out:
        unlock_res(res);
        ldlm_resource_putref(res);
#if 0
        if (lock) {
                ldlm_lock2handle(lock, lockh);

		if ((flags & LDLM_FL_LVB_READY) && (!(lock->l_flags & LDLM_FL_LVB_READY))) {
                        struct l_wait_info lwi;
                        if (lock->l_completion_ast) {
                                int err = lock->l_completion_ast(lock,
                                                          LDLM_FL_WAIT_NOREPROC,
                                                                 NULL);
                                if (err) {
                                        if (flags & LDLM_FL_TEST_LOCK)
                                                LDLM_LOCK_PUT(lock);
                                        else
                                                ldlm_lock_decref_internal(lock, mode);
                                        rc = 0;
                                        goto out2;
                                }
                        }

                        lwi = LWI_TIMEOUT_INTR(cfs_time_seconds(obd_timeout), NULL,
                                               LWI_ON_SIGNAL_NOOP, NULL);

                        /* XXX FIXME see comment on CAN_MATCH in lustre_dlm.h */
                        l_wait_event(lock->l_waitq,
                                     (lock->l_flags & LDLM_FL_LVB_READY), &lwi);
                }
        }
#endif
 out2:
	if (old_lock)
                LDLM_LOCK_PUT(old_lock);
        if (flags & LDLM_FL_TEST_LOCK && rc)
                LDLM_LOCK_PUT(lock);

        return rc ? mode : (ldlm_mode_t)0;
}

int LDLM::ldlm_lock_destroy_internal(struct ldlm_lock *lock)
{
	//LASSERT()
	if (lock->l_destroyed) {
                LASSERT(list_empty(&lock->l_lru));
                EXIT;
                return 0;
        }
        lock->l_destroyed = 1;

/* // MODOOODO
        if (lock->l_export && lock->l_export->exp_lock_hash &&
            !hlist_unhashed(&lock->l_exp_hash))
                lustre_hash_del(lock->l_export->exp_lock_hash,
                                &lock->l_remote_handle, &lock->l_exp_hash);
*/
        ldlm_lock_remove_from_lru(lock);
        //class_handle_unhash(&lock->l_handle);

#if 0
        /* Wake anyone waiting for this lock */
        /* FIXME: I should probably add yet another flag, instead of using
         * l_export to only call this on clients */
        if (lock->l_export)
                class_export_put(lock->l_export);
        lock->l_export = NULL;
        if (lock->l_export && lock->l_completion_ast)
                lock->l_completion_ast(lock, 0);
#endif
        EXIT;
        return 1;
}

void LDLM::ldlm_lock_destroy(struct ldlm_lock *lock)
{
        int first;
        ENTRY;
        lock_res_and_lock(lock);
        first = ldlm_lock_destroy_internal(lock);
        unlock_res_and_lock(lock);

        /* drop reference from hashtable only for first destroy */
        if (first)
                LDLM_LOCK_PUT(lock);
        EXIT;
}

void LDLM::ldlm_unlink_lock_skiplist(struct ldlm_lock *req)
{
	if (req->l_resource->lr_type != LDLM_PLAIN &&
            req->l_resource->lr_type != LDLM_IBITS)
                return;

        list_del_init(&req->l_sl_policy);
        list_del_init(&req->l_sl_mode);
}

void LDLM::ldlm_resource_add_lock(struct ldlm_resource *res, struct list_head *head,
                            struct ldlm_lock *lock)
{
        //check_res_locked(res);

        //ldlm_resource_dump(D_INFO, res);
        //CDEBUG(D_OTHER, "About to add this lock:\n");
        //ldlm_lock_dump(D_OTHER, lock, 0);

        if (lock->l_destroyed) {
                //CDEBUG(D_OTHER, "Lock destroyed, not adding to resource\n");
                return;
        }

        LASSERT(list_empty(&lock->l_res_link));

        list_add_tail(&lock->l_res_link, head);
}

void LDLM::ldlm_resource_unlink_lock(struct ldlm_lock *lock)
{
        int type = lock->l_resource->lr_type;

        check_res_locked(lock->l_resource);
        if (type == LDLM_IBITS || type == LDLM_PLAIN)
                ldlm_unlink_lock_skiplist(lock);
        else if (type == LDLM_EXTENT)
                ldlm_extent_unlink_lock(lock);
        list_del_init(&lock->l_res_link);
}

void LDLM::ldlm_resource_insert_lock_after(struct ldlm_lock *original,
                                     struct ldlm_lock *newlock)
{
        struct ldlm_resource *res = original->l_resource;

        check_res_locked(res);

        //ldlm_resource_dump(D_INFO, res);
        //CDEBUG(D_OTHER, "About to insert this lock after %p:\n", original);
        //ldlm_lock_dump(D_OTHER, new, 0);

        if (newlock->l_destroyed) {
                //CDEBUG(D_OTHER, "Lock destroyed, not adding to resource\n");
                goto out;
        }

        LASSERT(list_empty(&newlock->l_res_link));

        list_add(&newlock->l_res_link, &original->l_res_link);
 out:;
}

void LDLM::ldlm_granted_list_add_lock(struct ldlm_lock *lock,
                                       struct sl_insert_point *prev)
{
        struct ldlm_resource *res = lock->l_resource;
        ENTRY;

        check_res_locked(res);

        //ldlm_resource_dump(D_INFO, res);
        //CDEBUG(D_OTHER, "About to add this lock:\n");
        //ldlm_lock_dump(D_OTHER, lock, 0);

        if (lock->l_destroyed) {
                //CDEBUG(D_OTHER, "Lock destroyed, not adding to resource\n");
                return;
        }

        LASSERT(list_empty(&lock->l_res_link));
        LASSERT(list_empty(&lock->l_sl_mode));
        LASSERT(list_empty(&lock->l_sl_policy));

        list_add(&lock->l_res_link, prev->res_link);
        list_add(&lock->l_sl_mode, prev->mode_link);
        list_add(&lock->l_sl_policy, prev->policy_link);

        EXIT;
}

/*
 * search_granted_lock
 *
 * Description:
 *      Finds a position to insert the new lock.
 * Parameters:
 *      queue [input]:  the granted list where search acts on;
 *      req [input]:    the lock whose position to be located;
 *      prev [output]:  positions within 3 lists to insert @req to
 * Return Value:
 *      filled @prev
 * NOTE: called by
 *  - ldlm_grant_lock_with_skiplist
 */
void LDLM::search_granted_lock(struct list_head *queue,
                                struct ldlm_lock *req,
                                struct sl_insert_point *prev)
{
        struct list_head *tmp;
        struct ldlm_lock *lock, *mode_end, *policy_end;
        ENTRY;

        list_for_each(tmp, queue) {
                lock = list_entry(tmp, struct ldlm_lock, l_res_link);

                mode_end = list_entry(lock->l_sl_mode.prev, struct ldlm_lock,
                                      l_sl_mode);

                if (lock->l_req_mode != req->l_req_mode) {
                        /* jump to last lock of mode group */
                        tmp = &mode_end->l_res_link;
                        continue;
                }

		/* suitable mode group is found */
                if (lock->l_resource->lr_type == LDLM_PLAIN) {
                        /* insert point is last lock of the mode group */
                        prev->res_link = &mode_end->l_res_link;
                        prev->mode_link = &mode_end->l_sl_mode;
                        prev->policy_link = &req->l_sl_policy;
                        EXIT;
                        return;
                } else if (lock->l_resource->lr_type == LDLM_IBITS) {
                        for (;;) {
                                policy_end = list_entry(lock->l_sl_policy.prev,
                                                        struct ldlm_lock,
                                                        l_sl_policy);

                                if (lock->l_policy_data.l_inodebits.bits ==
                                    req->l_policy_data.l_inodebits.bits) {
                                        /* insert point is last lock of
                                         * the policy group */
                                        prev->res_link =
                                                &policy_end->l_res_link;
                                        prev->mode_link =
                                                &policy_end->l_sl_mode;
                                        prev->policy_link =
                                                &policy_end->l_sl_policy;
                                        EXIT;
                                        return;
                                }

                                if (policy_end == mode_end)
                                        /* done with mode group */
                                        break;

                                /* jump to next policy group within the mode group */
				tmp = policy_end->l_res_link.next;
                                lock = list_entry(tmp, struct ldlm_lock,
                                                  l_res_link);
                        }  /* loop over policy groups within the mode group */

                        /* insert point is last lock of the mode group,
                         * new policy group is started */
                        prev->res_link = &mode_end->l_res_link;
                        prev->mode_link = &mode_end->l_sl_mode;
                        prev->policy_link = &req->l_sl_policy;
                        EXIT;
                        return;
                } else {
                        //LDLM_ERROR(lock, "is not LDLM_PLAIN or LDLM_IBITS lock");
                        LBUG();
                }
        }
	 /* insert point is last lock on the queue,
         * new mode group and new policy group are started */
        prev->res_link = queue->prev;
        prev->mode_link = &req->l_sl_mode;
        prev->policy_link = &req->l_sl_policy;
        EXIT;
        return;
}

void LDLM::ldlm_grant_lock_with_skiplist(struct ldlm_lock *lock)
{
        struct sl_insert_point prev;
        ENTRY;

        LASSERT(lock->l_req_mode == lock->l_granted_mode);

        search_granted_lock(&lock->l_resource->lr_granted, lock, &prev);
        ldlm_granted_list_add_lock(lock, &prev);
        EXIT;
}

void LDLM::ldlm_add_bl_work_item(struct ldlm_lock *lock, struct ldlm_lock *newlock,
                           struct list_head *work_list)
{
        if ((lock->l_flags & LDLM_FL_AST_SENT) == 0) {
                //LDLM_DEBUG(lock, "lock incompatible; sending blocking AST.");
                lock->l_flags |= LDLM_FL_AST_SENT;
                /* If the enqueuing client said so, tell the AST recipient to
                 * discard dirty data, rather than writing back. */
                if (newlock->l_flags & LDLM_AST_DISCARD_DATA)
                        lock->l_flags |= LDLM_FL_DISCARD_DATA;
                LASSERT(list_empty(&lock->l_bl_ast));
                list_add(&lock->l_bl_ast, work_list);
                LDLM_LOCK_GET(lock);
                LASSERT(lock->l_blocking_lock == NULL);
                lock->l_blocking_lock = LDLM_LOCK_GET(newlock);
        }
}

void LDLM::ldlm_add_cp_work_item(struct ldlm_lock *lock, struct list_head *work_list)
{
        if ((lock->l_flags & LDLM_FL_CP_REQD) == 0) {
                lock->l_flags |= LDLM_FL_CP_REQD;
                //LDLM_DEBUG(lock, "lock granted; sending completion AST.");
                LASSERT(list_empty(&lock->l_cp_ast));
                list_add(&lock->l_cp_ast, work_list);
                LDLM_LOCK_GET(lock);
        }
}

void LDLM::ldlm_add_ast_work_item(struct ldlm_lock *lock, struct ldlm_lock *newlock,
                                struct list_head *work_list)
{
        ENTRY;
        check_res_locked(lock->l_resource);
        if (newlock)
                ldlm_add_bl_work_item(lock, newlock, work_list);
        else
                ldlm_add_cp_work_item(lock, work_list);
        EXIT;
}

void LDLM::ldlm_lock2desc(struct ldlm_lock *lock, struct ldlm_lock_desc *desc)
{
	//struct obd_export *exp = lock->l_export?:lock->l_conn_export;
        /* INODEBITS_INTEROP: If the other side does not support
         * inodebits, reply with a plain lock descriptor.
         */
#if 0
        if ((lock->l_resource->lr_type == LDLM_IBITS) &&
            (exp && !(exp->exp_connect_flags & OBD_CONNECT_IBITS))) {
                struct ldlm_resource res = *lock->l_resource;

                /* Make sure all the right bits are set in this lock we
                   are going to pass to client */
                LASSERTF(lock->l_policy_data.l_inodebits.bits ==
                         (MDS_INODELOCK_LOOKUP|MDS_INODELOCK_UPDATE),
                         "Inappropriate inode lock bits during "
                         "conversion " LPU64 "\n",
                         lock->l_policy_data.l_inodebits.bits);
                res.lr_type = LDLM_PLAIN;
                ldlm_res2desc(&res, &desc->l_resource);
                /* Convert "new" lock mode to something old client can
                   understand */
                if ((lock->l_req_mode == LCK_CR) ||
                    (lock->l_req_mode == LCK_CW))
                        desc->l_req_mode = LCK_PR;
                else
                        desc->l_req_mode = lock->l_req_mode;
                if ((lock->l_granted_mode == LCK_CR) ||
                    (lock->l_granted_mode == LCK_CW)) {
                        desc->l_granted_mode = LCK_PR;
                } else {
                        /* We never grant PW/EX locks to clients */
                        LASSERT((lock->l_granted_mode != LCK_PW) &&
                                (lock->l_granted_mode != LCK_EX));
                        desc->l_granted_mode = lock->l_granted_mode;
                }

                /* We do not copy policy here, because there is no
                   policy for plain locks */
        } else
	#endif
	{
		ldlm_res2desc(lock->l_resource, &desc->l_resource);
                desc->l_req_mode = lock->l_req_mode;
                desc->l_granted_mode = lock->l_granted_mode;
                desc->l_policy_data = lock->l_policy_data;
        }
}

/* Helper function for pair ldlm_run_{bl,cp}_ast_work().
 * 
 * Send an existing rpc set specified by @arg->set and then
 * destroy it. Create new one if @do_create flag is set. */
void LDLM::
ldlm_send_and_maybe_create_set(struct ldlm_cb_set_arg *arg, int do_create)
{
        int rc;

        rc = Ptlrpc::ptlrpc_set_wait(arg->set);
        if (arg->type == LDLM_BL_CALLBACK)
                OBD_FAIL_TIMEOUT(OBD_FAIL_LDLM_GLIMPSE, 2);
        Ptlrpc::ptlrpc_set_destroy(arg->set);

        if (do_create)
                arg->set = Ptlrpc::ptlrpc_prep_set();
}

int LDLM::ldlm_run_bl_ast_work(struct list_head *rpc_list)
{
	struct ldlm_cb_set_arg arg;
        struct list_head *tmp, *pos;
        struct ldlm_lock_desc d;
        int ast_count;
        int rc = 0;
        ENTRY;

        arg.set = Ptlrpc::ptlrpc_prep_set();
        atomic_set(&arg.restart, 0);
        arg.type = LDLM_BL_CALLBACK;

        ast_count = 0;
        list_for_each_safe(tmp, pos, rpc_list) {
                struct ldlm_lock *lock =
                        list_entry(tmp, struct ldlm_lock, l_bl_ast);

                /* nobody should touch l_bl_ast */
                lock_res_and_lock(lock);
                list_del_init(&lock->l_bl_ast);

                LASSERT(lock->l_flags & LDLM_FL_AST_SENT);
                LASSERT(lock->l_bl_ast_run == 0);
                LASSERT(lock->l_blocking_lock);
                lock->l_bl_ast_run++;
                unlock_res_and_lock(lock);

                ldlm_lock2desc(lock->l_blocking_lock, &d);

                LDLM_LOCK_PUT(lock->l_blocking_lock);
                lock->l_blocking_lock = NULL;
                rc = lock->l_blocking_ast(lock, &d, (void *)&arg,
                                          LDLM_CB_BLOCKING);
                LDLM_LOCK_PUT(lock);
                ast_count++;

		 /* Send the request set if it exceeds the PARALLEL_AST_LIMIT,
                 * and create a new set for requests that remained in
                 * @rpc_list */
                if (unlikely(ast_count == PARALLEL_AST_LIMIT)) {
                        ldlm_send_and_maybe_create_set(&arg, 1);
                        ast_count = 0;
                }
        }

        if (ast_count > 0)
                ldlm_send_and_maybe_create_set(&arg, 0);
        else
                /* In case when number of ASTs is multiply of
                 * PARALLEL_AST_LIMIT or @rpc_list was initially empty,
                 * @arg.set must be destroyed here, otherwise we get 
                 * write memory leaking. */
                Ptlrpc::ptlrpc_set_destroy(arg.set);

        RETURN(atomic_read(&arg.restart) ? -ERESTART : 0);
}

/* NOTE: called by
 *  - ldlm_lock_enqueue
 *  - ldlm_reprocess_queue
 *  - ldlm_lock_convert
 *
 * must be called with lr_lock held
 */
void LDLM::ldlm_grant_lock(struct ldlm_lock *lock, struct list_head *work_list)
{
        struct ldlm_resource *res = lock->l_resource;
        ENTRY;

        check_res_locked(res);

        lock->l_granted_mode = lock->l_req_mode;
        if (res->lr_type == LDLM_PLAIN || res->lr_type == LDLM_IBITS)
                ldlm_grant_lock_with_skiplist(lock);
        else if (res->lr_type == LDLM_EXTENT)
                ldlm_extent_add_lock(res, lock);
        else
                ldlm_resource_add_lock(res, &res->lr_granted, lock);

        if (lock->l_granted_mode < res->lr_most_restr)
                res->lr_most_restr = lock->l_granted_mode;

        if (work_list && lock->l_completion_ast != NULL)
                ldlm_add_ast_work_item(lock, NULL, work_list);

        //ldlm_pool_add(&res->lr_namespace->ns_pool, lock);
        EXIT;
}

ldlm_error_t LDLM::ldlm_lock_enqueue(struct ldlm_namespace *ns,
                               struct ldlm_lock **lockp,
                               void *cookie, int *flags)
{
	struct ldlm_lock *lock = *lockp;
        struct ldlm_resource *res = lock->l_resource;
        int local = ns_is_client(res->lr_namespace);
        ldlm_processing_policy policy;
        ldlm_error_t rc = ELDLM_OK;
        struct ldlm_interval *node = NULL;
        ENTRY;

        lock->l_last_activity = cfs_time_current_sec();

	lock_res_and_lock(lock);
        if (local && lock->l_req_mode == lock->l_granted_mode) {
                /* The server returned a blocked lock, but it was granted
                 * before we got a chance to actually enqueue it.  We don't
                 * need to do anything else. */
                *flags &= ~(LDLM_FL_BLOCK_GRANTED |
                            LDLM_FL_BLOCK_CONV | LDLM_FL_BLOCK_WAIT);
                GOTO(out, ELDLM_OK);
        }

        ldlm_resource_unlink_lock(lock);
        if (res->lr_type == LDLM_EXTENT && lock->l_tree_node == NULL) {
                if (node == NULL) {
                        ldlm_lock_destroy/*_nolock*/(lock);
                        GOTO(out, rc = (ldlm_error_t) -ENOMEM);
                }

                CFS_INIT_LIST_HEAD(&node->li_group);
                ldlm_interval_attach(node, lock);
                node = NULL;
        }

        /* Some flags from the enqueue want to make it into the AST, via the
         * lock's l_flags. */
        lock->l_flags |= *flags & LDLM_AST_DISCARD_DATA;

	/* This distinction between local lock trees is very important; a client
         * namespace only has information about locks taken by that client, and
         * thus doesn't have enough information to decide for itself if it can
         * be granted (below).  In this case, we do exactly what the server
         * tells us to do, as dictated by the 'flags'.
         *
         * We do exactly the same thing during recovery, when the server is
         * more or less trusting the clients not to lie.
         *
         * FIXME (bug 268): Detect obvious lies by checking compatibility in
         * granted/converting queues. */
        if (local) {
                if (*flags & LDLM_FL_BLOCK_CONV)
                        ldlm_resource_add_lock(res, &res->lr_converting, lock);
                else if (*flags & (LDLM_FL_BLOCK_WAIT | LDLM_FL_BLOCK_GRANTED))
                        ldlm_resource_add_lock(res, &res->lr_waiting, lock);
                else
                        ldlm_grant_lock(lock, NULL);
                GOTO(out, ELDLM_OK);
        } else if (*flags & LDLM_FL_REPLAY) {
                if (*flags & LDLM_FL_BLOCK_CONV) {
                        ldlm_resource_add_lock(res, &res->lr_converting, lock);
                        GOTO(out, ELDLM_OK);
                } else if (*flags & LDLM_FL_BLOCK_WAIT) {
                        ldlm_resource_add_lock(res, &res->lr_waiting, lock);
                        GOTO(out, ELDLM_OK);
                } else if (*flags & LDLM_FL_BLOCK_GRANTED) {
                        ldlm_grant_lock(lock, NULL);
                        GOTO(out, ELDLM_OK);
                }
                /* If no flags, fall through to normal enqueue path. */
        }

        policy = ldlm_processing_policy_table[res->lr_type];
        policy(lock, flags, 1, &rc, NULL);
	GOTO(out, rc);
out:
        unlock_res_and_lock(lock);
        if (node)
                OBD_SLAB_FREE(node, ldlm_interval_slab, sizeof(*node));

        return rc;

}

/* interval tree, for LDLM_EXTENT. */
void LDLM::ldlm_interval_attach(struct ldlm_interval *n,
                          struct ldlm_lock *l)
{
        LASSERT(l->l_tree_node == NULL);
        LASSERT(l->l_resource->lr_type == LDLM_EXTENT);

        list_add_tail(&l->l_sl_policy, &n->li_group);
        l->l_tree_node = n;
}

struct ldlm_interval *LDLM::ldlm_interval_detach(struct ldlm_lock *l)
{
	struct ldlm_interval *n = l->l_tree_node;

        if (n == NULL)
                return NULL;

        LASSERT(!list_empty(&n->li_group));
        l->l_tree_node = NULL;
        list_del_init(&l->l_sl_policy);

        return (list_empty(&n->li_group) ? n : NULL);
}

void LDLM::ldlm_interval_free(struct ldlm_interval *node)
{
	if (node) {
                LASSERT(list_empty(&node->li_group));
                LASSERT(!interval_is_intree(&node->li_node));
                //OBD_SLAB_FREE(node, ldlm_interval_slab, sizeof(*node));
		OBD_FREE(node, sizeof(*node));
        }
}

void LDLM::ldlm_extent_unlink_lock(struct ldlm_lock *lock)
{
        struct ldlm_resource *res = lock->l_resource;
        struct ldlm_interval *node = lock->l_tree_node;
        struct ldlm_interval_tree *tree;
        int idx;

        if (!node || !interval_is_intree(&node->li_node)) /* duplicate unlink */
                return;

        idx = lock_mode_to_index(lock->l_granted_mode);
        LASSERT(lock->l_granted_mode == 1 << idx);
        tree = &res->lr_itree[idx];

        LASSERT(tree->lit_root != NULL); /* assure the tree is not null */

        tree->lit_size--;
        node = ldlm_interval_detach(lock);
        if (node) {
                interval_erase(&node->li_node, &tree->lit_root);
                ldlm_interval_free(node);
        }
}

void LDLM::ldlm_extent_add_lock(struct ldlm_resource *res,
                          struct ldlm_lock *lock)
{
        struct interval_node *found, **root;
        struct ldlm_interval *node;
        struct ldlm_extent *extent;
        int idx;

        LASSERT(lock->l_granted_mode == lock->l_req_mode);

        node = lock->l_tree_node;
        LASSERT(node != NULL);
        LASSERT(!interval_is_intree(&node->li_node));

        idx = lock_mode_to_index(lock->l_granted_mode);
        LASSERT(lock->l_granted_mode == 1 << idx);
        LASSERT(lock->l_granted_mode == res->lr_itree[idx].lit_mode);

        /* node extent initialize */
        extent = &lock->l_policy_data.l_extent;
        interval_set(&node->li_node, extent->start, extent->end);

        root = &res->lr_itree[idx].lit_root;
        found = interval_insert(&node->li_node, root);
        if (found) { /* The policy group found. */
                struct ldlm_interval *tmp = ldlm_interval_detach(lock);
                LASSERT(tmp != NULL);
                ldlm_interval_free(tmp);
                ldlm_interval_attach(to_ldlm_interval(found), lock);
        }
        res->lr_itree[idx].lit_size++;

        /* even though we use interval tree to manage the extent lock, we also
         * add the locks into grant list, for debug purpose, .. */
        ldlm_resource_add_lock(res, &res->lr_granted, lock);
}

int LDLM::ldlm_check_contention(struct ldlm_lock *lock, int contended_locks)
{
        struct ldlm_resource *res = lock->l_resource;
        cfs_time_t now = cfs_time_current();

        //CDEBUG(D_DLMTRACE, "contended locks = %d\n", contended_locks);
        if (contended_locks > res->lr_namespace->ns_contended_locks)
                res->lr_contention_time = now;
        return cfs_time_before(now, cfs_time_add(res->lr_contention_time,
                cfs_time_seconds(res->lr_namespace->ns_contention_time)));
}

struct ldlm_extent_compat_args {
        struct list_head *work_list;
        struct ldlm_lock *lock;
        ldlm_mode_t mode;
        int *locks;
        int *compat;
};

enum interval_iter LDLM::ldlm_extent_compat_cb(struct interval_node *n,
                                                void *data)
{
        struct ldlm_extent_compat_args *priv = (ldlm_extent_compat_args *)data;
        struct ldlm_interval *node = to_ldlm_interval(n);
        struct ldlm_extent *extent;
        struct list_head *work_list = priv->work_list;
        struct ldlm_lock *lock, *enq = priv->lock;
        ldlm_mode_t mode = priv->mode;
        int count = 0;
        ENTRY;

        LASSERT(!list_empty(&node->li_group));

        list_for_each_entry(lock, &node->li_group, l_sl_policy) {
                /* interval tree is for granted lock */
               /* LASSERTF(mode == lock->l_granted_mode,
                         "mode = %s, lock->l_granted_mode = %s\n",
                         ldlm_lockname[mode],
                         ldlm_lockname[lock->l_granted_mode]);
		*/

                count++;
                if (lock->l_blocking_ast)
                        ldlm_add_ast_work_item(lock, enq, work_list);
        }
        LASSERT(count > 0);

        /* don't count conflicting glimpse locks */
        extent = ldlm_interval_extent(node);
        if (!(mode == LCK_PR &&
            extent->start == 0 && extent->end == OBD_OBJECT_EOF))
                *priv->locks += count;

        if (priv->compat)
                *priv->compat = 0;

        RETURN(INTERVAL_ITER_CONT);
}

void LDLM::ldlm_extent_internal_policy_fixup(struct ldlm_lock *req,
                                              struct ldlm_extent *new_ex,
                                              int conflicting)
{
	 ldlm_mode_t req_mode = req->l_req_mode;
        __u64 req_start = req->l_req_extent.start;
        __u64 req_end = req->l_req_extent.end;
        __u64 req_align, mask;

        if (conflicting > 32 && (req_mode == LCK_PW || req_mode == LCK_CW)) {
                if (req_end < req_start + LDLM_MAX_GROWN_EXTENT)
                        new_ex->end = min(req_start + LDLM_MAX_GROWN_EXTENT,
                                          new_ex->end);
        }

        if (new_ex->start == 0 && new_ex->end == OBD_OBJECT_EOF) {
                EXIT;
                return;
        }

        /* we need to ensure that the lock extent is properly aligned to what
         * the client requested.  We align it to the lowest-common denominator
         * of the clients requested lock start and end alignment. */
        mask = 0x1000ULL;
        req_align = (req_end + 1) | req_start;
        if (req_align != 0) {
                while ((req_align & mask) == 0)
                        mask <<= 1;
        }
        mask -= 1;
        /* We can only shrink the lock, not grow it.
         * This should never cause lock to be smaller than requested,
         * since requested lock was already aligned on these boundaries. */
        new_ex->start = ((new_ex->start - 1) | mask) + 1;
        new_ex->end = ((new_ex->end + 1) & ~mask) - 1;
        LASSERTF(new_ex->start <= req_start,
                 "mask " LPX64 " grant start " LPU64 " req start " LPU64 "\n",
                 mask, new_ex->start, req_start);
        LASSERTF(new_ex->end >= req_end,
                 "mask "LPX64" grant end "LPU64" req end "LPU64"\n",
                 mask, new_ex->end, req_end);
	
}

/* The purpose of this function is to return:
 * - the maximum extent
 * - containing the requested extent
 * - and not overlapping existing conflicting extents outside the requested one
 *
 * Use interval tree to expand the lock extent for granted lock.
 */
void LDLM::ldlm_extent_internal_policy_granted(struct ldlm_lock *req,
                                                struct ldlm_extent *new_ex)
{
        struct ldlm_resource *res = req->l_resource;
        ldlm_mode_t req_mode = req->l_req_mode;
        __u64 req_start = req->l_req_extent.start;
        __u64 req_end = req->l_req_extent.end;
        struct ldlm_interval_tree *tree;
        struct interval_node_extent limiter = { new_ex->start, new_ex->end };
        int conflicting = 0;
        int idx;
        ENTRY;

        lockmode_verify(req_mode);

        /* using interval tree to handle the ldlm extent granted locks */
        for (idx = 0; idx < LCK_MODE_NUM; idx++) {
                struct interval_node_extent ext = { req_start, req_end };

                tree = &res->lr_itree[idx];
                if (lockmode_compat(tree->lit_mode, req_mode))
                        continue;

                conflicting += tree->lit_size;
                if (conflicting > 4)
                        limiter.start = req_start;

                if (interval_is_overlapped(tree->lit_root, &ext))
                        printf("req_mode = %d, tree->lit_mode = %d, tree->lit_size = %d\n",
                               req_mode, tree->lit_mode, tree->lit_size);

		interval_expand(tree->lit_root, &ext, &limiter);
                limiter.start = max(limiter.start, ext.start);
                limiter.end = min(limiter.end, ext.end);
                if (limiter.start == req_start && limiter.end == req_end)
                        break;
        }

        new_ex->start = limiter.start;
        new_ex->end = limiter.end;
        LASSERT(new_ex->start <= req_start);
        LASSERT(new_ex->end >= req_end);

        ldlm_extent_internal_policy_fixup(req, new_ex, conflicting);
        EXIT;
}

/* The purpose of this function is to return:
 * - the maximum extent
 * - containing the requested extent
 * - and not overlapping existing conflicting extents outside the requested one
 */
void LDLM::
ldlm_extent_internal_policy_waiting(struct ldlm_lock *req,
                                    struct ldlm_extent *new_ex)
{
        struct list_head *tmp;
        struct ldlm_resource *res = req->l_resource;
        ldlm_mode_t req_mode = req->l_req_mode;
        __u64 req_start = req->l_req_extent.start;
        __u64 req_end = req->l_req_extent.end;
        int conflicting = 0;
        ENTRY;

        lockmode_verify(req_mode);

        /* for waiting locks */
        list_for_each(tmp, &res->lr_waiting) {
                struct ldlm_lock *lock;
                struct ldlm_extent *l_extent;

                lock = list_entry(tmp, struct ldlm_lock, l_res_link);
                l_extent = &lock->l_policy_data.l_extent;

                /* We already hit the minimum requested size, search no more */
                if (new_ex->start == req_start && new_ex->end == req_end) {
                        EXIT;
                        return;
                }

                /* Don't conflict with ourselves */
                if (req == lock)
                        continue;

		/* Locks are compatible, overlap doesn't matter */
                /* Until bug 20 is fixed, try to avoid granting overlapping
                 * locks on one client (they take a long time to cancel) */
                if (lockmode_compat(lock->l_req_mode, req_mode) &&
                    lock->l_export != req->l_export)
                        continue;

                /* If this is a high-traffic lock, don't grow downwards at all
                 * or grow upwards too much */
                ++conflicting;
                if (conflicting > 4)
                        new_ex->start = req_start;

                /* If lock doesn't overlap new_ex, skip it. */
                if (!ldlm_extent_overlap(l_extent, new_ex))
                        continue;

                /* Locks conflicting in requested extents and we can't satisfy
                 * both locks, so ignore it.  Either we will ping-pong this
                 * extent (we would regardless of what extent we granted) or
                 * lock is unused and it shouldn't limit our extent growth. */
                if (ldlm_extent_overlap(&lock->l_req_extent,&req->l_req_extent))
                        continue;

                /* We grow extents downwards only as far as they don't overlap
                 * with already-granted locks, on the assumtion that clients
                 * will be writing beyond the initial requested end and would
                 * then need to enqueue a new lock beyond previous request.
                 * l_req_extent->end strictly < req_start, checked above. */
                if (l_extent->start < req_start && new_ex->start != req_start) {
                        if (l_extent->end >= req_start)
                                new_ex->start = req_start;
                        else
                                new_ex->start = min(l_extent->end+1, req_start);
                }

		/* If we need to cancel this lock anyways because our request
                 * overlaps the granted lock, we grow up to its requested
                 * extent start instead of limiting this extent, assuming that
                 * clients are writing forwards and the lock had over grown
                 * its extent downwards before we enqueued our request. */
                if (l_extent->end > req_end) {
                        if (l_extent->start <= req_end)
                                new_ex->end = max(lock->l_req_extent.start - 1,
                                                  req_end);
                        else
                                new_ex->end = max(l_extent->start - 1, req_end);
                }
        }

        ldlm_extent_internal_policy_fixup(req, new_ex, conflicting);
        EXIT;
}

/* In order to determine the largest possible extent we can grant, we need
 * to scan all of the queues. */
void LDLM::ldlm_extent_policy(struct ldlm_resource *res,
                               struct ldlm_lock *lock, int *flags)
{
        struct ldlm_extent new_ex = { start : 0, end : OBD_OBJECT_EOF };

        if (lock->l_export == NULL)
                /*
                 * this is local lock taken by server (e.g., as a part of
                 * OST-side locking, or unlink handling). Expansion doesn't
                 * make a lot of sense for local locks, because they are
                 * dropped immediately on operation completion and would only
                 * conflict with other threads.
                 */
                return;

        if (lock->l_policy_data.l_extent.start == 0 &&
            lock->l_policy_data.l_extent.end == OBD_OBJECT_EOF)
                /* fast-path whole file locks */
                return;

        ldlm_extent_internal_policy_granted(lock, &new_ex);
        ldlm_extent_internal_policy_waiting(lock, &new_ex);

        if (new_ex.start != lock->l_policy_data.l_extent.start ||
            new_ex.end != lock->l_policy_data.l_extent.end) {
                *flags |= LDLM_FL_LOCK_CHANGED;
                lock->l_policy_data.l_extent.start = new_ex.start;
                lock->l_policy_data.l_extent.end = new_ex.end;
        }
}

void LDLM::discard_bl_list(struct list_head *bl_list)
{
        struct list_head *tmp, *pos;
        ENTRY;

        list_for_each_safe(pos, tmp, bl_list) {
                struct ldlm_lock *lock =
                        list_entry(pos, struct ldlm_lock, l_bl_ast);

                list_del_init(&lock->l_bl_ast);
                LASSERT(lock->l_flags & LDLM_FL_AST_SENT);
                lock->l_flags &= ~LDLM_FL_AST_SENT;
                LASSERT(lock->l_bl_ast_run == 0);
                LASSERT(lock->l_blocking_lock);
                LDLM_LOCK_PUT(lock->l_blocking_lock);
                lock->l_blocking_lock = NULL;
                LDLM_LOCK_PUT(lock);
        }
        EXIT;
}

int LDLM::ldlm_extent_compat_queue(struct list_head *queue, struct ldlm_lock *req,
                         int *flags, ldlm_error_t *err,
                         struct list_head *work_list, int *contended_locks)
{
	struct list_head *tmp;
        struct ldlm_lock *lock;
        struct ldlm_resource *res = req->l_resource;
        ldlm_mode_t req_mode = req->l_req_mode;
        __u64 req_start = req->l_req_extent.start;
        __u64 req_end = req->l_req_extent.end;
        int compat = 1;
        int scan = 0;
        int check_contention;
        ENTRY;

        lockmode_verify(req_mode);

        /* Using interval tree for granted lock */
        if (queue == &res->lr_granted) {
                struct ldlm_interval_tree *tree;
                struct ldlm_extent_compat_args data = {work_list : work_list,
                                               lock : req,
                                               mode : (ldlm_mode_t)0,
                                               locks : contended_locks,
                                               compat : &compat };
                struct interval_node_extent ex = { start : req_start,
                                                   end : req_end };
                int idx, rc;

                for (idx = 0; idx < LCK_MODE_NUM; idx++) {
                        tree = &res->lr_itree[idx];
                        if (tree->lit_root == NULL) /* empty tree, skipped */
                                continue;

                        data.mode = tree->lit_mode;
                        if (lockmode_compat(req_mode, tree->lit_mode)) {
                                struct ldlm_interval *node;
                                struct ldlm_extent *extent;

                                if (req_mode != LCK_GROUP)
                                        continue;

				 /* group lock, grant it immediately if
                                 * compatible */
                                node = to_ldlm_interval(tree->lit_root);
                                extent = ldlm_interval_extent(node);
                                if (req->l_policy_data.l_extent.gid ==
                                    extent->gid)
                                        RETURN(2);
                        }

                        if (tree->lit_mode == LCK_GROUP) {
                                if (*flags & LDLM_FL_BLOCK_NOWAIT) {
                                        compat = -EWOULDBLOCK;
                                        goto destroylock;
                                }

                                *flags |= LDLM_FL_NO_TIMEOUT;
                                if (!work_list)
                                        RETURN(0);

                                /* if work list is not NULL,add all
                                   locks in the tree to work list */
                                compat = 0;
                                interval_iterate(tree->lit_root,
                                                 ldlm_extent_compat_cb, &data);
                                continue;
                        }

                        if (!work_list) {
                                rc = interval_is_overlapped(tree->lit_root,&ex);
                                if (rc)
                                        RETURN(0);
                        } else {
                                interval_search(tree->lit_root, &ex,
                                                ldlm_extent_compat_cb, &data);
                                if (!list_empty(work_list) && compat)
                                        compat = 0;
                        }
		}
        } else {
                /* for waiting queue */
                list_for_each(tmp, queue) {
                        check_contention = 1;

                        lock = list_entry(tmp, struct ldlm_lock, l_res_link);

                        if (req == lock)
                                break;

                        if (unlikely(scan)) {
                                /* We only get here if we are queuing GROUP lock
                                   and met some incompatible one. The main idea of this
                                   code is to insert GROUP lock past compatible GROUP
                                   lock in the waiting queue or if there is not any,
                                   then in front of first non-GROUP lock */
                                if (lock->l_req_mode != LCK_GROUP) {
                                        /* Ok, we hit non-GROUP lock, there should be no
                                           more GROUP locks later on, queue in front of
                                           first non-GROUP lock */

                                        ldlm_resource_insert_lock_after(lock, req);
                                        list_del_init(&lock->l_res_link);
                                        ldlm_resource_insert_lock_after(req, lock);
                                        compat = 0;
                                        break;
                                }
                                if (req->l_policy_data.l_extent.gid ==
                                    lock->l_policy_data.l_extent.gid) {
                                        /* found it */
                                        ldlm_resource_insert_lock_after(lock, req);
                                        compat = 0;
                                        break;
                                }
                                continue;
                        }

			/* locks are compatible, overlap doesn't matter */
                        if (lockmode_compat(lock->l_req_mode, req_mode)) {
                                if (req_mode == LCK_PR &&
                                    ((lock->l_policy_data.l_extent.start <=
                                      req->l_policy_data.l_extent.start) &&
                                     (lock->l_policy_data.l_extent.end >=
                                      req->l_policy_data.l_extent.end))) {
                                        /* If we met a PR lock just like us or wider,
                                           and nobody down the list conflicted with
                                           it, that means we can skip processing of
                                           the rest of the list and safely place
                                           ourselves at the end of the list, or grant
                                           (dependent if we met an conflicting locks
                                           before in the list).
                                           In case of 1st enqueue only we continue
                                           traversing if there is something conflicting
                                           down the list because we need to make sure
                                           that something is marked as AST_SENT as well,
                                           in cse of empy worklist we would exit on
                                           first conflict met. */
                                        /* There IS a case where such flag is
                                           not set for a lock, yet it blocks
                                           something. Luckily for us this is
                                           only during destroy, so lock is
                                           exclusive. So here we are safe */
                                        if (!(lock->l_flags & LDLM_FL_AST_SENT)) {
                                                RETURN(compat);
                                        }
                                }

                                /* non-group locks are compatible, overlap doesn't
                                   matter */
                                if (likely(req_mode != LCK_GROUP))
                                        continue;
				/* If we are trying to get a GROUP lock and there is
                                   another one of this kind, we need to compare gid */
                                if (req->l_policy_data.l_extent.gid ==
                                    lock->l_policy_data.l_extent.gid) {
                                        /* If existing lock with matched gid is granted,
                                           we grant new one too. */
                                        if (lock->l_req_mode == lock->l_granted_mode)
                                                RETURN(2);

                                        /* Otherwise we are scanning queue of waiting
                                         * locks and it means current request would
                                         * block along with existing lock (that is
                                         * already blocked.
                                         * If we are in nonblocking mode - return
                                         * immediately */
                                        if (*flags & LDLM_FL_BLOCK_NOWAIT) {
                                                compat = -EWOULDBLOCK;
                                                goto destroylock;
                                        }
                                        /* If this group lock is compatible with another
                                         * group lock on the waiting list, they must be
                                         * together in the list, so they can be granted
                                         * at the same time.  Otherwise the later lock
                                         * can get stuck behind another, incompatible,
                                         * lock. */
                                        ldlm_resource_insert_lock_after(lock, req);
                                        /* Because 'lock' is not granted, we can stop
                                         * processing this queue and return immediately.
                                         * There is no need to check the rest of the
                                         * list. */
                                        RETURN(0);
                                }
                        }

			 if (unlikely(req_mode == LCK_GROUP &&
                                     (lock->l_req_mode != lock->l_granted_mode))) {
                                scan = 1;
                                compat = 0;
                                if (lock->l_req_mode != LCK_GROUP) {
                                        /* Ok, we hit non-GROUP lock, there should
                                         * be no more GROUP locks later on, queue in
                                         * front of first non-GROUP lock */

                                        ldlm_resource_insert_lock_after(lock, req);
                                        list_del_init(&lock->l_res_link);
                                        ldlm_resource_insert_lock_after(req, lock);
                                        break;
                                }
                                if (req->l_policy_data.l_extent.gid ==
                                    lock->l_policy_data.l_extent.gid) {
                                        /* found it */
                                        ldlm_resource_insert_lock_after(lock, req);
                                        break;
                                }
                                continue;
                        }

                        if (unlikely(lock->l_req_mode == LCK_GROUP)) {
                                /* If compared lock is GROUP, then requested is PR/PW/
                                 * so this is not compatible; extent range does not
                                 * matter */
                                if (*flags & LDLM_FL_BLOCK_NOWAIT) {
                                        compat = -EWOULDBLOCK;
                                        goto destroylock;
                                } else {
                                        *flags |= LDLM_FL_NO_TIMEOUT;
                                }
                        } else if (lock->l_policy_data.l_extent.end < req_start ||
                                   lock->l_policy_data.l_extent.start > req_end) {
                                /* if a non group lock doesn't overlap skip it */
                                continue;
			 } else if (lock->l_req_extent.end < req_start ||
                                   lock->l_req_extent.start > req_end)
                                /* false contention, the requests doesn't really overlap */
                                check_contention = 0;

                        if (!work_list)
                                RETURN(0);

                        /* don't count conflicting glimpse locks */
                        if (lock->l_req_mode == LCK_PR &&
                            lock->l_policy_data.l_extent.start == 0 &&
                            lock->l_policy_data.l_extent.end == OBD_OBJECT_EOF)
                                check_contention = 0;

                        *contended_locks += check_contention;

                        compat = 0;
                        if (lock->l_blocking_ast)
                                ldlm_add_ast_work_item(lock, req, work_list);
                }
        }

        if (ldlm_check_contention(req, *contended_locks) &&
            compat == 0 &&
            (*flags & LDLM_FL_DENY_ON_CONTENTION) &&
            req->l_req_mode != LCK_GROUP &&
            req_end - req_start <=
            req->l_resource->lr_namespace->ns_max_nolock_size)
                GOTO(destroylock, compat = -EUSERS);

        RETURN(compat);
destroylock:
        list_del_init(&req->l_res_link);
        ldlm_lock_destroy_nolock(req);
        *err = (ldlm_error_t)compat;
        RETURN(compat);
}

int LDLM::ldlm_process_plain_lock(struct ldlm_lock *lock, int *flags, int first_enq,
                            ldlm_error_t *err, struct list_head *work_list)
{
	RETURN(0);
}

int LDLM::ldlm_process_flock_lock(struct ldlm_lock *req, int *flags, int first_enq,
                        ldlm_error_t *err, struct list_head *work_list)
{
	RETURN(0);
}

int LDLM::ldlm_process_inodebits_lock(struct ldlm_lock *lock, int *flags,
                                int first_enq, ldlm_error_t *err,
                                struct list_head *work_list)
{
	RETURN(0);
}

int ldlm_process_flock_lock(struct ldlm_lock *lock, int *flags, int first_enq,
                             ldlm_error_t *err, struct list_head *work_list)
{
	RETURN(0);
}

int LDLM::ldlm_process_extent_lock(struct ldlm_lock *lock, int *flags, int first_enq,
                             ldlm_error_t *err, struct list_head *work_list)
{
	 struct ldlm_resource *res = lock->l_resource;
        struct list_head rpc_list = CFS_LIST_HEAD_INIT(rpc_list);
        int rc, rc2;
        int contended_locks = 0;
        ENTRY;

        LASSERT(list_empty(&res->lr_converting));
        LASSERT(!(*flags & LDLM_FL_DENY_ON_CONTENTION) ||
                !(lock->l_flags & LDLM_AST_DISCARD_DATA));
        check_res_locked(res);
        *err = ELDLM_OK;

        if (!first_enq) {
                /* Careful observers will note that we don't handle -EWOULDBLOCK
                 * here, but it's ok for a non-obvious reason -- compat_queue
                 * can only return -EWOULDBLOCK if (flags & BLOCK_NOWAIT).
                 * flags should always be zero here, and if that ever stops
                 * being true, we want to find out. */
                LASSERT(*flags == 0);
                rc = ldlm_extent_compat_queue(&res->lr_granted, lock, flags,
                                              err, NULL, &contended_locks);
                if (rc == 1) {
                        rc = ldlm_extent_compat_queue(&res->lr_waiting, lock,
                                                      flags, err, NULL,
                                                      &contended_locks);
                }
                if (rc == 0)
                        RETURN(LDLM_ITER_STOP);

                ldlm_resource_unlink_lock(lock);

                if (!OBD_FAIL_CHECK(OBD_FAIL_LDLM_CANCEL_EVICT_RACE))
                        ldlm_extent_policy(res, lock, flags);
                ldlm_grant_lock(lock, work_list);
                RETURN(LDLM_ITER_CONTINUE);
        }

restart:
        contended_locks = 0;
        rc = ldlm_extent_compat_queue(&res->lr_granted, lock, flags, err,
                                      &rpc_list, &contended_locks);
        if (rc < 0)
                GOTO(out, rc); /* lock was destroyed */
        if (rc == 2)
                goto grant;

        rc2 = ldlm_extent_compat_queue(&res->lr_waiting, lock, flags, err,
                                       &rpc_list, &contended_locks);
        if (rc2 < 0)
                GOTO(out, rc = rc2); /* lock was destroyed */

        if (rc + rc2 == 2) {
        grant:
                ldlm_extent_policy(res, lock, flags);
                ldlm_resource_unlink_lock(lock);
                ldlm_grant_lock(lock, NULL);
        } else {
		 /* If either of the compat_queue()s returned failure, then we
                 * have ASTs to send and must go onto the waiting list.
                 *
                 * bug 2322: we used to unlink and re-add here, which was a
                 * terrible folly -- if we goto restart, we could get
                 * re-ordered!  Causes deadlock, because ASTs aren't sent! */
                if (list_empty(&lock->l_res_link))
                        ldlm_resource_add_lock(res, &res->lr_waiting, lock);
                unlock_res(res);

                rc = ldlm_run_bl_ast_work(&rpc_list);

/* NO USE
                if (OBD_FAIL_CHECK(OBD_FAIL_LDLM_OST_FAIL_RACE) &&
                    !ns_is_client(res->lr_namespace))
                        class_fail_export(lock->l_export);
*/
                lock_res(res);
                if (rc == -ERESTART) {
                        /* 15715: The lock was granted and destroyed after
                         * resource lock was dropped. Interval node was freed
                         * in ldlm_lock_destroy. Anyway, this always happens
                         * when a client is being evicted. So it would be
                         * ok to return an error. -jay */
                        if (lock->l_destroyed) {
                                *err = (ldlm_error_t)-EAGAIN;
                                GOTO(out, rc = -EAGAIN);
                        }
			
			 /* lock was granted while resource was unlocked. */
                        if (lock->l_granted_mode == lock->l_req_mode) {
                                /* bug 11300: if the lock has been granted,
                                 * break earlier because otherwise, we will go
                                 * to restart and ldlm_resource_unlink will be
                                 * called and it causes the interval node to be
                                 * freed. Then we will fail at 
                                 * ldlm_extent_add_lock() */
                                *flags &= ~(LDLM_FL_BLOCK_GRANTED | LDLM_FL_BLOCK_CONV |
                                            LDLM_FL_BLOCK_WAIT);
                                GOTO(out, rc = 0);
                        }

                        GOTO(restart, -ERESTART);
                }

                *flags |= LDLM_FL_BLOCK_GRANTED;
                /* this way we force client to wait for the lock
                 * endlessly once the lock is enqueued -bzzz */
                *flags |= LDLM_FL_NO_TIMEOUT;

        }
        RETURN(0);
out:
        if (!list_empty(&rpc_list)) {
                LASSERT(!(lock->l_flags & LDLM_AST_DISCARD_DATA));
                discard_bl_list(&rpc_list);
        }
        RETURN(rc);
}

void LDLM::ldlm_test_1(void *data)
{
	int *a, *b;
	struct data_struct {
		int a1;
		int a2;
	} *c;

	enum {
		FUN_FIST_STATE,
		ldlm_test_1_SM_0,
	};

	a = (int *)Thread::GETP(0, sizeof(a));
	b = (int *)Thread::GETP(1, sizeof(b));
	c = (struct data_struct *)Thread::GETP(2, sizeof(c));

	switch(Thread::CurrentState()) {
	case FUN_FIRST_STATE:
		break;
	case ldlm_test_1_SM_0:
		goto L0;
	case FUN_LAST_STATE:
	L00:
		Thread::RestoreContext();
		return;
	}

	printf(NOW"%s:%d: a = %d, b = %d a1 = %d a2 = %d\n", Event::Clock(),
		__FUNCTION__, __LINE__, *a, *b, c->a1, c->a2);


		/* IN */{
			Thread::MoveState(ldlm_test_1_SM_0);
		}
	Thread::Current()->Sleep(6);
		/* OUT */ {
			return;
		L0:
			printf("Goto Lable L0\n");
		}

	*a += 5;
	*b += 6;

	printf(NOW"%s:%d: a = %d, b = %d\n", Event::Clock(), 
		__FUNCTION__, __LINE__, *a, *b);

		/* END */{
			goto L00;
		}
}

void LDLM::ldlm_test_SM(void *data)
{
	ThreadLocalData *tld = Thread::CurrentTLD();
	Context *ctx = tld->curctx;
	int *a, *b;
	struct data_struct {
		int a1;
		int a2;
		} *c;

		enum {
			ldlm_test_SM_1 = 1,
			ldlm_test_SM_2,
		};

		a = (int *)Thread::SETV(0, sizeof(*a));
		b = (int *)Thread::SETV(1, sizeof(*b));
		c = (data_struct *)Thread::SETV(2, sizeof(*c));
		/* START */	
		switch(Thread::CurrentState()) {
		case FUN_FIRST_STATE:
			ctx->fn = ldlm_test_SM;
			break;
		case ldlm_test_SM_1:
			goto L1;
		case ldlm_test_SM_2:
			goto L2;
		case FUN_LAST_STATE:
		L00:
			Thread::RestoreContext();
			return;
		}

	*a = 5;
	*b = 6;
	c->a1 = 7;
	c->a2 = 8;

	printf(NOW"%s:%d: a = %d, b = %d\n", Event::Clock(), 
		__FUNCTION__, __LINE__, *a, *b);
	
		/* IN */ {
			Thread::MoveState(ldlm_test_SM_1);
		}
	Thread::Current()->Sleep(5);
		/* OUT */ {
			return;
		} L1:
	*a += 5;
	*b += 6;
	printf(NOW" %s:%d: a = %d, b = %d\n", Event::Clock(), 
		__FUNCTION__, __LINE__, *a, *b);

	*a += 5;
	*b += 6;

		/* IN */ {
			Thread::IN(ldlm_test_SM_2, ldlm_test_1);
			Thread::INP(0,  sizeof(a), (void *)(a));
			Thread::INP(1, sizeof(b), (void *)(b));
			Thread::INP(2, sizeof(c), (void *)(c));
		}
	ldlm_test_1(data);
		/* OUT */ {
			return;
		L2:
			Thread::OUT();
		}
	printf(NOW" %s:%d: a = %d, b = %d \n", Event::Clock(), 
		__FUNCTION__, __LINE__, *a, *b);
		/*END */ {
			goto L00;
		}
}

void LDLM::ldlm_test()
{
	Thread *t = new Thread;
	ThreadLocalData *tld = new ThreadLocalData;

	Thread::InitTLD(tld);
	t->CreateThread(ldlm_test_SM, tld);
	t->RunAfter(1);

	Event::Schedule();

	delete tld;
	delete t;
}
