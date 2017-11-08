#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <unistd.h>
#include <fcntl.h>

#include <linux/fs.h> 
#include "ext2.h"

#define BASE_OFFSET 1024
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block-1)*block_size)

uint block_size = 0;

uint bitmapGet( char byte, uint pos ) {
  return (byte >> pos) & 1;
}

uint bitmapSwap( char byte, uint pos) {
  return (byte ^ (1 << pos) );
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

struct group_block{
  struct ext2_super_block * super;
  struct ext2_group_desc * group_desc;
  unsigned char * d_bmap;
  unsigned char * i_bmap;
  struct ext2_inode * inodes;
  void * data_start;
  void * d;
};


struct group_block * setup_group_block (int fd, uint pos, uint n_group) {
  struct group_block * gb;
  gb = malloc( sizeof(struct group_block) );
  gb->super = malloc( sizeof(struct ext2_super_block) );
  lseek( fd, pos, SEEK_SET);
  read(fd, gb->super, sizeof(struct ext2_super_block));
  
  uint group_count = (gb->super->s_blocks_count - 1) / gb->super->s_blocks_per_group;

  gb->d = malloc( block_size*gb->super->s_blocks_per_group );
  lseek( fd, pos, SEEK_SET);
  read( fd, gb->d, block_size*gb->super->s_blocks_per_group);
  gb->super      = gb->d;
  gb->group_desc = gb->d + block_size;
  gb->d_bmap     = gb->d + gb->group_desc[n_group].bg_block_bitmap*block_size;
  gb->i_bmap     = gb->d + gb->group_desc[n_group].bg_inode_bitmap*block_size;
  gb->inodes     = gb->d + gb->group_desc[n_group].bg_inode_table *block_size;
  gb->data_start = gb->inodes + gb->super->s_inodes_per_group*(sizeof(struct ext2_inode)/block_size);

  return gb;
}

void new_fun( int fd ) {
  struct ext2_super_block super;

  lseek(fd, BASE_OFFSET, SEEK_SET);
  read(fd, &super, sizeof(struct ext2_super_block));

  uint group_count = 1 + (super.s_blocks_count-1) / super.s_blocks_per_group;
  uint * group_pos = malloc( sizeof(uint) * group_count );
  uint n_blocks = super.s_blocks_per_group;
  for(uint i = 0; i < group_count; i++)
    group_pos[i] = 1 + (i*n_blocks);

  block_size = 1024 << super.s_log_block_size;
  
  struct group_block * g_block;
  g_block = setup_group_block( fd, 1024, 0 );

}

void orphan_inodes( int fd ) {
  struct ext2_super_block super;

  lseek(fd, BASE_OFFSET, SEEK_SET);
  read(fd, &super, sizeof(struct ext2_super_block));

  block_size = 1024 << super.s_log_block_size;
  
  struct group_block * g_block;
  g_block = setup_group_block( fd, 1024, 0 );

  uint inodes_per_block = block_size / sizeof(struct ext2_inode);
  uint itable_blocks    = super.s_inodes_per_group / inodes_per_block;
  uint group_count      = 1 + (super.s_blocks_count-1) / super.s_blocks_per_group;

  struct ext2_group_desc group[group_count];
  lseek(fd, BASE_OFFSET + block_size, SEEK_SET);
  read(fd, group, group_count*sizeof(struct ext2_group_desc));

  struct ext2_inode lost_found;
  uint lost_found_pos = BLOCK_OFFSET(group[0].bg_inode_table) + 10*sizeof(struct ext2_inode);
  lseek(fd, lost_found_pos, SEEK_SET);
  read(fd, &lost_found, sizeof(struct ext2_inode));

  uint dir[ super.s_inodes_per_group * group_count ];
  uint dir_count = 0;
  char block[block_size];
  struct ext2_dir_entry_2 * entry;
  struct ext2_inode inode;
  unsigned char inode_count_bmap[block_size];
  for(int n = 0; n < group_count; n++) {
    for(int i = 0; i < block_size; i++)
      inode_count_bmap[i] = 0;
    for(int i = 0; i < inodes_per_block*itable_blocks; i++){
      // Lost and found
      if ( i == 10 ) 
        continue;
      lseek( fd, BLOCK_OFFSET(group[n].bg_inode_table) + i*sizeof(struct ext2_inode), SEEK_SET );
      read( fd, &inode, sizeof(struct ext2_inode) );
      // EH DIR
      if ( S_ISDIR( inode.i_mode ) && inode.i_blocks > 0) {
        printf("DIR Inode: %d\n", i+1);
        uint n_b    = inode.i_blocks;
        uint * blocks = inode.i_block;
        for( int b_n = 0; b_n < n_b; b_n++ ){
          if( blocks[b_n] > 0 ) {
            lseek( fd, BLOCK_OFFSET( blocks[b_n] ), SEEK_SET );
            read( fd, block, block_size );
            entry = (struct ext2_dir_entry_2 *) block;
            uint offset = 0;
            while(offset < inode.i_size) {
              printf("Inode: %4.d, Size: %4.d, Type: %d, Name: %s\n", entry->inode, entry->rec_len, entry->file_type, entry->name); 
              offset += entry->rec_len;
              entry = (void *) entry + entry->rec_len;
            }
          }
        }
      }
    }
  }
}

int inode_type( struct ext2_inode i) {
  if( S_ISREG( i.i_mode ) ) {
    printf("Regular file\n");
  }
  else if( S_ISDIR( i.i_mode ) ) {
    printf("Directory\n");
  }
  else if( S_ISCHR( i.i_mode ) ) {
    printf("Character Device\n");
  }
  else if( S_ISBLK( i.i_mode ) ) {
    printf("Block Device\n");
  }
  else if( S_ISFIFO( i.i_mode ) ) {
    printf("Fifo\n");
  }
  else if( S_ISSOCK( i.i_mode ) ) {
    printf("Socket\n");
  }
  else if( S_ISLNK( i.i_mode ) ) {
    printf("Symbolic Link\n");
  }
  else {
    printf("Inode sem tipo\n");
    return 0;
  }
  return 1;
}

void multiple_inode( int fd ) {
  struct ext2_super_block super;

  lseek(fd, BASE_OFFSET, SEEK_SET);
  read(fd, &super, sizeof(super));

  block_size = 1024 << super.s_log_block_size;

  uint inodes_per_block = block_size / sizeof(struct ext2_inode);
  uint itable_blocks    = super.s_inodes_per_group / inodes_per_block;

  uint group_count      = 1 + (super.s_blocks_count-1) / super.s_blocks_per_group;

  struct ext2_group_desc group[group_count];
  lseek(fd, BASE_OFFSET + block_size, SEEK_SET);
  read(fd, group, sizeof(group));

  unsigned char *i_bmap;
  unsigned char *d_bmap;
  i_bmap = malloc(block_size);
  d_bmap = malloc(block_size);

  lseek(fd, BLOCK_OFFSET(group[0].bg_block_bitmap), SEEK_SET);

  read(fd, d_bmap, block_size);
  read(fd, i_bmap, block_size);

  struct ext2_inode inodes[inodes_per_block];

  unsigned char * ref;
  uint inode_count = 0;
  // Caminho por cada BLOCK GROUP
  for(int n = 0; n < group_count; n++) {
    ref = calloc( sizeof(unsigned char) , block_size );
    // Posicao no comeco da tabela de inodes
    lseek(fd, BLOCK_OFFSET(group[n].bg_inode_table), SEEK_SET);
    for(int i = 0; i < itable_blocks; i++) {
      // Leio 1 block de inodes
      read(fd, inodes, sizeof(inodes));
      //aux( group[n].bg_inode_table+i, inodes, inodes_per_block, ref, i_bmap );
      for(int j = 0; j < inodes_per_block; j++) {
        inode_count++;
        uint b_count    = inodes[j].i_blocks;
        uint * blocks   = inodes[j].i_block;
        // Se o inode tem algo
        if( b_count > 0 ) {
          printf("Inode: %d\n", inode_count);
          //if( inode_type( inodes[j] ) == 0 ) {
          //  printf("Recuperar tipo\n");
          //  printf("Regular File = 1\n");
          //  printf("Dir = 2\n");
          //  uint choice;
          //  scanf("%d",&choice);
          
          //}
          //printf("Inode bmap: %d\n", bitmapGet( i_bmap[i], blocks[j] ) );
          //printf("Usa os seguintes blocos\n");
          for( int k = 0; k < 12; k++ ) {
            if( blocks[k] == 0 ) {
              continue;
            }
            else if( ref[blocks[k]] == 0 ) {
              ref[blocks[k]]++;
              printf("Block %d\n",blocks[k]);
              //printf("Block %d is on: %d\n",blocks[k], bitmapGet( d_bmap[blocks[k]/8], blocks[k] % 8 ) );
            }
            // Bloco com mais de 1 ref
            // Remover inode
            else {
              struct ext2_inode * zero = malloc( sizeof(struct ext2_inode) );
              memset(zero, 0, sizeof(struct ext2_inode));
              printf("Bloco %d com mais de 1 ref\n",blocks[k]);
              lseek(fd, BLOCK_OFFSET(group[n].bg_inode_table) + i*1024 + j*128 , SEEK_SET);
              write( fd, zero, sizeof(struct ext2_inode) );
              lseek(fd, BLOCK_OFFSET(group[n].bg_inode_table) + (i+1)*1024 , SEEK_SET);
              printf("Inode removido\n");
            }
            printf("\n");
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

  superblock_fix( fd );

  //multiple_inode( fd );
  new_fun( fd );
  //orphan_inodes( fd );

  return 0;
}
