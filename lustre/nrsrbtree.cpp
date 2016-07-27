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
#include "nrsrbtree.h"

NrsRbtree::NrsRbtree()
    : Scheduler()
{}

NrsRbtree::NrsRbtree(compare_f c)
		: Scheduler()
{
	SetCompareFunc(c);
	keyf = GetRbnode;
}

NrsRbtree::NrsRbtree(compare_f c, key_f k)
    : Scheduler()
{
	SetCompareFunc(c);
	keyf = k;
}

NrsRbtree::~NrsRbtree()
{}

void NrsRbtree::SetCompareFunc(compare_f c)
{
	tree.rb_set_compare(c);
}

void NrsRbtree::Init(compare_f c, key_f k)
{
	SetCompareFunc(c);
	keyf = k;
}

int NrsRbtree::Enqueue(void *e)
{
	rb_node *node = keyf(e);
	//rb_node *node = (rb_node *)e;

	tree.rb_insert(node);
	size++;
	return 0;
}

/* Not Implement */
int NrsRbtree::Requeue(void *e)
{
	size++;
	return 0;
}

void NrsRbtree::Erase(void *e)
{
	size--;
	tree.rb_erase(keyf(e));
}

void *NrsRbtree::Dequeue()
{
	rb_node *node;

	node = tree.rb_first();
	if (node == NULL)
		return NULL;

	tree.rb_erase(node);
	size--;
	return node->rb_entry;
}

void *NrsRbtree::First()
{
	rb_node *node;

	node = tree.rb_first();
	if (node == NULL)
		return NULL;

	return node->rb_entry;
}

void *NrsRbtree::Last()
{
	rb_node *node;

	node = tree.rb_last();
	if (node == NULL);
		return NULL;

	return node->rb_entry;
}

int NrsRbtree::Compare(void *a, void *b)
{
	return ((node_t*)a)->key - ((node_t *)b)->key;
}

rb_node *NrsRbtree::GetRbnode(void *e)
{
	return (rb_node *)e;
}

void NrsRbtree::SelfTest()
{
	int i;
	node_t val[5];
	NrsRbtree nrs(Compare, GetRbnode);

	val[0].key = 3;
	val[1].key = 20;
	val[2].key = 10;
	val[3].key = 7;
	val[4].key = 8;
	
	for (i = 0; i < 5; i++) {
		val[i].idx = i;
		val[i].rbnode.rb_entry = &val[i];
	}

	for (i = 0; i < 5; i++)
		nrs.Enqueue(&val[i]);

	for (i = 0; i < 5; i++) {
		node_t *n;

		n = (node_t *)nrs.Dequeue();
		printf("%d %d\n", n->idx, n->key);
	}
}
