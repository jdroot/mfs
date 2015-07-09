mfs is a very simple read-only file system implementation for Linux based on https://github.com/psankar/simplefs

This file system is meant as a learning tool for the reading half of Linux's virtual file system as well as some very basic file system design.

Currently it is licensed under the MIT license. If anyone would like a different license, please contact me.

Building/Running
----------------
Assuming the linux kernel sources are installed, you should be able to just run make.

Once build, you can use `insmod mfs.ko` to install the file system.

You can use the following to build a file system: `mkfs.mfs <input directory> <output file>`

Once installed, you can mount with `mount -o loop -t mfs <mkfs.mfs output file> <mount point>`

Features
--------
- Any block size large enough to fit the superblock (32 bytes) should work. Currently it is hard coded to 4096 bytes however, and it has only been tested with that size.
- Max file length and max file/directory count are 2^64 (untested)
- Max file system size is block size * 2^64 (untested)
- Zero disk fragmentation, but there could be wasted space depending on file size and block size

File System Layout
------------------

Block 0:       32 Byte Superblock + (Block Size - 32) bytes of padding
Block 1-N:     Data blocks containing file data and directory listings
Block (N+1)-M: Inode store

All files are stored sequentially (zero fragmentation). This means that a file an exist with a single inode and based on the initial data block, block size, and read position it is possible to move to any position in the file.

All directory listings are stored sequentially.

All inodes are stored sequentially, allowing O(1) lookup of any inode by inode number.

Known Issues
------------
- There is a very limited amount of files per directory. mfs_lookup needs to be updated to support uint64_t files (mkfs.mfs should work fine however)
- Only directories and regular files are supported right now, there is no error handling for other files
- When moving past a simple test data structure, the file system fails to mount. I need to investigate this
- There are a TON of print statements currently to log nearly everything the file system is doing (for debugging)
- mkfs.mfs needs to be updated to better sanatize input and allow passing in of block size

Possible Issues
---------------
- Need to verify that mkfs.mfs works properly on larger directory structures and files
- The directory listings need to be iterated twice, once to get the listings and once to stat every file/folder. This should be changed, but I need to investigate if it is even possible (it should be in theory).

Contact
-------
If you have any questions or comments please contact me at jamesroot@gmail.com

Credits
-------
A lot of credit needs to go to psankar for his simplefs