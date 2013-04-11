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

#include <string.h>

int none_compress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	if (dsize < ssize) {
		return -1;
	}
	memcpy(dst, src, ssize);
	return ssize;
}

int none_uncompress (void *src, unsigned int ssize, void *dst, unsigned int dsize)
{
	if (dsize < ssize) {
		return -1;
	}
	memcpy(dst, src, ssize);
	return ssize;
}
