/***************************************************************************
 *   Copyright (C) 2008 by qian                                            *
 *   yingjin.qian@sun.com                                                  *
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
#ifndef NRSBINHEAP_H
#define NRSBINHEAP_H

#include <scheduler.h>

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/
class NrsBinheap : public Scheduler
{
	compare_f compare;
	#define CAA_SHIFT  10
	#define CAA_SIZE   (1 << CAA_SHIFT)             /* # ptrs per level */
	#define CAA_MASK   (CAA_SIZE - 1)
	#define CAA_NOB    (CAA_SIZE * sizeof(void *))
	
	typedef struct
	{
		void         ****aa_3d;                 /* Triple indirect */
		void          ***aa_2d;                 /* double indirect */
		void           **aa_1d;                 /* single indirect */
	} cfs_autoarray_t;
	
	cfs_autoarray_t  cbh_members;
	unsigned int     cbh_size;
	unsigned int     cbh_maxsize;

	void cfs_aa_init(cfs_autoarray_t *aa);
	void cfs_aa_fini(cfs_autoarray_t *aa);
	void **cfs_aa_index(cfs_autoarray_t *aa, unsigned int idx);
	int cfs_aa_lookup1(void **array, int grow);
	void **cfs_aa_lookup(cfs_autoarray_t *aa, unsigned int idx, int grow);
	int cfs_bheap_size();
	int cfs_bheap_insert(void *e);
	void *cfs_bheap_remove_root();
public:
  NrsBinheap();
	NrsBinheap(compare_f c);
  ~NrsBinheap();
	
	void SetCompareFunc(compare_f c);
	int Enqueue(void *e);
	int Requeue(void *e);
	void Erase(void *e);
	void *Dequeue();
};

#endif
