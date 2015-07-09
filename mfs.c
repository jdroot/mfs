#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/version.h>
#include <linux/jbd2.h>
#include <linux/parser.h>
#include <linux/blkdev.h>

#include "superblock.h"

static int mfs_init(void);
static int mfs_exit(void);

module_init(mfs_init);
module_exit(mfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("James Root");

void mfs_destroy_inode(struct inode *inode) {
	printk(KERN_INFO "[mfs] mfs_destroy_inode called!\n");
}

static void mfs_put_super(struct super_block *sb) {
	printk(KERN_INFO "[mfs] mfs_put_super called!\n");
}

static const struct super_operations mfs_sops = {
	.destroy_inode = mfs_destroy_inode,
	.put_super = mfs_put_super,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
static int mfs_iterate(struct file *filp, struct dir_context *ctx)
#else
static int mfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
#endif
{

	loff_t pos;
	struct inode *inode;
	struct super_block *sb;
	struct buffer_head *bh;
	struct mfs_inode *mfs;
	struct mfs_dir_record *record;
	int i;

	printk(KERN_INFO "[mfs] mfs_readdir called!\n");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
	pos = ctx->pos;
#else
	pos = filp->f_pos;
#endif
	inode = filp->f_dentry->d_inode;
	sb = inode->i_sb;

	printk(KERN_INFO "[mfs] Got pos: %lu\n", pos);

	if (pos) {
		/* FIXME: We use a hack of reading pos to figure if we have filled in all data.
		 * We should probably fix this to work in a cursor based model and
		 * use the tokens correctly to not fill too many data in each cursor based call */
		return 0;
	}

	mfs = inode->i_private;

	printk(KERN_INFO "[mfs] Got MFS inode: %llu\n", mfs->inode_no);

	if (unlikely(!S_ISDIR(mfs->mode))) {
		printk(KERN_ERR "[mfs] inode err, expected dir!\n");
		return -ENOTDIR;
	}
	printk(KERN_INFO "[mfs] Block number for dirents: %llu\n", mfs->data_block_number);
	printk(KERN_INFO "[mfs] Children: %llu\n", mfs->dir_children_count);
	bh = sb_bread(sb, mfs->data_block_number);
	BUG_ON(!bh);

	printk(KERN_INFO "[mfs] calling emit_dots\n");
	dir_emit_dots(filp, ctx);


	record = (struct mfs_dir_record *) bh->b_data;

	printk(KERN_INFO "Got record: %s\n", record->filename);
	printk(KERN_INFO "Record inode: %llu\n", record->inode_no);

	for (i = 0; i < mfs->dir_children_count; i++) {
		printk(KERN_INFO "Looping on record: %d\n", i);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
		dir_emit(ctx, record->filename, MFS_FILENAME_MAXLEN, record->inode_no, DT_UNKNOWN);
		ctx->pos += sizeof(struct mfs_dir_record);
#else
		filldir(dirent, record->filename, MFS_FILENAME_MAXLEN, pos, record->inode_no, DT_UNKNOWN);
		filp->f_pos += sizeof(struct mfs_dir_record);
#endif		
		pos += sizeof(struct mfs_dir_record);
		record++;
	}
	
	brelse(bh);

	return 0;
}

const struct file_operations mfs_dir_operations = {
	.owner = THIS_MODULE,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
	.iterate = mfs_iterate,
#else
	.readdir = mfs_readdir,
#endif
};

// Forward declare this...
struct dentry *mfs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags);
static struct inode_operations mfs_inode_ops = {
	// Unneeded cause the file system is read only
	// .create = mfs_create,
	// .mkdir  = mfs_mkdir,
	.lookup = mfs_lookup
};

struct mfs_inode *mfs_get_inode(struct super_block *sb, uint64_t inode_no) {
	struct mfs_super_block *mfs_sb = sb->s_fs_info;
	struct mfs_inode *mfs_inode    = NULL;
	struct mfs_inode *rv           = NULL;
	struct buffer_head *bh         = NULL;
	
	//TODO: Calculate actual datablock based on inode_no and MFS_INODES_PER_BLOCK

	printk(KERN_INFO "[mfs] mfs_get_inode called for inode: [%llu]\n", inode_no);
	printk(KERN_INFO "[mfs] mfs looking for inode block at [%llu]\n", mfs_sb->inodes_block);
	bh = sb_bread(sb, mfs_sb->inodes_block);
	BUG_ON(!bh);

	mfs_inode = (struct mfs_inode *)bh->b_data;
	
	mfs_inode += inode_no - 1; // Inode should be perfectly indexed
	printk(KERN_INFO "[mfs] Found the right inode!\n");
	printk(KERN_INFO "[mfs] data_block_number = %llu\n", mfs_inode->data_block_number);
	rv = kmalloc(sizeof(*rv), GFP_KERNEL);
	memcpy(rv, mfs_inode, sizeof(*rv));


	// while (1) {
	// 	printk(KERN_INFO "[mfs] Checking inode: [%p]\n", mfs_inode);
	// 	if (mfs_inode->inode_no == inode_no) {
	// 		printk(KERN_INFO "[mfs] Found the right inode!\n");
	// 		printk(KERN_INFO "[mfs] data_block_number = %llu\n", mfs_inode->data_block_number);
	// 		rv = kmalloc(sizeof(*rv), GFP_KERNEL);
	// 		memcpy(rv, mfs_inode, sizeof(*rv));
	// 		break;
	// 	}
	// 	mfs_inode++;
	// }

	brelse(bh);
	return rv;
}

ssize_t mfs_read(struct file * filp, char __user * buf, size_t len, loff_t * ppos) {
	printk(KERN_INFO "[mfs] mfs_read called!\n");

	struct mfs_inode       *inode      = filp->f_path.dentry->d_inode->i_private;
	struct super_block     *sb         = filp->f_path.dentry->d_inode->i_sb;
	struct mfs_super_block *mfs_sb     = sb->s_fs_info;
	uint64_t                data_block = ((*ppos) / (mfs_sb->block_size)) + inode->data_block_number;
	char                   *buffer     = NULL;
	ssize_t                 bytes      = 0;

	struct buffer_head *bh;

	printk(KERN_INFO "[mfs] Using inode %llu\n", inode->inode_no);
	printk(KERN_INFO "[mfs] Reading from pos: %lld\n", *ppos);
	printk(KERN_INFO "[mfs] Inode root datablock: %llu\n", inode->data_block_number);
	printk(KERN_INFO "[mfs] Position mapped to datablock: %llu\n", data_block);

	if (*ppos >= inode->file_size) {
		return 0;
	}

	//Calculate the datablock based on pos


	bh = sb_bread(sb, data_block);

	if (!bh) {
		printk(KERN_ERR "[mfs] Error reading data block [%llu]\n", data_block);
		return 0;
	}

	buffer = (char *)bh->b_data;
	bytes  = min(inode->file_size, len);

	if (copy_to_user(buf, buffer, bytes)) {
		brelse(bh);
		printk(KERN_ERR "[mfs] Error copying file to user space\n");
		return -EFAULT;
	}

	brelse(bh);

	*ppos += bytes;
	return bytes;
}

const struct file_operations mfs_file_ops = {
	.read = mfs_read,
	// .write = mfs_write, // Not implemented in read-only
};

struct dentry *mfs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags) {
	printk(KERN_INFO "[mfs] mfs_lookup for %s\n", child_dentry->d_name.name);
	struct mfs_inode   *parent    = parent_inode->i_private;
	struct mfs_inode   *mfs       = NULL;
	struct inode       *inode     = NULL;
	struct super_block *sb        = parent_inode->i_sb;
	struct mfs_dir_record *record = NULL;
	struct buffer_head *bh        = NULL;

