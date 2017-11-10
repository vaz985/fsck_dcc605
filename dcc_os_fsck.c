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
  }else{
    char option;
    printf("Superblock corrompido. Reparar? (y/n):");
    scanf("%c", &option);
    if(option == 'y'){
      lseek(fd, 1024*8193, SEEK_SET);
      read(fd, &super, sizeof(super));

      lseek(fd, BASE_OFFSET, SEEK_SET);
      write(fd, &super, sizeof(super));
      printf("Superblock reparado com sucesso.\n");
    }
  }

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
  // Superbloco
  struct ext2_super_block super;
  lseek(fd, BASE_OFFSET, SEEK_SET);
  read(fd, &super, sizeof(super));

  unsigned int block_size = 1024 << super.s_log_block_size;
  unsigned int inodes_per_block = block_size / sizeof(struct ext2_inode);
  unsigned int itable_blocks    = super.s_inodes_per_group / inodes_per_block;
  unsigned int group_count      = 1 + (super.s_blocks_count-1) / super.s_blocks_per_group;

  // Group descriptor
  struct ext2_group_desc group[group_count];
  lseek(fd, BASE_OFFSET + block_size, SEEK_SET);
  read(fd, group, sizeof(group));

  struct ext2_inode inodes[inodes_per_block];

  //Bitmaps
  unsigned char *i_bmap;
  unsigned char *d_bmap;
  i_bmap = malloc(block_size);
  d_bmap = malloc(block_size);
  lseek(fd, BLOCK_OFFSET(group[0].bg_block_bitmap), SEEK_SET);
  read(fd, d_bmap, block_size);
  read(fd, i_bmap, block_size);

  // Caminho por cada BLOCK GROUP
  for(int n = 0; n < group_count; n++) {
    // Posicao no comeco da tabela de inodes
    lseek(fd, BLOCK_OFFSET(group[n].bg_inode_table), SEEK_SET);
    for(int i = 0; i < itable_blocks; i++) {
      // Leio 1 block de inodes
      lseek(fd, BLOCK_OFFSET(group[n].bg_inode_table+i), SEEK_SET);
      read(fd, inodes, sizeof(inodes));
      for(int j = 0; j < inodes_per_block; j++) {
        unsigned int b_count    = inodes[j].i_blocks;
        unsigned int * blocks   = inodes[j].i_block;
        // Se o inode tem algo
        if( b_count > 0 ) {
        //if(S_ISREG(inodes[j].i_mode) || S_ISDIR(inodes[j].i_mode)){
            // Se não existe nenuma permissão
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
                // Descobrir como seta a permissão
                printf("Permissão não encontrada para inode %d. ", i*8 + j + 1);
                inodes[j].i_mode = 0x8100;
                if(inodes[j].i_mode & S_IRUSR)
                    printf("Permissão atualizada - S_IRUSR (user read)\n");
                lseek(fd, BLOCK_OFFSET(group[n].bg_inode_table + i) + ((j)*sizeof(struct ext2_inode)), SEEK_SET);
                write(fd, &inodes[j], sizeof(struct ext2_inode));
            }
        }
      }
    }
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
