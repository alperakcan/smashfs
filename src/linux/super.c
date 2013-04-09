/*
 * Copyright (c) 2013, Alper Akcan <alper.akcan@gmail.com>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of the FreeBSD Project.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/buffer_head.h>
#include <linux/statfs.h>
#include <linux/namei.h>
#include <linux/version.h>

#include "../include/smashfs.h"
#include "bitbuffer.h"
#include "compressor.h"
#include "super.h"

#define errorf(a...) { \
	printk(KERN_ERR "smashfs: " a); \
}

#define debugf(a...) { \
	printk(KERN_INFO "smashfs: " a); \
}

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
	long long parent;
};

static const struct super_operations smashfs_super_ops;
static const struct file_operations smashfs_directory_operations;
static const struct inode_operations smashfs_dir_inode_operations;
static const struct address_space_operations smashfs_aops;

static DEFINE_MUTEX(read_mutex);

static int block_fill (struct super_block *sb, long long number, struct block *block)
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

static int node_fill (struct super_block *sb, long long number, struct node *node)
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
	node->parent     = bitbuffer_getbits(&bb, sbi->super->bits.inode.parent);
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

static struct inode * smashfs_get_inode (struct super_block *sb, long long number)
{
	int rc;
	mode_t mode;
	struct node node;
	struct inode *inode;
	struct smashfs_super_info *sbi;

	enterf();

	sbi = sb->s_fs_info;
	rc = node_fill(sb, number, &node);
	if (rc != 0) {
		errorf("node fill failed\n");
		leavef();
		return ERR_PTR(-EINVAL);
	}

	inode = iget_locked(sb, number + 1);
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

	if (node.type == smashfs_inode_type_directory) {
		mode         = S_IFDIR;
		inode->i_op  = &smashfs_dir_inode_operations;
		inode->i_fop = &smashfs_directory_operations;
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
		unlock_new_inode(inode);
		leavef();
		return ERR_PTR(-EINVAL);
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

	inode->i_mode   = mode;
	inode->i_uid    = node.uid;
	inode->i_gid    = node.gid;
	inode->i_size   = node.size;
	inode->i_blocks = ((node.size + sbi->devblksize - 1) >> sbi->devblksize_log2) + 1;
	inode->i_ctime.tv_sec = node.ctime;
	inode->i_mtime.tv_sec = node.mtime;
	inode->i_atime.tv_sec = node.mtime;
	unlock_new_inode(inode);

	leavef();
	return inode;
}

static int smashfs_read (struct super_block *sb, void *buffer, int offset, int length)
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

	mutex_lock(&read_mutex);

	pages = (length + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;
	data = kcalloc(pages, sizeof(void *), GFP_KERNEL);
	if (data == NULL) {
		errorf("kcalloc failed\n");
		mutex_unlock(&read_mutex);
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

	debugf("offset: %d, length: %d, pages: %d, block: %d, index: %d, blocks: %d\n", offset, length, pages, block, index, blocks);
	bh = kcalloc(blocks, sizeof(struct buffer_head *), GFP_KERNEL);
	if (bh == NULL) {
		errorf("kcalloc failed for buffer heads\n");
		kfree(data);
		mutex_unlock(&read_mutex);
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
			mutex_unlock(&read_mutex);
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
			mutex_unlock(&read_mutex);
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
	mutex_unlock(&read_mutex);
	leavef();
	return length;
}

static int node_read (struct super_block *sb, struct node *node, int (*function) (void *context, void *buffer, long long size), void *context)
{
	int rc;
	long long s;
	long long i;
	long long b;
	void *ubuffer;
	void *cbuffer;
	struct block block;
	struct smashfs_super_info *sbi;

	enterf();

	sbi = sb->s_fs_info;

	ubuffer = kmalloc(sbi->super->block_size, GFP_KERNEL);
	if (ubuffer == NULL) {
		errorf("malloc failed\n");
		leavef();
		return -1;
	}
	cbuffer = kmalloc(sbi->super->block_size, GFP_KERNEL);
	if (cbuffer == NULL) {
		errorf("malloc failed\n");
		kfree(ubuffer);
		leavef();
		return -1;
	}

	s = 0;
	i = node->index;
	b = node->block;
	while (s < node->size) {
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
		rc = function(context, ubuffer + i, min_t(long long, node->size - s, block.size - i));
		if (rc != min_t(long long, node->size - s, block.size - i)) {
			errorf("function failed\n");
			goto bail;
		}
		s += min_t(long long, node->size - s, block.size - i);
		b += 1;
		i = 0;
	}

	kfree(cbuffer);
	kfree(ubuffer);
	leavef();
	return 0;
bail:
	kfree(cbuffer);
	kfree(ubuffer);
	leavef();
	return -1;
}

static int node_read_directory (void *context, void *buffer, long long size)
{
	unsigned char **b;

	enterf();

	b = context;
	memcpy(*b, buffer, size);
	*b += size;

	leavef();
	return size;
}

static int node_read_symbolic_link (void *context, void *buffer, long long size)
{
	unsigned char **b;

	enterf();

	b = context;
	memcpy(*b, buffer, size);
	*b += size;

	leavef();
	return size;
}

static int smashfs_readdir (struct file *filp, void *dirent, filldir_t filldir)
{
	int rc;

	struct node node;
	struct node enode;

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

	enterf();

	debugf("filp->f_pos: %lld\n", filp->f_pos);

	nbuffer = NULL;
	inode = filp->f_path.dentry->d_inode;
	sb = inode->i_sb;
	sbi = sb->s_fs_info;

	rc = node_fill(sb, inode->i_ino - 1, &node);
	if (rc != 0) {
		errorf("node fill failed\n");
		leavef();
		return -EINVAL;
	}

	if (filp->f_pos >= 3 + node.size) {
		debugf("finished reading (%lld, %lld)\n", filp->f_pos, node.size);
		leavef();
		return 0;
	}

	while (filp->f_pos < 3) {
		int i_ino;
		char *name;
		if (filp->f_pos == 0) {
			name = ".";
			s = 1;
			i_ino = inode->i_ino;
		} else {
			name = "..";
			s = 2;
			i_ino = node.parent + 1;
		}
		debugf("calling filldir(%p, %s, %lld, %lld, %d, %d)\n", dirent, name, s, filp->f_pos, i_ino, DT_DIR);
		if (filldir(dirent, name, s, filp->f_pos, i_ino, DT_DIR) < 0) {
			errorf("filldir failed\n");
			leavef();
			return 0;
		}
		filp->f_pos += s;
	}

	nbuffer = kmalloc(node.size, GFP_KERNEL);
	if (nbuffer == NULL) {
		errorf("kmalloc failed\n");
		leavef();
		return -ENOMEM;
	}

	buffer = nbuffer;
	rc = node_read(sb, &node, node_read_directory, &buffer);
	if (rc != 0) {
		errorf("node read failed\n");
		kfree(nbuffer);
		leavef();
		return -EIO;
	}

	buffer = nbuffer;
	bitbuffer_init_from_buffer(&bb, buffer, node.size);
	directory_parent   = bitbuffer_getbits(&bb, sbi->super->bits.inode.directory.parent);
	directory_nentries = bitbuffer_getbits(&bb, sbi->super->bits.inode.directory.nentries);
	bitbuffer_uninit(&bb);

	debugf("number: %lld, parent: %lld, nentries: %lld\n", node.number, directory_parent, directory_nentries);
	s  = sbi->super->bits.inode.directory.parent;
	s += sbi->super->bits.inode.directory.nentries;
	s  = (s + 7) / 8;
	buffer += s;
	if (filp->f_pos < 3 + s) {
		filp->f_pos += s;
	}

	for (e = 0; e < directory_nentries; e++) {
		s  = 0;
		s += sbi->super->bits.inode.directory.entries.number;
		s  = (s + 7) / 8;

		bitbuffer_init_from_buffer(&bb, buffer, s);
		directory_entry_number = bitbuffer_getbits(&bb, sbi->super->bits.inode.directory.entries.number);
		bitbuffer_uninit(&bb);

		buffer += s;

		debugf("  - %s (number: %lld)\n", buffer, directory_entry_number);
		rc = node_fill(sb, directory_entry_number, &enode);
		if (rc != 0) {
			errorf("node fill failed\n");
			kfree(nbuffer);
			leavef();
			return -EINVAL;
		}

		debugf("    filp->f_pos: %lld, %zd\n", filp->f_pos, ((buffer - s) - nbuffer) + 3);
		if (filp->f_pos == ((buffer - s) - nbuffer) + 3) {
			debugf("    calling filldir(%p, %s, %zd, %lld, %lld, %s)\n",
				dirent,
				buffer,
				strlen(buffer),
				filp->f_pos,
				directory_entry_number + 1,
				(enode.type == smashfs_inode_type_regular_file) ? "DT_REG" :
				(enode.type == smashfs_inode_type_directory) ? "DT_DIR" :
				(enode.type == smashfs_inode_type_symbolic_link) ? "DT_LNK" :
				(enode.type == smashfs_inode_type_character_device) ? "DT_CHR" :
				(enode.type == smashfs_inode_type_block_device) ? "DT_BLK" :
				(enode.type == smashfs_inode_type_fifo) ? "DT_FIFO" :
				(enode.type == smashfs_inode_type_socket) ? "DT_SOCK" : "DT_UNKNOWN");
			if (filldir(dirent,
				    buffer,
				    strlen(buffer),
				    filp->f_pos,
				    directory_entry_number,
				    (enode.type == smashfs_inode_type_regular_file) ? DT_REG :
				    (enode.type == smashfs_inode_type_directory) ? DT_DIR :
				    (enode.type == smashfs_inode_type_symbolic_link) ? DT_LNK :
				    (enode.type == smashfs_inode_type_character_device) ? DT_CHR :
				    (enode.type == smashfs_inode_type_block_device) ? DT_BLK :
				    (enode.type == smashfs_inode_type_fifo) ? DT_FIFO :
				    (enode.type == smashfs_inode_type_socket) ? DT_SOCK : DT_UNKNOWN) < 0) {
				errorf("filldir failed\n");
				kfree(nbuffer);
				leavef();
				return 0;
			}
		}

		filp->f_pos += s;
		filp->f_pos += strlen(buffer) + 1;
		buffer += strlen(buffer) + 1;
	}

	kfree(nbuffer);
	leavef();
	return 0;
}

static struct dentry * smashfs_lookup (struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
	int rc;

	struct node node;

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

	enterf();

	inode = NULL;
	nbuffer = NULL;
	sb = dir->i_sb;
	sbi = sb->s_fs_info;

	rc = node_fill(sb, dir->i_ino - 1, &node);
	if (rc != 0) {
		errorf("node fill failed\n");
		leavef();
		return ERR_PTR(-EINVAL);
	}

	nbuffer = kmalloc(node.size, GFP_KERNEL);
	if (nbuffer == NULL) {
		errorf("kmalloc failed\n");
		leavef();
		return ERR_PTR(-ENOMEM);
	}

	buffer = nbuffer;
	rc = node_read(sb, &node, node_read_directory, &buffer);
	if (rc != 0) {
		errorf("node read failed\n");
		kfree(nbuffer);
		leavef();
		return ERR_PTR(-EIO);
	}

	buffer = nbuffer;
	bitbuffer_init_from_buffer(&bb, buffer, node.size);
	directory_parent   = bitbuffer_getbits(&bb, sbi->super->bits.inode.directory.parent);
	directory_nentries = bitbuffer_getbits(&bb, sbi->super->bits.inode.directory.nentries);
	bitbuffer_uninit(&bb);

	debugf("number: %lld, parent: %lld, nentries: %lld\n", node.number, directory_parent, directory_nentries);
	s  = sbi->super->bits.inode.directory.parent;
	s += sbi->super->bits.inode.directory.nentries;
	s  = (s + 7) / 8;
	buffer += s;

	for (e = 0; e < directory_nentries; e++) {
		s  = 0;
		s += sbi->super->bits.inode.directory.entries.number;
		s  = (s + 7) / 8;

		bitbuffer_init_from_buffer(&bb, buffer, s);
		directory_entry_number = bitbuffer_getbits(&bb, sbi->super->bits.inode.directory.entries.number);
		bitbuffer_uninit(&bb);

		buffer += s;

		if (strlen(buffer) == dentry->d_name.len &&
		    strncmp(buffer, dentry->d_name.name, dentry->d_name.len) == 0) {
			debugf("  - %s (number: %lld)\n", buffer, directory_entry_number);
			inode = smashfs_get_inode(sb, directory_entry_number);
			if (inode == NULL) {
				errorf("get inode failed\n");
				leavef();
				return ERR_PTR(-EIO);
			} else {
				leavef();
				return d_splice_alias(inode, dentry);
			}
		}

		buffer += strlen(buffer) + 1;
	}

	kfree(nbuffer);
	leavef();
	return ERR_PTR(-EINVAL);
}

static int smashfs_readpage (struct file *file, struct page *page)
{
	int rc;
	int bytes_filled;
	int max_block;
	void *pgdata;
	char *buffer;
	char *nbuffer;
	struct node node;
	struct inode *inode;
	struct super_block *sb;
	enterf();

	inode = page->mapping->host;
	sb = inode->i_sb;

	rc = node_fill(sb, inode->i_ino - 1, &node);
	if (rc != 0) {
		errorf("node fill failed\n");
		goto bail;
	}

	max_block = (inode->i_size + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;
	bytes_filled = 0;
	pgdata = kmap(page);

	debugf("page index: %ld, node size: %lld, max block: %d\n", page->index, node.size, max_block);
	if (page->index < max_block) {
		if (node.type == smashfs_inode_type_symbolic_link) {
			nbuffer = kmalloc(node.size, GFP_KERNEL);
			if (nbuffer == NULL) {
				errorf("kmalloc failed\n");
				goto bail;
			}
			buffer = nbuffer;
			rc = node_read(sb, &node, node_read_symbolic_link, &buffer);
			if (rc != 0) {
				errorf("node read failed\n");
				kfree(nbuffer);
				leavef();
				goto bail;
			}
			buffer = nbuffer;
			memcpy(pgdata, buffer + (page->index * PAGE_CACHE_SIZE), min_t(long long, node.size, PAGE_CACHE_SIZE));
			bytes_filled = min_t(long long, node.size, PAGE_CACHE_SIZE);
			kfree(nbuffer);
		} else if (node.type == smashfs_inode_type_regular_file) {
		} else {
			errorf("unknown node type: %lld\n", node.type);
			goto bail;
		}
	}

	memset(pgdata + bytes_filled, 0, PAGE_CACHE_SIZE - bytes_filled);
	flush_dcache_page(page);
	kunmap(page);
	SetPageUptodate(page);
	unlock_page(page);
	return 0;
bail:
	kunmap(page);
	ClearPageUptodate(page);
	SetPageError(page);
	unlock_page(page);
	leavef();
	return 0;
}

static int smashfs_statfs (struct dentry *dentry, struct kstatfs *buf)
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

static int smashfs_remount (struct super_block *sb, int *flags, char *data)
{
	enterf();

	*flags |= MS_RDONLY;

	leavef();
	return 0;
}


static void smashfs_put_super (struct super_block *sb)
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

	leavef();
}

static const struct file_operations smashfs_directory_operations = {
	.llseek  = default_llseek,
	.read    = generic_read_dir,
	.readdir = smashfs_readdir,
};

static const struct inode_operations smashfs_dir_inode_operations = {
	.lookup = smashfs_lookup,
};

static const struct address_space_operations smashfs_aops = {
	.readpage = smashfs_readpage
};

static const struct super_operations smashfs_super_ops = {
	.statfs        = smashfs_statfs,
	.put_super     = smashfs_put_super,
	.remount_fs    = smashfs_remount
};

int smashfs_fill_super (struct super_block *sb, void *data, int silent)
{
	int rc;
	char b[BDEVNAME_SIZE];
	struct inode *root;
	struct smashfs_super_info *sbi;
	struct smashfs_super_block *sbl;

	enterf();

	sbi = NULL;
	sbl = NULL;
	sbi = kmalloc(sizeof(struct smashfs_super_info), GFP_KERNEL);
	if (sbi == NULL) {
		errorf("kalloc failed for super info\n");
		goto bail;
	}

	sb->s_fs_info = sbi;
	sbi->compressor = NULL;
	sbi->blocks_table = NULL;
	sbi->inodes_table = NULL;

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
	debugf("      symbolic_link:\n");
	debugf("    block:\n");
	debugf("      offset         : %u\n", sbl->bits.block.offset);
	debugf("      compressed_size: %u\n", sbl->bits.block.compressed_size);
	debugf("      size           : %u\n", sbl->bits.block.size);

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
	sbi->max_inode_size += sbl->bits.inode.parent;

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

	rc = smashfs_read(sb, sbi->inodes_table, sbl->inodes_offset, sbl->inodes_size);
	if (rc != sbl->inodes_size) {
		errorf("read failed for inodes table\n");
		goto bail;
	}

	rc = smashfs_read(sb, sbi->blocks_table, sbl->blocks_offset, sbl->blocks_size);
	if (rc != sbl->blocks_size) {
		errorf("read failed for blocks table\n");
		goto bail;
	}

	sbi->compressor = compressor_create_type(sbl->compression_type);
	if (sbi->compressor == NULL) {
		errorf("compressor create failed\n");
		goto bail;
	}

	sb->s_magic = sbl->magic;
	sb->s_maxbytes = MAX_LFS_FILESIZE;
	sb->s_flags |= MS_RDONLY;
	sb->s_op = &smashfs_super_ops;

	root = smashfs_get_inode(sb, sbl->root);
	if (IS_ERR(root)) {
		errorf("can not get root inode\n");
		goto bail;
	}

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
	kfree(sbl);
	sb->s_fs_info = NULL;
	leavef();
	return -EINVAL;
}
