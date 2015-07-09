#ifndef __MFS_INCLUDE_SUPER_BLOCK_H__
#define __MFS_INCLUDE_SUPER_BLOCK_H__

#include <linux/types.h>

#define MFS_VERSION            1
#define MFS_MAGIC              0xDEADBEEF
#define MFS_FILENAME_MAXLEN    63
#define MFS_ROOT_INODE_NUMBER  1

#define MFS_SUPERBLOCK_SIZE (uint64_t)sizeof(struct mfs_super_block)
#define MFS_SUPERBLOCK_PADDING(bs) bs - MFS_SUPERBLOCK_SIZE
#define MFS_INODE_SIZE (uint64_t)sizeof(struct mfs_inode)
#define MFS_INODES_PER_BLOCK(bs) bs / MFS_INODE_SIZE
#define MFS_INODE_BLOCK_PADDING(bs) bs - (MFS_INODES_PER_BLOCK(bs) * MFS_INODE_SIZE)

#define MFS_RECORD_SIZE (uint64_t)sizeof(struct mfs_dir_record)
#define MFS_RECORDS_PER_BLOCK(bs) (bs / MFS_RECORD_SIZE)
#define MFS_RECORDS_PADDING(bs) (bs - (MFS_RECORDS_PER_BLOCK(bs) * MFS_RECORD_SIZE))

#define _MFS_RAW_BLOCKS_NEEDED(bs, c) ((c) / (bs))
#define _MFS_MOD_BLOCKS_NEEDED(bs, c) (((c) % (bs)) > 0 ? 1 : 0)
#define MFS_BLOCKS_NEEDED(bs, c) (_MFS_RAW_BLOCKS_NEEDED(bs, c) + _MFS_MOD_BLOCKS_NEEDED(bs, c))

#define MFS_RECORD_BLOCKS_NEEDED(bs, rc) (MFS_BLOCKS_NEEDED(bs, (rc * MFS_RECORD_SIZE)))
#define MFS_INODE_BLOCKS_NEEDED(bs, ic) (MFS_BLOCKS_NEEDED(bs, (ic * MFS_INODE_SIZE)))

#define MFS_INODE_BLOCK(bs, i) (i / MFS_INODES_PER_BLOCK(bs)) + 1
#define MFS_INODE_OFFSET(bs, i) i % (MFS_INODES_PER_BLOCK(bs))
#define MFS_PERMISSIONS      S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH
#define MFS_FILE_PERMISSIONS S_IFREG | MFS_PERMISSIONS
#define MFS_DIR_PERMISSIONS S_IFDIR | MFS_PERMISSIONS

struct mfs_super_block {
    uint64_t version;
    uint64_t magic;
    uint64_t block_size;
    uint64_t inodes_block;
};


struct mfs_inode {
	mode_t   mode;
	uint64_t inode_no;
	uint64_t data_block_number;
	union {
		uint64_t file_size;
		uint64_t dir_children_count;
	};
};

struct mfs_dir_record {
    // Add one for \0
    char filename[MFS_FILENAME_MAXLEN + 1];
    uint64_t inode_no;
};

// #define INODE_SIZE sizeof(struct mfs_inode)
// #define INODE_RAW_SIZE(count) (count * INODE_SIZE)
// #define _COUNT_RAW_BLOCKS(size) (size >> 12)
// #define _COUNT_MOD_BLOCKS(size) ((size % MFS_BLOCK_SIZE) > 0 ? 1 : 0)
// #define COUNT_BLOCKS(size) _COUNT_RAW_BLOCKS(size) + _COUNT_MOD_BLOCKS(size)

// #define INODES_PER_BLOCK (MFS_BLOCK_SIZE / INODE_SIZE)
// #define INODE_BLOCK_PADDING MFS_BLOCK_SIZE - (INODES_PER_BLOCK * INODE_SIZE)

// #define DIR_RECORD_SIZE sizeof(struct mfs_dir_record)
// #define DIRS_PER_BLOCK (MFS_BLOCK_SIZE / DIR_RECORD_SIZE)

#endif