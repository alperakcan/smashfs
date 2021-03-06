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
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/buffer_head.h>
#include <linux/statfs.h>
#include <linux/namei.h>
#include <linux/version.h>

#include "smashfs.h"
#include "bitbuffer.h"
#include "compressor.h"
#include "super.h"

#define errorf(a...) { \
	printk(KERN_ERR "smashfs: " a); \
}

#if 0
#define debugf(a...) { \
	printk(KERN_INFO "smashfs: " a); \
}
#else
#define debugf(a...) do { } while (0);
#endif

#define enterf() { \
	debugf("enter (%s %s:%d)\n", __FUNCTION__, __FILE__, __LINE__); \
}

#define leavef() { \
	debugf("leave (%s %s:%d)\n", __FUNCTION__, __FILE__, __LINE__); \
}

struct block {
	long long offset;
	long long size;
	long long compressed_size;
};

struct node {
	long long number;
	long long type;
	long long owner_mode;
	long long group_mode;
	long long other_mode;
	long long uid;
	long long gid;
	long long ctime;
	long long mtime;
	long long size;
	long long block;
	long long index;
};

struct node_info {
	struct node node;
	struct inode inode;
};

static const struct super_operations smashfs_super_ops;
static const struct file_operations smashfs_directory_operations;
static const struct inode_operations smashfs_dir_inode_operations;
static const struct address_space_operations smashfs_aops;

static struct kmem_cache *smashfs_inode_cachep			= NULL;
static struct kmem_cache *smashfs_block_cachep			= NULL;

static inline void inodecache_init_once (void *foo)
{
	struct node_info *node;
	node = foo;
	inode_init_once(&node->inode);
}

static inline int init_inodecache (void)
{
	smashfs_inode_cachep = kmem_cache_create("smashfs_inode_cache", sizeof(struct node_info), 0, SLAB_HWCACHE_ALIGN | SLAB_RECLAIM_ACCOUNT, inodecache_init_once);
	return smashfs_inode_cachep ? 0 : -ENOMEM;
}


static inline void destroy_inodecache (void)
{
	if (smashfs_inode_cachep != NULL) {
		kmem_cache_destroy(smashfs_inode_cachep);
	}
}

static inline int init_blockcache (size_t size)
{
	smashfs_block_cachep = kmem_cache_create("smashfs_block_cache", size, 0, SLAB_HWCACHE_ALIGN | SLAB_RECLAIM_ACCOUNT, NULL);
	return smashfs_block_cachep ? 0 : -ENOMEM;
}


static inline void destroy_blockcache (void)
{
	if (smashfs_block_cachep != NULL) {
		kmem_cache_destroy(smashfs_block_cachep);
	}
}

static inline struct node_info * smashfs_i (struct inode *inode)
{
	return container_of(inode, struct node_info, inode);
}

static inline int block_fill (struct super_block *sb, long long number, struct block *block)
{
	int rc;
	struct bitbuffer bb;
	struct smashfs_super_info *sbi;

	enterf();

	debugf("looking for block number: %lld\n", number);

	sbi = sb->s_fs_info;
	debugf("blocks_table: %p, blocks_size: %d\n", sbi->blocks_table, sbi->super->blocks_size);

	rc = bitbuffer_init_from_buffer(&bb, sbi->blocks_table, sbi->super->blocks_size);
	if (rc != 0) {
		errorf("bitbuffer init from buffer failed\n");
		leavef();
		return -1;
	}

	bitbuffer_setpos(&bb, number * sbi->max_block_size);
	block->offset           = bitbuffer_getbits(&bb, sbi->super->bits.block.offset);
	block->compressed_size  = bitbuffer_getbits(&bb, sbi->super->bits.block.compressed_size) + sbi->super->min.block.compressed_size;
	block->size             = (number + 1 < sbi->super->blocks) ? sbi->super->block_size : bitbuffer_getbits(&bb, sbi->super->bits.block.size);
	bitbuffer_uninit(&bb);

	debugf("block:\n");
	debugf("  number: %lld\n", number);
	debugf("  offset: %lld\n", block->offset);
	debugf("  csize : %lld\n", block->compressed_size);
	debugf("  size  : %lld\n", block->size);

	leavef();
	return 0;
}

