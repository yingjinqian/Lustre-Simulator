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
#ifndef NRSRBTREE_H
#define NRSRBTREE_H

#include <scheduler.h>
#include <rbtree.h>

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/
typedef rb_node *(*key_f)(void *);
class NrsRbtree : public Scheduler
{
	Rbtree		tree;
	key_f keyf;
	
	/*used  by selftest */	
	struct node_t{
		rb_node rbnode;
		int idx;
		int key;
	};
	static int Compare(void *a, void *b);
	static rb_node *GetRbnode(void *e);
public:
	NrsRbtree();
	NrsRbtree(compare_f c, key_f k);
	NrsRbtree(compare_f c);
	~NrsRbtree();
	
	void SetCompareFunc(compare_f c);
	void Init(compare_f c, key_f k);
	int Enqueue(void *e);
	int Requeue(void *e);
	void Erase(void *e);
	void *Dequeue();
	void *First();
	void *Last();

	static void SelfTest();
};

#endif
