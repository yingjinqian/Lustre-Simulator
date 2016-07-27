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
#include "nrsbinheap.h"

NrsBinheap::NrsBinheap()
    : Scheduler()
{
	cfs_aa_init(&cbh_members);
	cbh_size = 0;
	cbh_maxsize = 0;
}

NrsBinheap::NrsBinheap(compare_f c)
    : Scheduler()
{
	cfs_aa_init(&cbh_members);
	cbh_size = 0;
	cbh_maxsize = 0;
	SetCompareFunc(c);
}

NrsBinheap::~NrsBinheap()
{
	cbh_size = 0;
	cbh_maxsize = 0;
	cfs_aa_fini(&cbh_members);
}

void NrsBinheap::SetCompareFunc(compare_f  c)
{
	compare = c;
}

/* Not Implement yet. */
void NrsBinheap::Erase(void *e)
{}

void *NrsBinheap::Dequeue()
{
	if (size == 0)
		return NULL;

	size--;
	return cfs_bheap_remove_root();
}

int NrsBinheap::Enqueue(void *e)
{
	size++;
 	return (cfs_bheap_insert(e));
}

int NrsBinheap::Requeue(void *e)
{
	size++;
	return (cfs_bheap_insert(e));
}

void NrsBinheap::cfs_aa_init(cfs_autoarray_t *aa)
{
        aa->aa_1d = NULL;
        aa->aa_2d = NULL;
        aa->aa_3d = NULL;
}

void NrsBinheap::cfs_aa_fini(cfs_autoarray_t *aa)
{
        int idx0;
        int idx1;

        if (aa->aa_1d != NULL) {
                LIBCFS_FREE(aa->aa_1d, CAA_NOB);
                aa->aa_1d = NULL;
        }

        if (aa->aa_2d != NULL) {
                for (idx0 = 0; idx0 < CAA_SIZE; idx0++) {
                        if (aa->aa_2d[idx0] == NULL)
                                continue;
                        LIBCFS_FREE(aa->aa_2d[idx0], CAA_NOB);
                }

                LIBCFS_FREE(aa->aa_2d, CAA_NOB);
                aa->aa_2d = NULL;
        }

        if (aa->aa_3d != NULL) {
                for (idx0 = 0; idx0 < CAA_SIZE; idx0++) {

                        if (aa->aa_3d[idx0] == NULL)
                                continue;

                        for (idx1 = 0; idx1 < CAA_SIZE; idx1++) {
                                if (aa->aa_3d[idx0][idx1] == NULL)
                                        continue;

                                LIBCFS_FREE(aa->aa_3d[idx0][idx1], CAA_NOB);
                        }

                        LIBCFS_FREE(aa->aa_3d[idx0], CAA_NOB);
                }

                LIBCFS_FREE(aa->aa_3d, CAA_NOB);
                aa->aa_3d = NULL;
        }
}

void **NrsBinheap::cfs_aa_index(cfs_autoarray_t *a, unsigned int idx)
{
        unsigned int idx0;
        unsigned int idx1;

        if (idx < CAA_SIZE)
                return &(a->aa_1d[idx]);

        idx -= CAA_SIZE;
        idx0 = idx >> CAA_SHIFT;
        if (idx0 < CAA_SIZE)
                return &(a->aa_2d[idx0][idx & CAA_MASK]);

        idx -= CAA_SIZE * CAA_SIZE;
        idx0 = idx >> CAA_SHIFT;
        idx1 = idx0 >> CAA_SHIFT;

        return &(a->aa_3d[idx1][idx0 & CAA_MASK][idx & CAA_MASK]);
}

int NrsBinheap::cfs_aa_lookup1(void **array, int grow)
{
        void *a;

        if (*array != NULL)
                return 1;

        if (!grow)
                return 0;

        LIBCFS_ALLOC(a, CAA_NOB);
        if (a == NULL)
                return 0;

        *array = a;
        return 1;
}