static inline int node_fill (struct super_block *sb, long long number, struct node *node)
{
	int rc;
	struct bitbuffer bb;
	struct smashfs_super_info *sbi;

	enterf();

	debugf("looking for node number: %lld\n", number);

	sbi = sb->s_fs_info;
	debugf("inodes_table: %p, inodes_size: %d", sbi->inodes_table, sbi->super->inodes_size);

	rc = bitbuffer_init_from_buffer(&bb, sbi->inodes_table, sbi->super->inodes_size);
	if (rc != 0) {
		errorf("bitbuffer init for inodes tabled failed\n");
		leavef();
		return -1;
	}

	bitbuffer_setpos(&bb, number * sbi->max_inode_size);
	node->number     = number;
	node->type       = bitbuffer_getbits(&bb, sbi->super->bits.inode.type);
	node->owner_mode = bitbuffer_getbits(&bb, sbi->super->bits.inode.owner_mode);
	node->group_mode = bitbuffer_getbits(&bb, sbi->super->bits.inode.group_mode);
	node->other_mode = bitbuffer_getbits(&bb, sbi->super->bits.inode.other_mode);
	node->uid        = bitbuffer_getbits(&bb, sbi->super->bits.inode.uid);
	node->gid        = bitbuffer_getbits(&bb, sbi->super->bits.inode.gid);
	node->ctime      = bitbuffer_getbits(&bb, sbi->super->bits.inode.ctime);
	node->mtime      = bitbuffer_getbits(&bb, sbi->super->bits.inode.mtime);
	node->size       = bitbuffer_getbits(&bb, sbi->super->bits.inode.size);
	node->block      = bitbuffer_getbits(&bb, sbi->super->bits.inode.block);
	node->index      = bitbuffer_getbits(&bb, sbi->super->bits.inode.index);
	bitbuffer_uninit(&bb);

	if (sbi->super->bits.inode.group_mode == 0) {
		node->group_mode = node->owner_mode;
	}
	if (sbi->super->bits.inode.other_mode == 0) {
		node->other_mode = node->owner_mode;
	}
	if (sbi->super->bits.inode.uid == 0) {
		node->uid = 0;
	}
	if (sbi->super->bits.inode.gid == 0) {
		node->gid = 0;
	}
	if (sbi->super->bits.inode.ctime == 0) {
		node->ctime  = sbi->super->ctime;
	}
	if (sbi->super->bits.inode.mtime == 0) {
		node->mtime = node->ctime;
	}
	node->ctime += sbi->super->min.inode.ctime;
	node->mtime += sbi->super->min.inode.mtime;

	debugf("node\n");
	debugf("  number: %lld\n", node->number);
	debugf("  type  : %lld\n", node->type);
	debugf("  size  : %lld\n", node->size);

	leavef();
	return 0;
}

static inline int smashfs_read_inode (struct super_block *sb, struct inode *inode, long long number)
{
	int rc;
	mode_t mode;
	struct node node;
	struct node_info *node_info;
	struct smashfs_super_info *sbi;

	enterf();

	rc = node_fill(sb, number, &node);
	if (rc != 0) {
		errorf("node fill failed\n");
		leavef();
		return -EINVAL;
	}

	if (node.type == smashfs_inode_type_directory) {
		mode                = S_IFDIR;
		inode->i_op         = &smashfs_dir_inode_operations;
		inode->i_fop        = &smashfs_directory_operations;
	} else if (node.type == smashfs_inode_type_regular_file) {
		mode                = S_IFREG;
		inode->i_fop        = &generic_ro_fops;
		inode->i_data.a_ops = &smashfs_aops;
	} else if (node.type == smashfs_inode_type_symbolic_link) {
		mode                = S_IFLNK;
		inode->i_op         = &page_symlink_inode_operations;
		inode->i_data.a_ops = &smashfs_aops;
	} else {
		errorf("unknown node type: %lld\n", node.type);
		leavef();
		return -EINVAL;
	}

	if (node.owner_mode & smashfs_inode_mode_read) {
		mode |= S_IRUSR;
	}
	if (node.owner_mode & smashfs_inode_mode_write) {
		mode |= S_IWUSR;
	}
	if (node.owner_mode & smashfs_inode_mode_execute) {
		mode |= S_IXUSR;
	}
	if (node.group_mode & smashfs_inode_mode_read) {
		mode |= S_IRGRP;
	}
	if (node.group_mode & smashfs_inode_mode_write) {
		mode |= S_IWGRP;
	}
	if (node.group_mode & smashfs_inode_mode_execute) {
		mode |= S_IXGRP;
	}
	if (node.other_mode & smashfs_inode_mode_read) {
		mode |= S_IROTH;
	}
	if (node.other_mode & smashfs_inode_mode_write) {
		mode |= S_IWOTH;
	}
	if (node.other_mode & smashfs_inode_mode_execute) {
		mode |= S_IXOTH;
	}

	sbi = sb->s_fs_info;

	inode->i_mode   = mode;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	inode->i_uid    = node.uid;
	inode->i_gid    = node.gid;
#else
	i_uid_write(inode, node.uid);
	i_gid_write(inode, node.gid);
#endif
	inode->i_size   = node.size;
	inode->i_blocks = ((node.size + sbi->devblksize - 1) >> sbi->devblksize_log2) + 1;
	inode->i_ctime.tv_sec = node.ctime;
	inode->i_mtime.tv_sec = node.mtime;
	inode->i_atime.tv_sec = node.mtime;

	node_info = smashfs_i(inode);
	memcpy(&node_info->node, &node, sizeof(struct node));

	leavef();
	return 0;
}

static inline struct inode * smashfs_get_inode (struct super_block *sb, long long number)
{
	int rc;
	struct inode *inode;

	enterf();

	inode = iget_locked(sb, number);
	if (inode == NULL) {
		errorf("iget_locked failed\n");
		leavef();
		return ERR_PTR(-ENOMEM);
	}
	if ((inode->i_state & I_NEW) == 0) {
		debugf("inode is not new for %lld\n", number);
		leavef();
		return inode;
	}

	rc = smashfs_read_inode(sb, inode, number);
	if (rc != 0) {
		errorf("read inode failed\n");
		iget_failed(inode);
		leavef();
		return ERR_PTR(-EINVAL);
	}

	unlock_new_inode(inode);

	leavef();
	return inode;
}

