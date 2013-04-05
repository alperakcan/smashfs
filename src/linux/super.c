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

#include "../include/smashfs.h"
#include "bitbuffer.h"
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
	vfree(sbi->inodes_table);
	vfree(sbi->blocks_table);
	kfree(sbi->super);
	kfree(sbi);
	leavef();
}

static const struct super_operations smashfs_super_ops = {
	.statfs        = smashfs_statfs,
	.put_super     = smashfs_put_super,
	.remount_fs    = smashfs_remount
};

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
	pages = (length + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;
	data = kcalloc(pages, sizeof(void *), GFP_KERNEL);
	if (data == NULL) {
		errorf("kcalloc failed\n");
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
		return -ENOMEM;
	}
	b = 0;
	bytes = -index;
	debugf("bytes: %d, length: %d, bytes < length: %d\n", bytes, length, bytes < length);
	while (bytes < length) {
		debugf("get block %d\n", block);
		bh[b] = sb_getblk(sb, block);
		if (bh[b] == NULL) {
			errorf("sb_getblk failed\n");
			for (i = 0; i < b; i++) {
				put_bh(bh[i]);
			}
			kfree(bh);
			kfree(data);
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
	return length;
}

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

static int node_fill (struct super_block *sb, long long number, struct node *node)
{
	int rc;
	struct bitbuffer bb;
	struct smashfs_super_info *sbi;
	enterf();
	sbi = sb->s_fs_info;
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
	debugf("node\n");
	debugf("  number: %lld\n", node->number);
	debugf("  type  : %lld\n", node->type);
	debugf("  size  : %lld\n", node->size);
	leavef();
	return 0;
}

static int smashfs_readdir (struct file *filp, void *dirent, filldir_t filldir)
{
	enterf();
	leavef();
	return 0;
}

static struct dentry * smashfs_lookup (struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
	enterf();
	leavef();
	return ERR_PTR(-EINVAL);
}

static const struct file_operations smashfs_directory_operations = {
	.llseek  = generic_file_llseek,
	.read    = generic_read_dir,
	.readdir = smashfs_readdir,
};

static const struct inode_operations smashfs_dir_inode_operations = {
	.lookup = smashfs_lookup,
};

static struct inode * smashfs_get_inode (struct super_block *sb, long long number)
{
	int rc;
	struct node node;
	struct inode *inode;
	struct smashfs_super_info *sbi;
	enterf();
	sbi = sb->s_fs_info;
	rc = node_fill(sb, number, &node);
	if (rc != 0) {
		errorf("node fill failed\n");
		leavef();
		return ERR_PTR(-ENOMEM);
	}
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
	if (node.type == smashfs_inode_type_directory) {
		inode->i_op  = &smashfs_dir_inode_operations;
		inode->i_fop = &smashfs_directory_operations;
	} else {
		errorf("unknown node type: %lld\n", node.type);
		unlock_new_inode(inode);
		leavef();
		return ERR_PTR(-ENOMEM);
	}
	inode->i_mode   = 0777;
	inode->i_uid    = 0;
	inode->i_gid    = 0;
	inode->i_size   = node.size;
	inode->i_blocks = ((node.size + sbi->devblksize - 1) >> sbi->devblksize_log2) + 1;
	inode->i_ctime.tv_sec = node.ctime;
	inode->i_mtime.tv_sec = node.mtime;
	inode->i_atime.tv_sec = node.mtime;
	unlock_new_inode(inode);
	leavef();
	return inode;
}

int smashfs_fill_super (struct super_block *sb, void *data, int silent)
{
	int rc;
	char b[BDEVNAME_SIZE];
	struct inode *root;
	struct smashfs_super_info *sbi;
	struct smashfs_super_block *sbl;
	enterf();
	sbi = kmalloc(sizeof(struct smashfs_super_info), GFP_KERNEL);
	if (sbi == NULL) {
		errorf("kalloc failed for super info\n");
		leavef();
		return -ENOMEM;
	}
	sb->s_fs_info = sbi;
	debugf("devname: %s\n", bdevname(sb->s_bdev, b));
	sbi->devblksize = sb_min_blocksize(sb, BLOCK_SIZE);
	sbi->devblksize_log2 = ffz(~sbi->devblksize);
	debugf("dev block size: %d, log2: %d", sbi->devblksize, sbi->devblksize_log2);
	sbl = kmalloc(sizeof(struct smashfs_super_block), GFP_KERNEL);
	if (sbl == NULL) {
		errorf("kalloc failed for super block\n");
		kfree(sbi);
		sb->s_fs_info = NULL;
		leavef();
		return -ENOMEM;
	}
	sbi->super = sbl;
	rc = smashfs_read(sb, sbl, SMASHFS_START, sizeof(struct smashfs_super_block));
	if (rc != sizeof(struct smashfs_super_block)) {
		errorf("could not read super block\n");
		kfree(sbl);
		kfree(sbi);
		sb->s_fs_info = NULL;
		leavef();
		return rc;
	}
	if (sbl->magic != SMASHFS_MAGIC) {
		errorf("magic mismatch\n");
		kfree(sbl);
		kfree(sbi);
		sb->s_fs_info = NULL;
		leavef();
		return -EINVAL;
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
	sbi->inodes_table = kmalloc(sbl->inodes_size, GFP_KERNEL);
	if (sbi->inodes_table == NULL) {
		errorf("kmalloc failed for inodes table\n");
		kfree(sbl);
		kfree(sbi);
		sb->s_fs_info = NULL;
		leavef();
		return -ENOMEM;
	}
	sbi->blocks_table = kmalloc(sbl->blocks_size, GFP_KERNEL);
	if (sbi->blocks_table == NULL) {
		errorf("kmalloc failed for blocks table\n");
		kfree(sbi->inodes_table);
		kfree(sbl);
		kfree(sbi);
		sb->s_fs_info = NULL;
		leavef();
		return -ENOMEM;
	}
	rc = smashfs_read(sb, sbi->inodes_table, sbl->inodes_offset, sbl->inodes_size);
	if (rc != sbl->inodes_size) {
		errorf("read failed for inodes table\n");
		kfree(sbi->blocks_table);
		kfree(sbi->inodes_table);
		kfree(sbl);
		kfree(sbi);
		sb->s_fs_info = NULL;
		leavef();
		return -EINVAL;
	}
	debugf("inodes table\n");
	debugf("  0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", sbi->inodes_table[0], sbi->inodes_table[1], sbi->inodes_table[2], sbi->inodes_table[3], sbi->inodes_table[4], sbi->inodes_table[5], sbi->inodes_table[6], sbi->inodes_table[7]);
	rc = smashfs_read(sb, sbi->blocks_table, sbl->blocks_offset, sbl->blocks_size);
	if (rc != sbl->blocks_size) {
		errorf("read failed for blocks table\n");
		kfree(sbi->blocks_table);
		kfree(sbi->inodes_table);
		kfree(sbl);
		kfree(sbi);
		sb->s_fs_info = NULL;
		leavef();
		return -EINVAL;
	}
	sb->s_magic = sbl->magic;
	sb->s_maxbytes = MAX_LFS_FILESIZE;
	sb->s_flags |= MS_RDONLY;
	sb->s_op = &smashfs_super_ops;
	root = smashfs_get_inode(sb, sbl->root);
	if (IS_ERR(root)) {
		errorf("can not get root inode\n");
		kfree(sbi->blocks_table);
		kfree(sbi->inodes_table);
		kfree(sbl);
		kfree(sbi);
		sb->s_fs_info = NULL;
		leavef();
		return -EINVAL;
	}
	sb->s_root = d_alloc_root(root);
	if (!sb->s_root) {
		errorf("d_alloc_root failed\n");
		iput(root);
		kfree(sbi->blocks_table);
		kfree(sbi->inodes_table);
		kfree(sbl);
		kfree(sbi);
		sb->s_fs_info = NULL;
		leavef();
		return -EINVAL;
	}
	kfree(sbl);
	leavef();
	return 0;
}