	int i = 0;

	bh = sb_bread(sb, parent->data_block_number);
	BUG_ON(!bh);

	record = (struct mfs_dir_record *) bh->b_data;

	for (i = 0; i < parent->dir_children_count; i++) {
		if (!strcmp(record->filename, child_dentry->d_name.name)) {
			mfs = mfs_get_inode(sb, record->inode_no);

			printk(KERN_INFO "[mfs] Got inode %llu\n", mfs->inode_no);

			inode = new_inode(sb);
			inode = new_inode(sb);
			inode->i_ino = mfs->inode_no;
			inode->i_sb = sb;
			inode->i_op = &mfs_inode_ops;

			if (S_ISDIR(mfs->mode))
				inode->i_fop = &mfs_dir_operations;
			else if (S_ISREG(mfs->mode))
				inode->i_fop = &mfs_file_ops;

			inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
			inode->i_private = mfs;

			inode_init_owner(inode, parent_inode, mfs->mode);

			d_add(child_dentry, inode);
			return NULL;
		}
		record ++;
		//TODO: Check for rolling over block size when record == MFS_RECORDS_PER_BLOCK
	}

	return NULL;
}

char * print_permissions(umode_t mode) {
	char * buf = kmalloc(11 * sizeof(char), GFP_KERNEL);
	snprintf(buf, 11, "%s%s%s%s%s%s%s%s%s%s",
		S_ISDIR(mode) ? "d" : "-",
		(mode & S_IRUSR) ? "r" : "-",
	    (mode & S_IWUSR) ? "w" : "-",
	    (mode & S_IXUSR) ? "x" : "-",
	    (mode & S_IRGRP) ? "r" : "-",
	    (mode & S_IWGRP) ? "w" : "-",
	    (mode & S_IXGRP) ? "x" : "-",
	    (mode & S_IROTH) ? "r" : "-",
	    (mode & S_IWOTH) ? "w" : "-",
	    (mode & S_IXOTH) ? "x" : "-");
	return buf;
}

