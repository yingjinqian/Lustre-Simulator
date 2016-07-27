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
#include "rbtree.h"

Rbtree::Rbtree()
{
	root = RB_ROOT;
	leftmost = NULL;
	num = 0;
	compare = NULL;
}


Rbtree::~Rbtree()
{}

rb_root *Rbtree::rb_get_root()
{
	return &root;
}

void Rbtree::rb_set_color(rb_node *rb, int color)
{
	rb->rb_parent_color = (rb->rb_parent_color & ~1) | color;
}

void Rbtree::rb_set_parent(rb_node *rb, rb_node *p)
{
	rb->rb_parent_color = (rb->rb_parent_color & 3) | (unsigned long)p;
}

void Rbtree::rb_clear_node(rb_node *rb)
{
	rb_set_parent(rb, rb);
}

void Rbtree:: __rb_rotate_left(rb_node *node)
{
	rb_node *right = node->rb_right;
	rb_node *parent = rb_parent(node);

	if ((node->rb_right = right->rb_left))
		rb_set_parent(right->rb_left, node);
	right->rb_left = node;

	rb_set_parent(right, parent);

	if (parent)
	{
		if (node == parent->rb_left)
			parent->rb_left = right;
		else
			parent->rb_right = right;
	}
	else
		root.rb_node = right;
	rb_set_parent(node, right);
}

void Rbtree::__rb_rotate_right(rb_node *node)
{
	rb_node *left = node->rb_left;
	rb_node *parent = rb_parent(node);

	if ((node->rb_left = left->rb_right))
		rb_set_parent(left->rb_right, node);
	left->rb_right = node;

	rb_set_parent(left, parent);

	if (parent)
	{
		if (node == parent->rb_right)
			parent->rb_right = left;
		else
			parent->rb_left = left;
	}
	else
		root.rb_node = left;
	rb_set_parent(node, left);
}

