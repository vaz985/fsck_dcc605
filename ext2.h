#include <linux/types.h>
struct ext2_super_block {
    __le32    s_inodes_count;        /* Inodes count */
    __le32    s_blocks_count;        /* Blocks count */
    __le32    s_r_blocks_count;    /* Reserved blocks count */
    __le32    s_free_blocks_count;    /* Free blocks count */
    __le32    s_free_inodes_count;    /* Free inodes count */
    __le32    s_first_data_block;    /* First Data Block */
    __le32    s_log_block_size;    /* Block size */
    __le32    s_log_frag_size;    /* Fragment size */
    __le32    s_blocks_per_group;    /* # Blocks per group */
    __le32    s_frags_per_group;    /* # Fragments per group */
    __le32    s_inodes_per_group;    /* # Inodes per group */
    __le32    s_mtime;        /* Mount time */
    __le32    s_wtime;        /* Write time */
    __le16    s_mnt_count;        /* Mount count */
    __le16    s_max_mnt_count;    /* Maximal mount count */
    __le16    s_magic;        /* Magic signature */
    __le16    s_state;        /* File system state */
    __le16    s_errors;        /* Behaviour when detecting errors */
    __le16    s_minor_rev_level;     /* minor revision level */
    __le32    s_lastcheck;        /* time of last check */
    __le32    s_checkinterval;    /* max. time between checks */
    __le32    s_creator_os;        /* OS */
    __le32    s_rev_level;        /* Revision level */
    __le16    s_def_resuid;        /* Default uid for reserved blocks */
    __le16    s_def_resgid;        /* Default gid for reserved blocks */
    __le32    s_first_ino;         /* First non-reserved inode */
    __le16   s_inode_size;         /* size of inode structure */
    __le16    s_block_group_nr;     /* block group # of this superblock */
    __le32    s_feature_compat;     /* compatible feature set */
    __le32    s_feature_incompat;     /* incompatible feature set */
    __le32    s_feature_ro_compat;     /* readonly-compatible feature set */
    __u8    s_uuid[16];        /* 128-bit uuid for volume */
    char    s_volume_name[16];     /* volume name */
    char    s_last_mounted[64];     /* directory where last mounted */
    __le32    s_algorithm_usage_bitmap; /* For compression */
    __u8    s_prealloc_blocks;    /* Nr of blocks to try to preallocate*/
    __u8    s_prealloc_dir_blocks;    /* Nr to preallocate for dirs */
    __u16    s_padding1;
    __u8    s_journal_uuid[16];    /* uuid of journal superblock */
    __u32    s_journal_inum;        /* inode number of journal file */
    __u32    s_journal_dev;        /* device number of journal file */
    __u32    s_last_orphan;        /* start of list of inodes to delete */
    __u32    s_hash_seed[4];        /* HTREE hash seed */
    __u8    s_def_hash_version;    /* Default hash version to use */
    __u8    s_reserved_char_pad;
    __u16    s_reserved_word_pad;
    __le32    s_default_mount_opts;
    __le32    s_first_meta_bg;     /* First metablock block group */
    __u32    s_reserved[190];    /* Padding to the end of the block */
};
