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
#include <linux/xz.h>

int xz_compress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	(void) src;
	(void) ssize;
	(void) dst;
	(void) dsize;
	return -1;
}

int xz_uncompress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	enum xz_ret ret;
	struct xz_buf b;
	struct xz_dec *s;
	//xz_crc32_init();
	s = xz_dec_init(XZ_SINGLE, 0);
	if (s == NULL) {
		return -1;
	}
	b.in = src;
	b.in_pos = 0;
	b.in_size = ssize;
	b.out = dst;
	b.out_pos = 0;
	b.out_size = dsize;
	ret = xz_dec_run(s, &b);
	xz_dec_end(s);
	return (ret == XZ_STREAM_END) ? b.out_pos : -1;
}
