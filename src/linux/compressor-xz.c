/*
 * Copyright (c) 2013, Alper Akcan <alper.akcan@gmail.com>.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/xz.h>

#include "compressor-xz.h"

struct xz {
	struct xz_dec *state;
	struct xz_buf buffer;
};

void * xz_create (void)
{
	struct xz *xz;
	xz = kmalloc(sizeof(struct xz), GFP_KERNEL);
	if (xz == NULL) {
		return NULL;
	}
	xz->state = xz_dec_init(XZ_PREALLOC, 0);
	if (xz->state == NULL) {
		kfree(xz);
		return NULL;
	}
	return xz;
}

void xz_destroy (void *context)
{
	struct xz *xz;
	if (context == NULL) {
		return;
	}
	xz = context;
	xz_dec_end(xz->state);
	kfree(xz);
}

int xz_uncompress (void *context, void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	enum xz_ret ret;
	struct xz *xz;
	struct xz_buf *b;
	struct xz_dec *s;
	xz = context;
	s = xz->state;
	b = &xz->buffer;
	xz_dec_reset(s);
	b->in = src;
	b->in_pos = 0;
	b->in_size = ssize;
	b->out = dst;
	b->out_pos = 0;
	b->out_size = dsize;
	ret = xz_dec_run(s, b);
	return (ret == XZ_STREAM_END) ? b->out_pos : -1;
}
