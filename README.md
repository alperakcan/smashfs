# smashfs #

smashfs is a read-only linux kernel filesystem with compression support.

1. <a href="#1-overview">overview</a>
2. <a href="#2-creating">creating</a>
3. <a href="#3-extracting">extracting</a>
4. <a href="#4-linux-kernel">linux-kernel</a>
4. <a href="#5-using">using</a>
6. <a href="#6-contact">contact</a>

## 1. overview ##

smashfs is a read-only filesystem with compression support.

supported compressions are:

* none
* gzip
* lzma
* lzo
* xz

a smashed filesystem has four main blocks

* super block

  stored as uncompressed, and holds the information about the filesystem.

* nodes table

  stored as compressed, and hold the information about filesystem items.

* blocks table

  stored as compressed, and holds the information about accessing data blocks.

* data blocks

  stored as compressed, and holds the actual data of filesystem items.

## 2. creating ##

a smashed filesystem is created with the tool <tt>mkfs.smashfs</tt>.

command line options:

* -s / --source

  source file or directory

* -o / --output

  output file

* -b / --block_size

  data block size, default is <tt>1048576</tt> bytes

* -d / --debug

  enable debugging, can be used repeatedly to increase debug level

* -j / --jobs

  enable and set job count for multi-threaded compressing to decrease filesystem creation time,
  default is <tt>8</tt>

* --no_group_mode

  disable group mode and use user mode to gain some space

* --no_other_mode

  disable other mode and use user mode to gain some space

* --no_uid

  disable uid and use 0 to gain some space

* --no_gid

  disable gui and use 0 to gain some space

* --no_ctime

  disable ctime and use filesystem creation time to gain some space

* --no_mtime

  disable mtime and use filesystem creation time to gain some space

* --no_padding

  disable padding to 4K to gain some space

* --no_duplicates

  disable duplicate file checking, will increase filesystem size.
  may be usefull for debugging purposes.

## 3. extracting ##

a smashed filesystem is extracted with the tool <tt>unfs.smashfs</tt>.

command line options:

* -s / --source

  source file or directory

* -o / --output

  output file

* -d / --debug

  enable debugging, can be used repeatedly to increase debug level

## 4. linux kernel ##

linux kernel has to be patched with <tt>smashfs-kernel.patch</tt>.

## 5. using ##

## 6. contact ##

if you are using the software and/or have any questions, suggestions, etc. please contact with me at alper.akcan@gmail.com