void Rbtree::rb_insert_color(rb_node *node)
{
	rb_node *parent, *gparent;

	while ((parent = rb_parent(node)) && rb_is_red(parent))
	{
		gparent = rb_parent(parent);

		if (parent == gparent->rb_left)
		{
			{
				register struct rb_node *uncle = gparent->rb_right;
				if (uncle && rb_is_red(uncle))
				{
					rb_set_black(uncle);
					rb_set_black(parent);
					rb_set_red(gparent);
					node = gparent;
					continue;
				}
			}

			if (parent->rb_right == node)
			{
				register struct rb_node *tmp;
				__rb_rotate_left(parent);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			rb_set_black(parent);
			rb_set_red(gparent);
			__rb_rotate_right(gparent);
		} else {
			{
				register struct rb_node *uncle = gparent->rb_left;
				if (uncle && rb_is_red(uncle))
				{
					rb_set_black(uncle);
					rb_set_black(parent);
					rb_set_red(gparent);
					node = gparent;
					continue;
				}
			}

			if (parent->rb_left == node)
			{
				register struct rb_node *tmp;
				__rb_rotate_right(parent);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			rb_set_black(parent);
			rb_set_red(gparent);
			__rb_rotate_left(gparent);
		}
	}

	rb_set_black(root.rb_node);
}

void Rbtree::__rb_erase_color(rb_node *node, rb_node *parent)
{
	rb_node *other;

	while ((!node || rb_is_black(node)) && node != root.rb_node)
	{
		if (parent->rb_left == node)
		{
			other = parent->rb_right;
			if (rb_is_red(other))
			{
				rb_set_black(other);
				rb_set_red(parent);
				__rb_rotate_left(parent);
				other = parent->rb_right;
			}
			if ((!other->rb_left || rb_is_black(other->rb_left)) &&
			    (!other->rb_right || rb_is_black(other->rb_right)))
			{
				rb_set_red(other);
				node = parent;
				parent = rb_parent(node);
			}
			else
			{
				if (!other->rb_right || rb_is_black(other->rb_right))
				{
					struct rb_node *o_left;
					if ((o_left = other->rb_left))
						rb_set_black(o_left);
					rb_set_red(other);
					__rb_rotate_right(other);
					other = parent->rb_right;
				}
				rb_set_color(other, rb_color(parent));
				rb_set_black(parent);
				if (other->rb_right)
					rb_set_black(other->rb_right);
				__rb_rotate_left(parent);
				node = root.rb_node;
				break;
			}
		}
		else
		{
			other = parent->rb_left;
			if (rb_is_red(other))
			{
				rb_set_black(other);
				rb_set_red(parent);
				__rb_rotate_right(parent);
				other = parent->rb_left;
			}
			if ((!other->rb_left || rb_is_black(other->rb_left)) &&
			    (!other->rb_right || rb_is_black(other->rb_right)))
			{
				rb_set_red(other);
				node = parent;
				parent = rb_parent(node);
			}
			else
			{
				if (!other->rb_left || rb_is_black(other->rb_left))
				{
					register struct rb_node *o_right;
					if ((o_right = other->rb_right))
						rb_set_black(o_right);
					rb_set_red(other);
					__rb_rotate_left(other);
					other = parent->rb_left;
				}
				rb_set_color(other, rb_color(parent));
				rb_set_black(parent);
				if (other->rb_left)
					rb_set_black(other->rb_left);
				__rb_rotate_right(parent);
				node = root.rb_node;
				break;
			}
		}
	}
	if (node)
		rb_set_black(node);
}

void Rbtree::rb_erase(rb_node *node)
{
	rb_node *child, *parent;
	int color;

	num--;
	//printf("rbtree remove num %d.\n", num);
	if (!node->rb_left)
		child = node->rb_right;
	else if (!node->rb_right)
		child = node->rb_left;
	else
	{
		rb_node *old = node, *left;

		node = node->rb_right;
		while ((left = node->rb_left) != NULL)
			node = left;
		child = node->rb_right;
		parent = rb_parent(node);
		color = rb_color(node);

		if (child)
			rb_set_parent(child, parent);
		if (parent == old) {
			parent->rb_right = child;
			parent = node;
		} else
			parent->rb_left = child;

		node->rb_parent_color = old->rb_parent_color;
		node->rb_right = old->rb_right;
		node->rb_left = old->rb_left;

		if (rb_parent(old))
		{
			if (rb_parent(old)->rb_left == old)
				rb_parent(old)->rb_left = node;
			else
				rb_parent(old)->rb_right = node;
		} else
			root.rb_node = node;

		rb_set_parent(old->rb_left, node);
		if (old->rb_right)
			rb_set_parent(old->rb_right, node);
		goto color;
	}

	parent = rb_parent(node);
	color = rb_color(node);

	if (child)
		rb_set_parent(child, parent);
	if (parent)
	{
		if (parent->rb_left == node)
			parent->rb_left = child;
		else
			parent->rb_right = child;
	}
	else
		root.rb_node = child;

 color:
	if (color == RB_BLACK)
		__rb_erase_color(child, parent);
}

/*
 * This function returns the first node (in sort order) of the tree.
 */
rb_node *Rbtree::rb_first()
{
	rb_node	*n;

	n = root.rb_node;
	if (!n)
		return NULL;
	while (n->rb_left)
		n = n->rb_left;
	return n;
}

rb_node *Rbtree::rb_last()
{
	rb_node	*n;

	n = root.rb_node;
	if (!n)
		return NULL;
	while (n->rb_right)
		n = n->rb_right;
	return n;
}

rb_node *Rbtree::rb_next(rb_node *node)
{
	rb_node *parent;

	if (rb_parent(node) == node)
		return NULL;

	/* If we have a right-hand child, go down and then left as far
	   as we can. */
	if (node->rb_right) {
		node = node->rb_right; 
		while (node->rb_left)
			node=node->rb_left;
		return node;
	}

	/* No right-hand children.  Everything down and left is
	   smaller than us, so any 'next' node must be in the general
	   direction of our parent. Go up the tree; any time the
	   ancestor is a right-hand child of its parent, keep going
	   up. First time it's a left-hand child of its parent, said
	   parent is our 'next' node. */
	while ((parent = rb_parent(node)) && node == parent->rb_right)
		node = parent;

	return parent;
}

rb_node *Rbtree::rb_prev(struct rb_node *node)
{
	rb_node *parent;

	if (rb_parent(node) == node)
		return NULL;

	/* If we have a left-hand child, go down and then right as far
	   as we can. */
	if (node->rb_left) {
		node = node->rb_left; 
		while (node->rb_right)
			node=node->rb_right;
		return node;
	}

	/* No left-hand children. Go up till we find an ancestor which
	   is a right-hand child of its parent */
	while ((parent = rb_parent(node)) && node == parent->rb_left)
		node = parent;

	return parent;
}

void Rbtree::rb_replace_node(rb_node *victim, rb_node *n)
{
	rb_node *parent = rb_parent(victim);

	/* Set the surrounding nodes to point to the replacement */
	if (parent) {
		if (victim == parent->rb_left)
			parent->rb_left = n;
		else
			parent->rb_right = n;
	} else {
		root.rb_node = n;
	}
	if (victim->rb_left)
		rb_set_parent(victim->rb_left, n);
	if (victim->rb_right)
		rb_set_parent(victim->rb_right, n);

	/* Copy the pointers/colour from the victim to the replacement */
	*n = *victim;
}

void Rbtree::rb_link_node(rb_node * node, rb_node * parent, rb_node ** rb_link)
{
	node->rb_parent_color = (unsigned long )parent;
	node->rb_left = node->rb_right = NULL;

	*rb_link = node;
}

void Rbtree::rb_set_compare(compare_f c)
{
	compare = c;
}

void Rbtree::rb_insert(rb_node * node)
{
	rb_node **p = &root.rb_node;
	rb_node *parent = NULL;
	int	rc;
	
	while (*p) {
		parent = *p;
		rc = compare(node->rb_entry, parent->rb_entry);
		if (rc < 0)
			p = &parent->rb_left;
		else
			p = &parent->rb_right;
	}
	rb_link_node(node, parent, p);
	rb_insert_color(node);
	num++;
	//printf("rbtree insert %p num %d.\n", node->rb_entry, num);
}

/* Find and return the nearest element.*/
int Rbtree::rb_find_nearest(rb_node *node, rb_node *parent, rb_node **p)
{
	int	rc = 1;
	
	p = &root.rb_node;
	while (*p) {
		parent = *p;
		rc = compare(node->rb_entry, parent->rb_entry);
		if (rc < 0)
			p = &parent->rb_left;
		else if (rc > 0)
			p = &parent->rb_right;
		else
			break;
	}
	
	return rc;
}

