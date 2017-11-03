#include <stdlib.h>
#include <stdio.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <unistd.h>
#include <fcntl.h>

#include <linux/fs.h> 
#include "ext2.h"

#define BASE_OFFSET 1024
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block-1)*block_size)


// Tornar mais brando o fix do superblock
void superblock_fix( int fd ){
  struct ext2_super_block super;
  lseek(fd, BASE_OFFSET, SEEK_SET);
  read(fd, &super, sizeof(super));
  if( super.s_magic == 61267 ) {
    printf("Superblock OK\n");
    return;
  }
  
  lseek(fd, BASE_OFFSET*8193, SEEK_SET);
  read(fd, &super, sizeof(super));

  lseek(fd, BASE_OFFSET, SEEK_SET);
  write(fd, &super, sizeof(super));
  
}

void multiple_inode( int fd ) {
  struct ext2_super_block super;
  struct ext2_group_desc group_descr;


  lseek(fd, BASE_OFFSET, sizeof(super));
  read(fd, &super, sizeof(super));

  int block_size = super.s_blocksize;

  unsigned int group_count = 1 + (super.s_blocks_count-1) / super.s_blocks_per_group;
  unsigned int inodes_per_block = block_size / sizeof(struct ext2_inode);
  unsigned int itable_blocks = super.s_inodes_per_group / inodes_per_block;

  for( int group_i = 0; group_i < group_count; group_i++) {
    lseek( fd, BASE_OFFSET + group_i*super.s_blocks_per_group, SEEK_SET);
    for( int block_i = 0; block_i < (block_size-2)/3; block_i++){ 
      read(fd, &group_descr, sizeof(group_descr));

       
    } 
  }
}

int main(int agrc, const char * argv[]) {
  int fd;
  fd = open(argv[1], O_RDWR);

  superblock_fix( fd );

  multiple_inode( fd );

  return 0;
}
