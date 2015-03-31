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

struct compressor;

struct compressor * compressor_create_name (const char *name);
struct compressor * compressor_create_type (enum smashfs_compression_type type);
int compressor_destroy (struct compressor *compressor);
enum smashfs_compression_type compressor_type (struct compressor *compressor);
int compressor_uncompress (struct compressor *compressor, void *src, unsigned int ssize, void *dst, unsigned int dsize);
