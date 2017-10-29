#include <stdlib.h>
#include <stdio.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <unistd.h>
#include <fcntl.h>

#include <linux/fs.h> 
#include <ext2fs/ext2fs.h>

#define BASE_OFFSET 1024
#define BLOCK_OFFSET(block) (BASE_OFFSERT + (block-1)*block_size)

struct ext2_super_block sb;
struct ext2_group_desc group_descr;

int main(int agrc, const char * argv[]) {
  int fd;
  fd = open(argv[1], O_RDWR);

  // Read and decode the superblock
  lseek(fd, BASE_OFFSET, SEEK_SET);
  read(fd, &sb, sizeof(sb));
  /////////////////////////////////
  // Read and decode the group descriptor
  int group_size = BASE_OFFSET << sb.s_log_block_size;
  read(fd, &group_descr, SEEK_SET); 
  
  if( mount(argv[1], "/tmp", "ext2", MS_RDONLY, "")== -1 ) { 
    printf("erro no mount, recuperar superblock");
    
    exit(1);
  }

  return 0;
}