int mfs_fill_superblock(struct super_block *sb, void *data, int silent) {
	int rv = -EPERM;
	struct inode *root_inode = NULL;
	struct buffer_head *bh = NULL;
	struct mfs_inode * root = NULL;
	struct mfs_super_block *mfs_sb = NULL;
	char * mode = NULL;

	printk(KERN_INFO "[mfs] mfs_fill_superblock\n");

	bh = sb_bread(sb, 0);
	BUG_ON(!bh);

	mfs_sb = (struct mfs_super_block *)bh->b_data;

	printk(KERN_INFO "[mfs] Loaded the superblock at address [%p]\n", mfs_sb);
	
	printk(KERN_INFO "[mfs] Verifying superblock magic: [%llu]\n", mfs_sb->magic);

	if (likely(mfs_sb->magic == MFS_MAGIC)) {
		printk(KERN_INFO "[mfs] Superblock magic matches.\n");
	} else {
		printk(KERN_ERR "[mfs] Failed to match superblock magic!\n");
		goto done;
	}

	printk(KERN_INFO "[mfs] MFS filesystem with version [%llu] and block size [%llu] detected.\n", mfs_sb->version, mfs_sb->block_size);

	sb->s_magic = mfs_sb->magic;

	/* For all practical purposes, we will be using this s_fs_info as the super block */
	sb->s_fs_info = mfs_sb;

	sb->s_maxbytes = mfs_sb->block_size;
	sb->s_op = &mfs_sops;

	printk(KERN_INFO "[mfs] Creating the inode\n");
	root_inode = new_inode(sb);
	root = mfs_get_inode(sb, MFS_ROOT_INODE_NUMBER);
	printk(KERN_INFO "[mfs] got inode!\n");
	printk(KERN_INFO "[mfs] inode_no = %llu\n", root->inode_no);
	printk(KERN_INFO "[mfs] data_block_number = %llu\n", root->data_block_number);
	printk(KERN_INFO "[mfs] inode created at [%p]\n", root_inode);
	root_inode->i_ino = MFS_ROOT_INODE_NUMBER;
	printk(KERN_INFO "[mfs] setting inode owner\n");
	mode = print_permissions(root->mode);
	printk(KERN_INFO "[mfs] Root permissions: %s\n", mode);
	printk(KERN_INFO "[mvs] Mode: %llu\n", root->mode);
	kfree(mode);
	if (S_ISDIR(root->mode))
		printk(KERN_INFO "[mfs] is dir");
	else
		printk(KERN_INFO "[mfs] not dir");
	inode_init_owner(root_inode, NULL, root->mode);

	mode = print_permissions(root_inode->i_mode);
	printk(KERN_INFO "[mfs] Root permissions: %s\n", mode);
	kfree(mode);

	root_inode->i_sb = sb;
	root_inode->i_size = sizeof(*root);
	root_inode->i_op = &mfs_inode_ops;
	root_inode->i_fop = &mfs_dir_operations; //TODO
	// root_inode->i_fop = &simplefs_dir_operations;
	root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = CURRENT_TIME;
	root_inode->i_private = root;

	printk(KERN_INFO "[mfs] calling d_make_root or d_alloc_root\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)
	sb->s_root = d_make_root(root_inode);
#else
	sb->s_root = d_alloc_root(root_inode);
	if (!sb->s_root) {
		iput(root_inode);
	}
#endif
	printk(KERN_INFO "[mfs] done with make root\n");

	if (!sb->s_root) {
		printk(KERN_INFO "[mfs] failed to make root inode!\n");
		rv = -ENOMEM;
		goto done;
	}

	// if ((ret = simplefs_parse_options(sb, data)))
	// 	goto release;
	
	rv = 0;
	
done:
	brelse(bh);

	printk(KERN_INFO "[mfs] mfs_fill_superblock done\n");

	return rv;
}

static struct dentry *mfs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {
	struct dentry *rv = NULL;

	// Only allow read only mounting
	if (!(flags & MS_RDONLY))
		return ERR_PTR(-EACCES);

	rv = mount_bdev(fs_type, flags, dev_name, data, mfs_fill_superblock);

	if (unlikely(IS_ERR(rv)))
		printk(KERN_ERR "[mfs] Error mounting mfs: %s", dev_name);
	else
		printk(KERN_INFO "[mfs] Succesfully mounted %s\n", dev_name);

	return rv;
}

// TODO: Implement this
static void mfs_kill_superblock(struct super_block *sb) {
	
}

struct file_system_type mfs_fs_type = {
	.owner = THIS_MODULE,
	.name = "mfs",
	.mount = mfs_mount,
	.kill_sb = mfs_kill_superblock,
	.fs_flags = FS_REQUIRES_DEV
};

static int mfs_init(void) {
	int rv = 0;

	rv = register_filesystem(&mfs_fs_type);
	if (likely(rv == 0))
		printk(KERN_INFO "[mfs] Sucessfully registered mfs\n");
	else {
		printk(KERN_ERR "[mfs] Failed to register mfs\n");
	}

	return rv;
}

static int mfs_exit(void) {
	int rv = 0;

	rv = unregister_filesystem(&mfs_fs_type);
	if (likely(rv == 0))
		printk(KERN_INFO "[mfs] Sucessfully unregistered mfs\n");
	else {
		printk(KERN_ERR "[mfs] Failed to unregister mfs\n");
	}

	return rv;
}