void **NrsBinheap::cfs_aa_lookup(cfs_autoarray_t *a, unsigned int idx, int grow)
{
        unsigned int idx0;
        unsigned int idx1;

        if (idx < CAA_SIZE) {
                if (!cfs_aa_lookup1((void **)&a->aa_1d, grow))
                        return NULL;
                return &(a->aa_1d[idx]);
        }

        idx -= CAA_SIZE;
        idx0 = idx >> CAA_SHIFT;
        if (idx0 < CAA_SIZE) {
                if (!cfs_aa_lookup1((void **)&a->aa_2d, grow) ||
                    !cfs_aa_lookup1((void **)&(a->aa_2d[idx0]), grow))
                        return NULL;

                return &(a->aa_2d[idx0][idx & CAA_MASK]);
        }

        idx -= CAA_SIZE * CAA_SIZE;
        idx0 = idx >> CAA_SHIFT;
        idx1 = idx0 >> CAA_SHIFT;
        idx0 &= CAA_MASK;

        if (!cfs_aa_lookup1((void **)&a->aa_3d, grow) ||
            !cfs_aa_lookup1((void **)&(a->aa_3d[idx1]), grow) ||
            !cfs_aa_lookup1((void **)&(a->aa_3d[idx1][idx0]), grow))
                return NULL;

        return &(a->aa_3d[idx1][idx0 & CAA_MASK][idx & CAA_MASK]);
}

int NrsBinheap::cfs_bheap_size() 
{
        return cbh_size;
}

int NrsBinheap::cfs_bheap_insert(void *e)
{
        unsigned int     child_idx = cbh_size;
        void           **child_ptr;
        unsigned int     parent_idx;
        void           **parent_ptr;
        int              rc;

        LASSERT (child_idx <= cbh_maxsize);

        /* add new child at the very bottom of the tree == last slot in the members array */
        cbh_size = child_idx + 1;

        if (child_idx < cbh_maxsize) {
                /* I know there is a slot in the members array */
                child_ptr = cfs_aa_index(&cbh_members, child_idx);
        } else {
                /* Ensure auto-array grows to accomodate new member */
                child_ptr = cfs_aa_lookup(&cbh_members, child_idx, 1);
                if (child_ptr == NULL)
                        return -ENOMEM;
                cbh_maxsize = cbh_size;
        }

        while (child_idx > 0) {
                parent_idx = (child_idx - 1) >> 1;

                parent_ptr = cfs_aa_index(&cbh_members, parent_idx);

                if (compare(*parent_ptr, e))
                        break;

                *child_ptr = *parent_ptr;
                child_ptr = parent_ptr;
                child_idx = parent_idx;
        }

        *child_ptr = e;
        return 0;
}

void *NrsBinheap::cfs_bheap_remove_root()
{
        unsigned int  n = cbh_size;
        void         *r;
        void         *e;
        unsigned int  child_idx;
        void        **child_ptr;
        void         *child;
        unsigned int  child2_idx;
        void        **child2_ptr;
        void         *child2;
        unsigned int  parent_idx;
        void        **parent_ptr;

        if (n == 0)
                return NULL;

        parent_idx = 0;
        parent_ptr = cfs_aa_index(&cbh_members, parent_idx);
        r = *parent_ptr;

        n--;
        e = *cfs_aa_index(&cbh_members, n);
        cbh_size = n;

        while (parent_idx < n) {
                child_idx = (parent_idx << 1) + 1;
                if (child_idx >= n)
                        break;

                child_ptr = cfs_aa_index(&cbh_members, child_idx);
                child = *child_ptr;

                child2_idx = child_idx + 1;
                if (child2_idx < n) {
                        child2_ptr = cfs_aa_index(&cbh_members, child2_idx);
                        child2 = *child2_ptr;

                        if (compare(child2, child)) {
                                child_idx = child2_idx;
                                child_ptr = child2_ptr;
                                child = child2;
                        }
                }

                if (compare(e, child))
                        break;

                *parent_ptr = child;
                parent_ptr = child_ptr;
                parent_idx = child_idx;
        }

        *parent_ptr = e;
        return r;
}
