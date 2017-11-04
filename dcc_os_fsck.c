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

unsigned int bitmapGet( char byte, unsigned int pos ) {
  return (byte >> pos) & 1;
}

// Tornar mais brando o fix do superblock
void superblock_fix( int fd ){
  struct ext2_super_block super;
  lseek(fd, BASE_OFFSET, SEEK_SET);
  read(fd, &super, sizeof(super));
  if( super.s_magic == 61267 ) {
    printf("Superblock OK\n");
    return;
  }
  
  lseek(fd, 1024*8193, SEEK_SET);
  read(fd, &super, sizeof(super));

  lseek(fd, BASE_OFFSET, SEEK_SET);
  write(fd, &super, sizeof(super));
  
}

static void read_inode( int fd, int inode_no, const struct ext2_group_desc * group, struct ext2_inode * inode) {
  unsigned int block_size = 1024;

  lseek(fd, BLOCK_OFFSET(group->bg_inode_table) + (inode_no)*sizeof(struct ext2_inode), SEEK_SET);
  read(fd, inode, sizeof(struct ext2_inode));
  
}

void multiple_inode( int fd ) {
  struct ext2_super_block super;

  lseek(fd, BASE_OFFSET, SEEK_SET);
  read(fd, &super, sizeof(super));

  unsigned int block_size = 1024 << super.s_log_block_size;

  unsigned int inodes_per_block = block_size / sizeof(struct ext2_inode);
  unsigned int itable_blocks    = super.s_inodes_per_group / inodes_per_block;

  unsigned int group_count      = 1 + (super.s_blocks_count-1) / super.s_blocks_per_group;

  struct ext2_group_desc group[group_count];
  lseek(fd, BASE_OFFSET + block_size, SEEK_SET);
  read(fd, group, sizeof(group));

  unsigned int * ref;
  struct ext2_inode inodes[inodes_per_block];
  // Caminho por cada BLOCK GROUP
  for(int n = 0; n < group_count; n++) {
    ref = calloc( sizeof(unsigned int) , 8192 );
    // Posicao no comeco da tabela de inodes
    lseek(fd, BLOCK_OFFSET(group[n].bg_inode_table), SEEK_SET);
    for(int i = 0; i < itable_blocks; i++) {
      // Leio 1 block de inodes
      read(fd, inodes, sizeof(inodes));
      for(int j = 0; j < inodes_per_block; j++) {
        unsigned int b_count    = inodes[j].i_blocks;
        unsigned int * blocks = inodes[j].i_block;
        // Se o inode tem algo
        if( b_count > 0 ) {
          for( int k = 0; k < 12; k++ ) {
            if( blocks[k] == 0 ) {
            }
            else if( ref[blocks[k]] == 0 ) {
              ref[blocks[k]]++;
            }
            // Bloco com mais de 1 ref
            else {
              printf("Bloco %d com mais de 1 ref\n",blocks[k]);
            }
          }
        }
      }
    }
    free(ref);
  }
  //// In bytes

  //unsigned int descr_list_size = group_count * sizeof(struct ext2_group_desc);

  //unsigned int inodes_per_block = block_size / sizeof(struct ext2_inode);
  //unsigned int itable_blocks = super.s_inodes_per_group / inodes_per_block;

  //lseek( fd, BASE_OFFSET + block_size, SEEK_SET);
  //read( fd, &group, sizeof(struct ext2_group_desc));

  //unsigned int inode_bitmap = group.bg_inode_bitmap;
  //unsigned int inode_table  = group.bg_inode_table;
  //
  //unsigned char *bitmap      = malloc(block_size);
  //unsigned char *block_count = calloc(block_size, sizeof(unsigned char));

  //lseek(fd, BLOCK_OFFSET(inode_bitmap), SEEK_SET);
  //read(fd, bitmap, sizeof(bitmap));

  //struct ext2_inode * inode = malloc( sizeof(struct ext2_inode) );
  //read_inode( fd, 1, &group, inode);


  //for(int i = 0; i < 1024; i++) {
  //  if( bitmap[i] > 0 ) {
  //    for(int j = 0; j < 8; j++) {

  //    }
  //  }
  //  printf("Bloco: %d\n", i);
  //}


}

int main(int agrc, const char * argv[]) {
  int fd;
  fd = open(argv[1], O_RDWR);

  superblock_fix( fd );

  multiple_inode( fd );

  return 0;
}
