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
#ifndef RBTREE_H
#define RBTREE_H

#include <lustre.h>

/**
	@author yingjin.qian <yingjin.qian@sun.com>
*/

struct rb_node
{
	unsigned long rb_parent_color;
	rb_node *rb_right;
	rb_node *rb_left;
	void *rb_entry;
};

struct rb_root {
	struct rb_node *rb_node;
};

#define rb_entry(ptr, type, member) container_of(ptr, type, member)

class Rbtree
{
	#define RB_RED		0
	#define RB_BLACK	1

	#define rb_parent(r)   ((struct rb_node *)((r)->rb_parent_color & ~3))
	#define rb_color(r)   ((r)->rb_parent_color & 1)
	#define rb_is_red(r)   (!rb_color(r))
	#define rb_is_black(r) rb_color(r)
	#define rb_set_red(r)  do { (r)->rb_parent_color &= ~1; } while (0)
	#define rb_set_black(r)  do { (r)->rb_parent_color |= 1; } while (0)

	#define RB_ROOT	(rb_root) { NULL, }

	#define RB_EMPTY_ROOT(root)	((root)->rb_node == NULL)
	#define RB_EMPTY_NODE(node)	(rb_parent(node) == node)
	#define RB_CLEAR_NODE(node)	(rb_set_parent(node, node))
	

	
	int num;
	rb_node *leftmost;
	compare_f compare;

	void __rb_rotate_left(struct rb_node *node);
	void __rb_rotate_right(struct rb_node *node);
	void __rb_erase_color(rb_node *node, rb_node *parent);
public:
	rb_root root;
public:
  Rbtree();

  ~Rbtree();
	
	void rb_set_compare(compare_f c);
	void rb_insert(rb_node *);
	void rb_insert_color(rb_node *);
	void rb_erase(rb_node *);
	rb_root *rb_get_root();
	int rb_find_nearest(rb_node *, rb_node *, rb_node **);
/* Find logical next and previous nodes in a tree */
	static rb_node *rb_next(rb_node *);
	static rb_node *rb_prev(rb_node *);

	rb_node *rb_first();
	rb_node *rb_last();

/* Fast replacement of a single node without remove/rebalance/add/rebalance */
	void rb_replace_node(rb_node *victim, rb_node *n);
	void rb_link_node(rb_node * node, rb_node * parent, rb_node ** rb_link);

	void rb_set_parent(rb_node *rb, rb_node *p);
	void rb_clear_node(rb_node *rb);
	void rb_set_color(rb_node *rb, int color);
};

#endif
