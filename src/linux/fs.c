
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
	return rc;
}

static void __exit exit_smashfs_fs (void)
{
	unregister_filesystem(&smashfs_fs_type);
}

module_init(init_smashfs_fs)
module_exit(exit_smashfs_fs)
MODULE_LICENSE("LGPL");
