
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/buffer_head.h>
#include <linux/statfs.h>

#include "../include/smashfs.h"
#include "super.h"

static int smashfs_statfs (struct dentry *dentry, struct kstatfs *buf)
{
	u64 id;
	struct smashfs_super_info *sbi;

	sbi = dentry->d_sb->s_fs_info;
	id = huge_encode_dev(dentry->d_sb->s_bdev->bd_dev);

	buf->f_type = SMASHFS_MAGIC;
	buf->f_bsize = sbi->block_size;
	buf->f_blocks = ((sbi->bytes_used - 1) >> sbi->block_log2) + 1;
	buf->f_bfree = buf->f_bavail = 0;
	buf->f_files = sbi->inodes;
	buf->f_ffree = 0;
	buf->f_namelen = SMASHFS_NAME_LEN;
	buf->f_fsid.val[0] = (u32) id;
	buf->f_fsid.val[1] = (u32) (id >> 32);

	return 0;
}

static int smashfs_remount(struct super_block *sb, int *flags, char *data)
{
	*flags |= MS_RDONLY;
	return 0;
}


static void smashfs_put_super(struct super_block *sb)
{
	struct smashfs_super_info *sbi;
	if (sb->s_fs_info == NULL) {
		return;
	}
	sbi = sb->s_fs_info;
	sb->s_fs_info = NULL;
	kfree(sbi);
}

static const struct super_operations smashfs_super_ops = {
	.statfs        = smashfs_statfs,
	.put_super     = smashfs_put_super,
	.remount_fs    = smashfs_remount
};

static int smashfs_read (struct super_block *sb, void *buffer, unsigned int offset, unsigned int length)
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
		printk(KERN_ERR "kcalloc failed\n");
		return -ENOMEM;
	}
	for (i = 0; i < pages; i++, buffer += PAGE_CACHE_SIZE) {
		data[i] = buffer;
	}
	sbi = sb->s_fs_info;
	index = offset & ((1 << sbi->devblksize_log2) - 1);
	block = offset >> sbi->devblksize_log2;
	blocks = ((length + sbi->devblksize - 1) >> sbi->devblksize_log2) + 1;
	printk(KERN_INFO "smashfs: offset: %d, length: %d, pages: %d, block: %d, index: %d, blocks: %d\n", offset, length, pages, block, index, blocks);
	bh = kcalloc(blocks, sizeof(struct buffer_head *), GFP_KERNEL);
	if (bh == NULL) {
		printk(KERN_ERR "smashfs: kcalloc failed for buffer heads\n");
		return -ENOMEM;
	}
	b = 0;
	bytes = -index;
	while (bytes < length) {
		bh[b] = sb_getblk(sb, block);
		if (bh[b] == NULL) {
			printk(KERN_ERR "smashfs: sb_getblk failed\n");
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
			printk(KERN_ERR "smashfs: buffer_uptodate failed\n");
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

int smashfs_fill_super (struct super_block *sb, void *data, int silent)
{
	int rc;
	char b[BDEVNAME_SIZE];
	struct smashfs_super *sbl;
	struct smashfs_super_info *sbi;

	sbi = kmalloc(sizeof(struct smashfs_super_info), GFP_KERNEL);
	if (sbi == NULL) {
		printk(KERN_ERR "smashfs: kalloc failed for super info\n");
		return -ENOMEM;
	}
	sb->s_fs_info = sbi;

	printk(KERN_INFO "smashfs: devname: %s\n", bdevname(sb->s_bdev, b));

	sbi->devblksize = sb_min_blocksize(sb, BLOCK_SIZE);
	sbi->devblksize_log2 = ffz(~sbi->devblksize);

	printk(KERN_INFO "smashfs: dev block size: %d, log2: %d", sbi->devblksize, sbi->devblksize_log2);

	sbl = kmalloc(sizeof(struct smashfs_super_info), GFP_KERNEL);
	if (sbl == NULL) {
		printk(KERN_ERR "smashfs: kalloc failed for super block\n");
		kfree(sbi);
		sb->s_fs_info = NULL;
		return -ENOMEM;
	}

	rc = smashfs_read(sb, sbl, SMASHFS_START, sizeof(struct smashfs_super));
	if (rc != sizeof(struct smashfs_super)) {
		printk(KERN_ERR "smashfs: could not read super block\n");
		kfree(sbl);
		kfree(sbi);
		sb->s_fs_info = NULL;
		return rc;
	}

	if (sbl->magic != SMASHFS_MAGIC) {
		printk(KERN_ERR "smashfs: magic mismatch\n");
		kfree(sbl);
		kfree(sbi);
		sb->s_fs_info = NULL;
		return -EINVAL;
	}

	printk(KERN_INFO "smashfs: magic  : 0x%08x\n", sbl->magic);
	printk(KERN_INFO "smashfs: version: 0x%08x\n", sbl->version);

	sb->s_magic = sbl->magic;
	sb->s_maxbytes = MAX_LFS_FILESIZE;
	sb->s_flags |= MS_RDONLY;
	sb->s_op = &smashfs_super_ops;

	kfree(sbl);
	kfree(sbi);
	sb->s_fs_info = NULL;

	return -EIO;
}
