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
#define BLOCK_OFFSET(block) (BASE_OFFSERT + (block-1)*block_size)

struct ext2_super_block sb;

// Tornar mais brando o fix do superblock
void superblock_fix( int fd ){
  
  lseek(fd, BASE_OFFSET, SEEK_SET);
  read(fd, &sb, sizeof(sb));
  if( sb.s_magic == 61267 ) {
    printf("Superblock OK\n");
    return;
  }
  
  lseek(fd, BASE_OFFSET*8193, SEEK_SET);
  read(fd, &sb, sizeof(sb));

  lseek(fd, BASE_OFFSET, SEEK_SET);
  write(fd, &sb, sizeof(sb));
  
}

int main(int agrc, const char * argv[]) {
  int fd;
  fd = open(argv[1], O_RDWR);

  superblock_fix( fd );

  return 0;
}
