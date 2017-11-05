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

//static void read_inode( int fd, int inode_no, const ext2_group_desc * group, struct ext2_inode * inode) {
//  unsigned int block_size = 1024;
//
//  lseek(fd, BLOCK_OFFSET(group->bg_inode_table) + (inode_no)*sizeof(struct ext2_inode), SEEK_SET);
//  read(fd, inode, sizeof(struct ext2_inode));
//  
//}

void multiple_inode( int fd ) {
  struct ext2_super_block super;
  struct ext2_group_desc  group;
  struct ext2_group_desc  group2;

  lseek(fd, BASE_OFFSET, SEEK_SET);
  read(fd, &super, sizeof(super));

  // In bytes
  int block_size = 1024 << super.s_log_block_size;

  unsigned int group_count = 1 + (super.s_blocks_count-1) / super.s_blocks_per_group;
  unsigned int descr_list_size = group_count * sizeof(struct ext2_group_desc);

  unsigned int inodes_per_block = block_size / sizeof(struct ext2_inode);
  unsigned int itable_blocks = super.s_inodes_per_group / inodes_per_block;

  lseek( fd, BASE_OFFSET + block_size, SEEK_SET);
  read( fd, &group, sizeof(struct ext2_group_desc));

  unsigned int inode_bitmap = group.bg_inode_bitmap;
  unsigned int inode_table  = group.bg_inode_table;
  
  unsigned char *bitmap      = malloc(block_size);
  unsigned char *block_count = calloc(block_size, sizeof(unsigned char));

  lseek(fd, BLOCK_OFFSET(inode_bitmap), SEEK_SET);
  read(fd, bitmap, sizeof(bitmap));

  struct ext2_inode * inode = malloc( sizeof(struct ext2_inode) );
  //read_inode( fd, 1, &group, inode);

  //for(int i = 0; i < 1024; i++) {
  //  if( bitmap[i] > 0 ) {
  //    for(int j = 0; j < 8; j++) {

  //    }
  //  }
  //  printf("Bloco: %d\n", i);
  //}


}


void inode_permission( int fd ) {
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
    
      //-----
      //unsigned int inode_bitmap = group[n].bg_inode_bitmap;
      //unsigned int inode_table  = group[n].bg_inode_table;
      
      //unsigned char *bitmap      = malloc(block_size);
      //unsigned char *block_count = calloc(block_size, sizeof(unsigned char));
      //lseek(fd, BLOCK_OFFSET(inode_bitmap), SEEK_SET);
      //read(fd, bitmap, sizeof(bitmap));
      //-----

    // Posicao no comeco da tabela de inodes
    lseek(fd, BLOCK_OFFSET(group[n].bg_inode_table), SEEK_SET);
    for(int i = 0; i < itable_blocks; i++) {
      // Leio 1 block de inodes
      read(fd, inodes, sizeof(inodes));
      for(int j = 0; j < inodes_per_block; j++) {
        //if((S_ISREG(inodes[j].i_mode) || S_ISDIR(inodes[j].i_mode)) || bitmap[j] != 0){
        if(S_ISREG(inodes[j].i_mode) || S_ISDIR(inodes[j].i_mode)){
          printf("inode %d: ", i);
          // Se não existe nenuma permissão
          if (inodes[j].i_mode & S_IRUSR)
            printf("S_IRUSR\n");
          else if(inodes[j].i_mode & S_IWUSR)  
            printf("S_IWUSR\n");
          else if(inodes[j].i_mode & S_IWUSR)  
            printf("S_IWUSR\n");
          else if(inodes[j].i_mode & S_IXUSR)  
            printf("S_IXUSR\n");
          else if(inodes[j].i_mode & S_IRWXU)  
            printf("S_IRWXU\n");
          else if(inodes[j].i_mode & S_IRGRP)  
            printf("S_IRGRP\n");
          else if(inodes[j].i_mode & S_IXGRP)  
            printf("S_IXGRP\n");
          else if(inodes[j].i_mode & S_IRWXG)  
            printf("S_IRWXG\n");
          else if(inodes[j].i_mode & S_IROTH)  
            printf("S_IROTH\n");
          else if(inodes[j].i_mode & S_IWOTH)  
            printf("S_IWOTH\n");
          else if(inodes[j].i_mode & S_IXOTH)  
            printf("S_IXOTH\n");
          else if(inodes[j].i_mode & S_IRWXO)  
            printf("S_IRWXO\n");
          else
            printf("NONE\n");

          if ((inodes[j].i_mode & S_IRUSR
              || inodes[j].i_mode & S_IWUSR
              || inodes[j].i_mode & S_IXUSR
              || inodes[j].i_mode & S_IRWXU
              || inodes[j].i_mode & S_IRGRP
              || inodes[j].i_mode & S_IWGRP
              || inodes[j].i_mode & S_IXGRP
              || inodes[j].i_mode & S_IRWXG
              || inodes[j].i_mode & S_IROTH
              || inodes[j].i_mode & S_IWOTH
              || inodes[j].i_mode & S_IXOTH
              || inodes[j].i_mode & S_IRWXO
            ) == 0){
            printf("Permissão não encontrada.\n");
          }
        }
      }
    }
    free(ref);
  }
}

int main(int agrc, const char * argv[]) {
  int fd;
  fd = open(argv[1], O_RDWR);

  //superblock_fix( fd );

  //multiple_inode( fd );

  inode_permission( fd );

  return 0;
}
