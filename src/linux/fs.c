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

#include "super.h"

static struct dentry * smashfs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data)
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
	rc = register_filesystem(&smashfs_fs_type);
	printk(KERN_INFO "smashfs: (c) 2013 Alper Akcan\n");
	return rc;
}

static void __exit exit_smashfs_fs (void)
{
	unregister_filesystem(&smashfs_fs_type);
}

module_init(init_smashfs_fs)
module_exit(exit_smashfs_fs)
MODULE_LICENSE("GPL");
