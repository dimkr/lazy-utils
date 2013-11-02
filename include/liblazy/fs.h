#ifndef _FS_H_INCLUDED
#	define _FS_H_INCLUDED

#	include <stdint.h>
#	include <sys/types.h>
#	include <linux/magic.h>
#	include <liblazy/common.h>

typedef struct {
	char name[1 + STRLEN("squashfs")];
	uint64_t magic;
	size_t size;
	off_t offset;
} _fs_magic_t;

static const _fs_magic_t g_file_systems[] = {
	{
		"ext4",
		0xEF53, /* EXT4_SUPER_MAGIC, defined in magic.h */
		sizeof(uint16_t),
		1024 + /* the first superblock is at offset 1024 */ \
		56 /* the offset of s_magic inside struct ext2_super_block */
	},
	{
		"xfs",
		0x58465342, /* XFS_SB_MAGIC, defined in fs/xfs/xfs_sb.h */
		sizeof(uint32_t),
		0 /* xfs_sb_t begins with XFS_SB_MAGIC */
	},
	{
		"squashfs",
		SQUASHFS_MAGIC,
		sizeof(uint32_t),
		0 /* struct squashfs_super_block begins with SQUASHFS_MAGIC */
	}
};

const char *fs_guess(const char *path);

#endif