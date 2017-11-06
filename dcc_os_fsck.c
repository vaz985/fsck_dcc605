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

struct group_block {
  uint number;
  struct ext2_group_desc * group_desc;
  unsigned char *i_bmap;
  unsigned char *d_bmap;
  unsigned char ref[1024];
};

struct group_block * setup_group_block (int fd, uint n) {
  struct group_block * gb;
  gb = malloc(sizeof(struct group_block));
  gb->number = n;
  gb->group_desc = malloc(sizeof(struct group_block));
  gb->i_bmap = malloc(sizeof(block_size));
  gb->d_bmap = malloc(sizeof(block_size));

  lseek(fd, BASE_OFFSET + block_size + n*sizeof(struct ext2_group_desc), SEEK_SET);
  read(fd, gb->group_desc, sizeof(struct ext2_group_desc));
  lseek(fd, BLOCK_OFFSET( gb->group_desc->bg_block_bitmap ) + n*sizeof(struct ext2_group_desc), SEEK_SET);
  read(fd, gb->d_bmap, block_size);
  read(fd, gb->i_bmap, block_size);

  return gb;
}

void check_inodes( int fd, struct group_block * group, uint itable_blocks, uint block_n ) {
  struct ext2_inode inodes[ block_size/sizeof(struct ext2_inode) ];
  lseek( fd, BLOCK_OFFSET(group->group_desc->bg_inode_table) + block_n , SEEK_SET);
  read( fd, inodes, sizeof(inodes) );
  char * bitmap = group->i_bmap;
  for( uint i = 0; i < block_size/sizeof(struct ext2_inode); i++ ){
    int n_blocks = inodes[i].i_blocks;
    int * blocks = inodes[i].i_block;
    if( n_blocks > 0 ) {
      printf("Inode no: %d\n", (block_n + i + 1));
      //printf("N blocks: %d\n", n_blocks);
      for( uint block_i = 0; block_i < 12; block_i++ ) {
        if( blocks[block_i] > 0 ) {
          // Remove bloco/inode
          if( group->ref[blocks[block_i]] > 1 ) {
            char choice;
            printf(" Bloco repetido, remover o inode?<y/n>\n");
            scanf("%c",&choice);
            if( choice == 'y' ) {
              bitmapSwap( bitmap[block_n], i );
              lseek(fd, BLOCK_OFFSET( group->group_desc->bg_inode_bitmap ), SEEK_SET);
              write(fd, bitmap, sizeof(bitmap)); 
              lseek(fd, BLOCK_OFFSET( group->group_desc->bg_inode_table ) + (block_n+1)*block_size , SEEK_SET);
            }
            else {
              group->ref[blocks[block_i]]++;
            }
          }
          else
            group->ref[blocks[block_i]]++;
        }
      }
    }
  }
}

void orphan_inodes( int fd ) {
  struct ext2_super_block super;
  lseek(fd, BASE_OFFSET, SEEK_SET);
  read(fd, &super, sizeof(struct ext2_super_block));

  block_size = 1024 << super.s_log_block_size;
  
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
  struct ext2_inode inode;
  for(int n = 0; n < group_count; n++) {
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
            struct ext2_dir_entry_2 dir;
            lseek( fd, BLOCK_OFFSET( blocks[b_n] ), SEEK_SET );
            read( fd, &dir, sizeof(struct ext2_dir_entry_2) );
            
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

  orphan_inodes( fd );

  return 0;
}