static inline int smashfs_read (struct super_block *sb, void *buffer, int offset, int length)
{
	int i;
	int b;
	int in;
	int page;
	int avail;
	int bytes;
	int pages;
	int index;
	int block;
	int blocks;
	int pageoff;

	void **data;
	struct buffer_head **bh;
	struct smashfs_super_info *sbi;

	enterf();

	pages = (length + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;
	data = kcalloc(pages, sizeof(void *), GFP_KERNEL);
	if (data == NULL) {
		errorf("kcalloc failed\n");
		leavef();
		return -ENOMEM;
	}
	for (i = 0; i < pages; i++, buffer += PAGE_CACHE_SIZE) {
		data[i] = buffer;
	}

	sbi = sb->s_fs_info;
	index = offset & ((1 << sbi->devblksize_log2) - 1);
	block = offset >> sbi->devblksize_log2;
	blocks = ((length + sbi->devblksize - 1) >> sbi->devblksize_log2) + 1;

	debugf("devblksize: %d, devblksize_log2: %d, offset: %d, length: %d, pages: %d, block: %d, index: %d, blocks: %d\n", sbi->devblksize, sbi->devblksize_log2, offset, length, pages, block, index, blocks);
	bh = kcalloc(blocks, sizeof(struct buffer_head *), GFP_KERNEL);
	if (bh == NULL) {
		errorf("kcalloc failed for buffer heads\n");
		kfree(data);
		leavef();
		return -ENOMEM;
	}

	b = 0;
	bytes = -index;
	debugf("bytes: %d, length: %d, bytes < length: %d\n", bytes, length, bytes < length);
	while (bytes < length) {
		bh[b] = sb_getblk(sb, block);
		if (bh[b] == NULL) {
			errorf("sb_getblk failed\n");
			for (i = 0; i < b; i++) {
				put_bh(bh[i]);
			}
			kfree(bh);
			kfree(data);
			leavef();
			return -EIO;
		}
		b += 1;
		block += 1;
		bytes += sbi->devblksize;
	}

	ll_rw_block(READ, b, bh);
	for (i = 0; i < b; i++) {
		wait_on_buffer(bh[i]);
		if (!buffer_uptodate(bh[i])) {
			errorf("buffer_uptodate failed\n");
			for (i = 0; i < b; i++) {
				put_bh(bh[i]);
			}
			kfree(bh);
			kfree(data);
			leavef();
			return -EIO;
		}
	}

	page = 0;
	pageoff = 0;
	for (bytes = length, i = 0; i < b; i++) {
		in = min(bytes, sbi->devblksize - index);
		bytes -= in;
		while (in) {
			if (pageoff == PAGE_CACHE_SIZE) {
				page++;
				pageoff = 0;
			}
			avail = min_t(int, in, PAGE_CACHE_SIZE - pageoff);
			memcpy(data[page] + pageoff, bh[i]->b_data + index, avail);
			in -= avail;
			pageoff += avail;
			index += avail;
		}
		index = 0;
		put_bh(bh[i]);
	}

	for (; i < b; i++) {
		put_bh(bh[i]);
	}

	kfree(bh);
	kfree(data);
	leavef();
	return length;
}

static inline int node_read (struct super_block *sb, struct node *node, int (*function) (void *context, void *buffer, long long size), void *context, long long offset, long long size)
{
	int rc;
	long long s;
	long long i;
	long long b;
	long long n;
	long long o;
	long long l;
	void *ubuffer;
	void *cbuffer;
	struct block block;
	struct smashfs_super_info *sbi;

	enterf();
	debugf("offset: %lld, size: %lld\n", offset, size);

	ubuffer = NULL;
	cbuffer = NULL;

	sbi = sb->s_fs_info;

	ubuffer = kmem_cache_alloc(smashfs_block_cachep, GFP_NOIO);
	if (ubuffer == NULL) {
		errorf("malloc failed\n");
		goto bail;
	}
	cbuffer = kmem_cache_alloc(smashfs_block_cachep, GFP_NOIO);
	if (cbuffer == NULL) {
		errorf("malloc failed\n");
		kmem_cache_free(smashfs_block_cachep, ubuffer);
		goto bail;
	}

	o = offset + node->index + (node->block * sbi->super->block_size);
	i = o & ((1 << sbi->super->block_log2) - 1);
	b = o >> sbi->super->block_log2;
	n = ((size + sbi->super->block_size - 1) >> sbi->super->block_log2) + 1;
	debugf("offset: %lld, index: %lld, block: %lld, blocks: %lld\n", offset, i, b, n);

	s = 0;
	while (s < size) {
		rc = block_fill(sb, b, &block);
		if (rc != 0) {
			errorf("block fill failed\n");
			goto bail;
		}
		if (block.size > sbi->super->block_size) {
			errorf("logic error\n");
			goto bail;
		}

		rc = smashfs_read(sb, cbuffer, sbi->super->entries_offset + block.offset, block.compressed_size);
		if (rc != block.compressed_size) {
			errorf("read block failed");
			goto bail;
		}
		rc = compressor_uncompress(sbi->compressor, cbuffer, block.compressed_size, ubuffer, block.size);
		if (rc != block.size) {
			errorf("uncompress failed");
			goto bail;
		}

		l = min_t(long long, size - s, block.size - i);
		rc = function(context, ubuffer + i, l);
		if (rc != l) {
			errorf("function failed\n");
			goto bail;
		}
		s += l;
		b += 1;
		i = 0;
	}

	kmem_cache_free(smashfs_block_cachep, cbuffer);
	kmem_cache_free(smashfs_block_cachep, ubuffer);
	leavef();
	return 0;
bail:	if (cbuffer != NULL) {
		kmem_cache_free(smashfs_block_cachep, cbuffer);
	}
	if (ubuffer != NULL) {
		kmem_cache_free(smashfs_block_cachep, ubuffer);
	}
	leavef();
	return -1;
}

static inline int node_read_directory (void *context, void *buffer, long long size)
{
	unsigned char **b;

	enterf();

	b = context;
	memcpy(*b, buffer, size);
	*b += size;

	leavef();
	return size;
}

static inline int node_read_symbolic_link (void *context, void *buffer, long long size)
{
	unsigned char **b;

	enterf();

	b = context;
	memcpy(*b, buffer, size);
	*b += size;

	leavef();
	return size;
}

static inline int node_read_regular_file (void *context, void *buffer, long long size)
{
	unsigned char **b;

	enterf();

	b = context;
	memcpy(*b, buffer, size);
	*b += size;

	leavef();
	return size;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
static int smashfs_readdir (struct file *file, void *dirent, filldir_t filldir)
#else
static inline int smashfs_readdir (struct file *file, struct dir_context *dirent)
#endif
{
	int rc;

	struct node *node;

	char *buffer;
	char *nbuffer;
	struct bitbuffer bb;

	struct inode *inode;
	struct super_block *sb;
	struct smashfs_super_info *sbi;

	long long e;
	long long s;
	long long directory_parent;
	long long directory_nentries;
	long long directory_entry_number;
	long long directory_entry_length;
	long long directory_entry_type;

	enterf();

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	debugf("file->f_pos: %lld\n", file->f_pos);
#else
	debugf("dirent->pos: %lld\n", dirent->pos);
#endif

	nbuffer = NULL;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	inode = file->f_path.dentry->d_inode;
#else
	inode = file_inode(file);
#endif
	sb = inode->i_sb;
	sbi = sb->s_fs_info;

	node = &(smashfs_i(inode)->node);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	if (file->f_pos >= 3 + node->size) {
		debugf("finished reading (%lld, %lld)\n", file->f_pos, node->size);
#else
	if (dirent->pos >= 3 + node->size) {
		debugf("finished reading (%lld, %lld)\n", dirent->pos, node->size);
#endif
		leavef();
		return 0;
	}

	nbuffer = kmalloc(node->size, GFP_KERNEL);
	if (nbuffer == NULL) {
		errorf("kmalloc failed\n");
		leavef();
		return -ENOMEM;
	}

	buffer = nbuffer;
	rc = node_read(sb, node, node_read_directory, &buffer, 0, node->size);
	if (rc != 0) {
		errorf("node read failed\n");
		kfree(nbuffer);
		leavef();
		return -EIO;
	}

	buffer = nbuffer;
	bitbuffer_init_from_buffer(&bb, buffer, node->size);
	directory_parent   = bitbuffer_getbits(&bb, sbi->super->bits.inode.directory.parent);
	directory_nentries = bitbuffer_getbits(&bb, sbi->super->bits.inode.directory.nentries);
	bitbuffer_uninit(&bb);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	while (file->f_pos < 3) {
#else
	while (dirent->pos < 3) {
#endif
		int i_ino;
		char *name;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
		if (file->f_pos == 0) {
#else
		if (dirent->pos == 0) {
#endif
			name = ".";
			s = 1;
			i_ino = inode->i_ino;
		} else {
			name = "..";
			s = 2;
			i_ino = directory_parent + 1;
		}
		debugf("calling filldir(%p, %s, %lld, %lld, %d, %d)\n", dirent, name, s, file->f_pos, i_ino, DT_DIR);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
		if (filldir(dirent, name, s, file->f_pos, i_ino, DT_DIR) < 0) {
#else
		if (dir_emit(dirent, name, s, i_ino, DT_DIR) == 0) {
#endif
			debugf("filldir failed\n");
			kfree(nbuffer);
			leavef();
			return 0;
		}
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
		file->f_pos += s;
#else
		dirent->pos += s;
#endif
	}

	debugf("number: %lld, parent: %lld, nentries: %lld\n", node->number, directory_parent, directory_nentries);
	s  = 0;
	s += sbi->super->bits.inode.directory.parent;
	s += sbi->super->bits.inode.directory.nentries;
	s  = (s + 7) / 8;
	buffer += s;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	if (file->f_pos < 3 + s) {
		file->f_pos += s;
	}
#else
	if (dirent->pos < 3 + s) {
		dirent->pos += s;
	}
#endif

	for (e = 0; e < directory_nentries; e++) {
		s  = 0;
		s += sbi->super->bits.inode.directory.entries.number;
		s += sbi->super->bits.inode.directory.entries.length;
		s += sbi->super->bits.inode.directory.entries.type;
		s  = (s + 7) / 8;

		bitbuffer_init_from_buffer(&bb, buffer, s);
		directory_entry_number = bitbuffer_getbits(&bb, sbi->super->bits.inode.directory.entries.number);
		directory_entry_length = bitbuffer_getbits(&bb, sbi->super->bits.inode.directory.entries.length);
		directory_entry_type = bitbuffer_getbits(&bb, sbi->super->bits.inode.directory.entries.type);
		bitbuffer_uninit(&bb);

		buffer += s;

		debugf("  - %lld, f_pos: %lld, %zd\n", directory_entry_number, file->f_pos, ((buffer - s) - nbuffer) + 3);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
		if (file->f_pos == ((buffer - s) - nbuffer) + 3) {
#else
		if (dirent->pos == ((buffer - s) - nbuffer) + 3) {
#endif
			debugf("    calling filldir(%p, %lld, %lld, %lld, %s)\n",
				dirent,
				directory_entry_length,
				file->f_pos,
				directory_entry_number + 1,
				(directory_entry_type == smashfs_inode_type_regular_file) ? "DT_REG" :
				(directory_entry_type == smashfs_inode_type_directory) ? "DT_DIR" :
				(directory_entry_type == smashfs_inode_type_symbolic_link) ? "DT_LNK" :
				(directory_entry_type == smashfs_inode_type_character_device) ? "DT_CHR" :
				(directory_entry_type == smashfs_inode_type_block_device) ? "DT_BLK" :
				(directory_entry_type == smashfs_inode_type_fifo) ? "DT_FIFO" :
				(directory_entry_type == smashfs_inode_type_socket) ? "DT_SOCK" : "DT_UNKNOWN");
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
			if (filldir(dirent,
				    buffer,
				    directory_entry_length,
				    file->f_pos,
				    directory_entry_number + 1,
				    (directory_entry_type == smashfs_inode_type_regular_file) ? DT_REG :
				    (directory_entry_type == smashfs_inode_type_directory) ? DT_DIR :
				    (directory_entry_type == smashfs_inode_type_symbolic_link) ? DT_LNK :
				    (directory_entry_type == smashfs_inode_type_character_device) ? DT_CHR :
				    (directory_entry_type == smashfs_inode_type_block_device) ? DT_BLK :
				    (directory_entry_type == smashfs_inode_type_fifo) ? DT_FIFO :
				    (directory_entry_type == smashfs_inode_type_socket) ? DT_SOCK : DT_UNKNOWN) < 0) {
#else
			if (dir_emit(dirent,
				    buffer,
				    directory_entry_length,
				    directory_entry_number + 1,
				    (directory_entry_type == smashfs_inode_type_regular_file) ? DT_REG :
				    (directory_entry_type == smashfs_inode_type_directory) ? DT_DIR :
				    (directory_entry_type == smashfs_inode_type_symbolic_link) ? DT_LNK :
				    (directory_entry_type == smashfs_inode_type_character_device) ? DT_CHR :
				    (directory_entry_type == smashfs_inode_type_block_device) ? DT_BLK :
				    (directory_entry_type == smashfs_inode_type_fifo) ? DT_FIFO :
				    (directory_entry_type == smashfs_inode_type_socket) ? DT_SOCK : DT_UNKNOWN) == 0) {
#endif
				debugf("filldir failed\n");
				kfree(nbuffer);
				leavef();
				return 0;
			}
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
			file->f_pos += s;
			file->f_pos += directory_entry_length;
#else
			dirent->pos += s;
			dirent->pos += directory_entry_length;
#endif
		}

		buffer += directory_entry_length;
	}

	kfree(nbuffer);
	leavef();
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
static inline struct dentry * smashfs_lookup (struct inode *dir, struct dentry *dentry, struct nameidata *nd)
#else
static inline struct dentry * smashfs_lookup (struct inode *dir, struct dentry *dentry, unsigned int flags)
#endif
{
	int rc;

	struct node *node;

	char *buffer;
	char *nbuffer;
	struct bitbuffer bb;

	struct inode *inode;
	struct super_block *sb;
	struct smashfs_super_info *sbi;

	long long e;
	long long s;
	long long directory_parent;
	long long directory_nentries;
	long long directory_entry_number;
	long long directory_entry_length;

	enterf();

	inode = NULL;
	nbuffer = NULL;
	sb = dir->i_sb;
	sbi = sb->s_fs_info;

	node = &(smashfs_i(dir)->node);

	nbuffer = kmalloc(node->size, GFP_KERNEL);
	if (nbuffer == NULL) {
		errorf("kmalloc failed\n");
		leavef();
		return ERR_PTR(-ENOMEM);
	}

	buffer = nbuffer;
	rc = node_read(sb, node, node_read_directory, &buffer, 0, node->size);
	if (rc != 0) {
		errorf("node read failed\n");
		kfree(nbuffer);
		leavef();
		return ERR_PTR(-EIO);
	}

	buffer = nbuffer;
	bitbuffer_init_from_buffer(&bb, buffer, node->size);
	directory_parent   = bitbuffer_getbits(&bb, sbi->super->bits.inode.directory.parent);
	directory_nentries = bitbuffer_getbits(&bb, sbi->super->bits.inode.directory.nentries);
	bitbuffer_uninit(&bb);

	debugf("number: %lld, parent: %lld, nentries: %lld\n", node->number, directory_parent, directory_nentries);
	s  = 0;
	s += sbi->super->bits.inode.directory.parent;
	s += sbi->super->bits.inode.directory.nentries;
	s  = (s + 7) / 8;
	buffer += s;

	for (e = 0; e < directory_nentries; e++) {
		s  = 0;
		s += sbi->super->bits.inode.directory.entries.number;
		s += sbi->super->bits.inode.directory.entries.length;
		s += sbi->super->bits.inode.directory.entries.type;
		s  = (s + 7) / 8;

		bitbuffer_init_from_buffer(&bb, buffer, s);
		directory_entry_number = bitbuffer_getbits(&bb, sbi->super->bits.inode.directory.entries.number);
		directory_entry_length = bitbuffer_getbits(&bb, sbi->super->bits.inode.directory.entries.length);
		/* directory_entry_type */ bitbuffer_skipbits(&bb, sbi->super->bits.inode.directory.entries.type);
		bitbuffer_uninit(&bb);

		buffer += s;

		if (dentry->d_name.name[0] < buffer[0]) {
			break;
		}

		if (directory_entry_length == dentry->d_name.len &&
		    memcmp(buffer, dentry->d_name.name, dentry->d_name.len) == 0) {
			debugf("  - %lld\n", directory_entry_number);
			inode = smashfs_get_inode(sb, directory_entry_number);
			if (inode == NULL) {
				errorf("get inode failed\n");
				leavef();
				return ERR_PTR(-EIO);
			} else {
				kfree(nbuffer);
				leavef();
				return d_splice_alias(inode, dentry);
			}
		}

		buffer += directory_entry_length;
	}

	kfree(nbuffer);
	leavef();
	return ERR_PTR(-ENOENT);
}

static inline int smashfs_readpage (struct file *file, struct page *page)
{
	int rc;
	int bytes_filled;
	int max_block;
	void *pgdata;
	char *buffer;
	long long size;
	struct node *node;
	struct inode *inode;
	struct super_block *sb;

	enterf();

	inode = page->mapping->host;
	sb = inode->i_sb;

	node = &(smashfs_i(inode)->node);

	max_block = (inode->i_size + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;
	bytes_filled = 0;
	pgdata = kmap(page);

	debugf("page index: %ld, node size: %lld, max block: %d\n", page->index, node->size, max_block);
	if (page->index < max_block) {
		if (node->type == smashfs_inode_type_symbolic_link) {
			buffer = pgdata;
			size = min_t(long long, node->size - (page->index * PAGE_CACHE_SIZE), PAGE_CACHE_SIZE);
			rc = node_read(sb, node, node_read_symbolic_link, &buffer, page->index * PAGE_CACHE_SIZE, size);
			if (rc != 0) {
				errorf("node read failed\n");
				leavef();
				goto bail;
			}
			bytes_filled = size;
		} else if (node->type == smashfs_inode_type_regular_file) {
			buffer = pgdata;
			size = min_t(long long, node->size - (page->index * PAGE_CACHE_SIZE), PAGE_CACHE_SIZE);
			rc = node_read(sb, node, node_read_regular_file, &buffer, page->index * PAGE_CACHE_SIZE, size);
			if (rc != 0) {
				errorf("node read failed\n");
				leavef();
				goto bail;
			}
			bytes_filled = size;
		} else {
			errorf("unknown node type: %lld\n", node->type);
			goto bail;
		}
	}

	memset(pgdata + bytes_filled, 0, PAGE_CACHE_SIZE - bytes_filled);
	flush_dcache_page(page);
	kunmap(page);
	SetPageUptodate(page);
	unlock_page(page);

	leavef();
	return 0;
bail:
	kunmap(page);
	ClearPageUptodate(page);
	SetPageError(page);
	unlock_page(page);
	leavef();
	return 0;
}

static inline struct inode * smashfs_alloc_inode (struct super_block *sb)
{
	struct node_info *node;
	node = kmem_cache_alloc(smashfs_inode_cachep, GFP_KERNEL);
	return node ? &node->inode : NULL;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)

static void smashfs_destroy_inode (struct inode *inode)
{
	kmem_cache_free(smashfs_inode_cachep, smashfs_i(inode));
}

#else

static inline void smashfs_i_callback (struct rcu_head *head)
{
	struct inode *inode;
	inode = container_of(head, struct inode, i_rcu);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	INIT_LIST_HEAD(&inode->i_dentry);
#endif
	kmem_cache_free(smashfs_inode_cachep, smashfs_i(inode));
}

static inline void smashfs_destroy_inode(struct inode *inode)
{
	call_rcu(&inode->i_rcu, smashfs_i_callback);
}

#endif

static inline int smashfs_statfs (struct dentry *dentry, struct kstatfs *buf)
{
	u64 id;
	struct smashfs_super_info *sbi;

	enterf();

	sbi = dentry->d_sb->s_fs_info;
	id = huge_encode_dev(dentry->d_sb->s_bdev->bd_dev);
	buf->f_type = SMASHFS_MAGIC;
	buf->f_bsize = sbi->super->block_size;
	buf->f_blocks = sbi->super->blocks;
	buf->f_bfree = 0;
	buf->f_bavail = 0;
	buf->f_files = sbi->super->inodes;
	buf->f_ffree = 0;
	buf->f_namelen = SMASHFS_NAME_LEN;
	buf->f_fsid.val[0] = (u32) id;
	buf->f_fsid.val[1] = (u32) (id >> 32);

	leavef();
	return 0;
}

static inline int smashfs_remount (struct super_block *sb, int *flags, char *data)
{
	enterf();

	*flags |= MS_RDONLY;

	leavef();
	return 0;
}


static inline void smashfs_put_super (struct super_block *sb)
{
	struct smashfs_super_info *sbi;

	enterf();

	if (sb->s_fs_info == NULL) {
		debugf("sb->s_fs_info is null\n");
		leavef();
		return;
	}
	sbi = sb->s_fs_info;
	sb->s_fs_info = NULL;
	kfree(sbi->inodes_table);
	kfree(sbi->blocks_table);
	compressor_destroy(sbi->compressor);
	kfree(sbi->super);
	kfree(sbi);
	destroy_blockcache();

	leavef();
}

static const struct file_operations smashfs_directory_operations = {
	.llseek  = default_llseek,
	.read    = generic_read_dir,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	.readdir = smashfs_readdir,
#else
	.iterate = smashfs_readdir,
#endif
};

static const struct inode_operations smashfs_dir_inode_operations = {
	.lookup = smashfs_lookup,
};

static const struct address_space_operations smashfs_aops = {
	.readpage = smashfs_readpage
};

static const struct super_operations smashfs_super_ops = {
	.alloc_inode   = smashfs_alloc_inode,
	.destroy_inode = smashfs_destroy_inode,
	.statfs        = smashfs_statfs,
	.put_super     = smashfs_put_super,
	.remount_fs    = smashfs_remount
};

static inline int smashfs_fill_super (struct super_block *sb, void *data, int silent)
{
	int rc;
	char b[BDEVNAME_SIZE];
	void *cbuffer;
	struct inode *root;
	struct smashfs_super_info *sbi;
	struct smashfs_super_block *sbl;

	enterf();

	sbi = NULL;
	sbl = NULL;
	cbuffer = NULL;
	sbi = kmalloc(sizeof(struct smashfs_super_info), GFP_KERNEL);
	if (sbi == NULL) {
		errorf("kalloc failed for super info\n");
		goto bail;
	}

	sb->s_fs_info = sbi;
	sbi->compressor = NULL;
	sbi->blocks_table = NULL;
	sbi->inodes_table = NULL;

	(void) b;
	debugf("devname: %s\n", bdevname(sb->s_bdev, b));

	sbi->devblksize = sb_min_blocksize(sb, BLOCK_SIZE);
	sbi->devblksize_log2 = ffz(~sbi->devblksize);

	debugf("dev block size: %d, log2: %d", sbi->devblksize, sbi->devblksize_log2);

	sbl = kmalloc(sizeof(struct smashfs_super_block), GFP_KERNEL);
	if (sbl == NULL) {
		errorf("kalloc failed for super block\n");
		goto bail;
	}
	sbi->super = sbl;

	rc = smashfs_read(sb, sbl, SMASHFS_START, sizeof(struct smashfs_super_block));
	if (rc != sizeof(struct smashfs_super_block)) {
		errorf("could not read super block\n");
		goto bail;
	}

	if (sbl->magic != SMASHFS_MAGIC) {
		errorf("magic mismatch\n");
		goto bail;
	}

	debugf("super block:\n");
	debugf("  magic         : 0x%08x, %u\n", sbl->magic, sbl->magic);
	debugf("  version       : 0x%08x, %u\n", sbl->version, sbl->version);
	debugf("  ctime         : 0x%08x, %u\n", sbl->ctime, sbl->ctime);
	debugf("  block_size    : 0x%08x, %u\n", sbl->block_size, sbl->block_size);
	debugf("  block_log2    : 0x%08x, %u\n", sbl->block_log2, sbl->block_log2);
	debugf("  inodes        : 0x%08x, %u\n", sbl->inodes, sbl->inodes);
	debugf("  blocks        : 0x%08x, %u\n", sbl->blocks, sbl->blocks);
	debugf("  root          : 0x%08x, %u\n", sbl->root, sbl->root);
	debugf("  inodes_offset : 0x%08x, %u\n", sbl->inodes_offset, sbl->inodes_offset);
	debugf("  inodes_size   : 0x%08x, %u\n", sbl->inodes_size, sbl->inodes_size);
	debugf("  inodes_csize  : 0x%08x, %u\n", sbl->inodes_csize, sbl->inodes_csize);
	debugf("  blocks_offset : 0x%08x, %u\n", sbl->blocks_offset, sbl->blocks_offset);
	debugf("  blocks_size   : 0x%08x, %u\n", sbl->blocks_size, sbl->blocks_size);
	debugf("  entries_offset: 0x%08x, %u\n", sbl->entries_offset, sbl->entries_offset);
	debugf("  entries_size  : 0x%08x, %u\n", sbl->entries_size, sbl->entries_size);
	debugf("  bits:\n");
	debugf("    min:\n");
	debugf("      inode:\n");
	debugf("        ctime : 0x%08x, %u\n", sbl->min.inode.ctime, sbl->min.inode.ctime);
	debugf("        mtime : 0x%08x, %u\n", sbl->min.inode.mtime, sbl->min.inode.mtime);
	debugf("      block:\n");
	debugf("        compressed_size : 0x%08x, %u\n", sbl->min.block.compressed_size, sbl->min.block.compressed_size);
	debugf("    inode:\n");
	debugf("      type      : %u\n", sbl->bits.inode.type);
	debugf("      owner_mode: %u\n", sbl->bits.inode.owner_mode);
	debugf("      group_mode: %u\n", sbl->bits.inode.group_mode);
	debugf("      other_mode: %u\n", sbl->bits.inode.other_mode);
	debugf("      uid       : %u\n", sbl->bits.inode.uid);
	debugf("      gid       : %u\n", sbl->bits.inode.gid);
	debugf("      ctime     : %u\n", sbl->bits.inode.ctime);
	debugf("      mtime     : %u\n", sbl->bits.inode.mtime);
	debugf("      size      : %u\n", sbl->bits.inode.size);
	debugf("      block     : %u\n", sbl->bits.inode.block);
	debugf("      index     : %u\n", sbl->bits.inode.index);
	debugf("      regular_file:\n");
	debugf("      directory:\n");
	debugf("        parent   : %u\n", sbl->bits.inode.directory.parent);
	debugf("        nentries : %u\n", sbl->bits.inode.directory.nentries);
	debugf("        entries:\n");
	debugf("          number : %u\n", sbl->bits.inode.directory.entries.number);
	debugf("          length : %u\n", sbl->bits.inode.directory.entries.length);
	debugf("      symbolic_link:\n");
	debugf("    block:\n");
	debugf("      offset         : %u\n", sbl->bits.block.offset);
	debugf("      compressed_size: %u\n", sbl->bits.block.compressed_size);
	debugf("      size           : %u\n", sbl->bits.block.size);

	rc = init_blockcache(sbi->super->block_size);
	if (rc != 0) {
		errorf("can not create block cache");
		goto bail;
	}

	sbi->compressor = compressor_create_type(sbl->compression_type);
	if (sbi->compressor == NULL) {
		errorf("compressor create failed\n");
		goto bail;
	}

	sbi->max_inode_size  = 0;
	sbi->max_inode_size += sbl->bits.inode.type;
	sbi->max_inode_size += sbl->bits.inode.owner_mode;
	sbi->max_inode_size += sbl->bits.inode.group_mode;
	sbi->max_inode_size += sbl->bits.inode.other_mode;
	sbi->max_inode_size += sbl->bits.inode.uid;;
	sbi->max_inode_size += sbl->bits.inode.gid;
	sbi->max_inode_size += sbl->bits.inode.ctime;
	sbi->max_inode_size += sbl->bits.inode.mtime;
	sbi->max_inode_size += sbl->bits.inode.size;
	sbi->max_inode_size += sbl->bits.inode.block;
	sbi->max_inode_size += sbl->bits.inode.index;

	sbi->inodes_table = kmalloc(sbl->inodes_size, GFP_KERNEL);
	if (sbi->inodes_table == NULL) {
		errorf("kmalloc failed for inodes table\n");
		goto bail;
	}

	sbi->max_block_size  = 0;
	sbi->max_block_size += sbl->bits.block.offset;
	sbi->max_block_size += sbl->bits.block.compressed_size;

	sbi->blocks_table = kmalloc(sbl->blocks_size, GFP_KERNEL);

	if (sbi->blocks_table == NULL) {
		errorf("kmalloc failed for blocks table\n");
		goto bail;
	}

	cbuffer = kmem_cache_alloc(smashfs_block_cachep, GFP_NOIO);
	if (cbuffer == NULL) {
		errorf("kmalloc failed\n");
		goto bail;
	}
	rc = smashfs_read(sb, cbuffer, sbl->inodes_offset, sbl->inodes_csize);
	if (rc != sbl->inodes_csize) {
		errorf("read failed for inodes table\n");
		goto bail;
	}
	rc = compressor_uncompress(sbi->compressor, cbuffer, sbl->inodes_csize, sbi->inodes_table, sbl->inodes_size);
	if (rc != sbl->inodes_size) {
		errorf("uncompress failed\n");
		goto bail;
	}
	kmem_cache_free(smashfs_block_cachep, cbuffer);
	cbuffer = NULL;

	rc = smashfs_read(sb, sbi->blocks_table, sbl->blocks_offset, sbl->blocks_size);
	if (rc != sbl->blocks_size) {
		errorf("read failed for blocks table\n");
		goto bail;
	}

	sb->s_magic = sbl->magic;
	sb->s_maxbytes = MAX_LFS_FILESIZE;
	sb->s_flags |= MS_RDONLY;
	sb->s_op = &smashfs_super_ops;

	root = new_inode(sb);
	if (IS_ERR(root)) {
		errorf("can not get root inode\n");
		goto bail;
	}
	rc = smashfs_read_inode(sb, root, sbl->root);
	if (rc != 0) {
		errorf("can not read root inode\n");
		goto bail;
	}
	insert_inode_hash(root);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
	sb->s_root = d_alloc_root(root);
#else
	sb->s_root = d_make_root(root);
#endif
	if (!sb->s_root) {
		errorf("d_alloc_root failed\n");
		iput(root);
		goto bail;
	}

	if (cbuffer != NULL) {
		kmem_cache_free(smashfs_block_cachep, cbuffer);
	}
	leavef();
	return 0;
bail:
	if (sbi != NULL) {
		if (sbi->blocks_table != NULL) {
			kfree(sbi->blocks_table);
		}
		if (sbi->inodes_table != NULL) {
			kfree(sbi->inodes_table);
		}
		if (sbi->compressor != NULL) {
			compressor_destroy(sbi->compressor);
		}
		kfree(sbi);
	}
	if (cbuffer != NULL) {
		kmem_cache_free(smashfs_block_cachep, cbuffer);
	}
	if (sbl != NULL) {
		kfree(sbl);
	}
	sb->s_fs_info = NULL;
	destroy_blockcache();
	leavef();
	return -EINVAL;
}

static inline struct dentry * smashfs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, smashfs_fill_super);
}

static struct file_system_type smashfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "smashfs",
	.mount		= smashfs_mount,
	.kill_sb	= kill_block_super,
	.fs_flags	= FS_REQUIRES_DEV,
};

static int __init init_smashfs_fs (void)
{
	int rc;
	rc = init_inodecache();
	if (rc != 0) {
		errorf("inode cache init failed\n");
		goto bail;
	}
	rc = register_filesystem(&smashfs_fs_type);
	printk(KERN_INFO "smashfs: (c) 2013 Alper Akcan <alper.akcan@gmail.com>\n");
	return rc;
bail:
	destroy_inodecache();
	return rc;
}

static void __exit exit_smashfs_fs (void)
{
	unregister_filesystem(&smashfs_fs_type);
	destroy_inodecache();
}

module_init(init_smashfs_fs)
module_exit(exit_smashfs_fs)
MODULE_LICENSE("GPL");
