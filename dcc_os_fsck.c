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

unsigned char bitmapSwap( unsigned char byte, uint pos) {
  return ( byte ^ ( 1 << pos ) );
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

void fix_bad_type( int fd, struct ext2_group_desc * groups, struct ext2_inode * inode, struct ext2_dir_entry_2 * dir ) {
  uint type = dir->file_type;
  if( type == 1 ) {
    inode->i_mode = ( ( inode->i_mode & 0170000 ) | 0100000 );
  }
  else if( type == 2 ) {
    inode->i_mode = ( ( inode->i_mode & 0170000 ) | 0040000 );
  }
}

uint all_zero_blocks( uint * blocks ) {
  for( uint i = 0; i < 15; i++ )
    if( blocks[i] > 0 )
      return 0;
  return 1;
}

void run( int fd, struct ext2_super_block * super, struct ext2_group_desc * group, struct ext2_inode * root, unsigned char * inode_bmap, unsigned char * data_bmap) 
{
  uint inodes_per_block = block_size / sizeof(struct ext2_inode);
  unsigned char * block = malloc( block_size );
  struct ext2_dir_entry_2 * entry;
  struct ext2_inode * inode = malloc(sizeof(struct ext2_inode));

  printf("-------------DIR------------\n");
  if( all_zero_blocks( root->i_block ) )
  {
    printf("---------EMPTY DIR----------\n");
    printf("----------------------------\n");
    return;
  }
  for( int i=0; i<12; i++ )
  {
    if( root->i_block[i] == 0 )
      continue;
    // DUPLICATE BLOCK
    uint inode_block = root->i_block[i] / inodes_per_block;  
    uint offset      = root->i_block[i] % inodes_per_block;
    // Duplicado
    if( bitmapGet(data_bmap[ inode_block ], offset ) ){
      printf("---Bloco_Duplicado---\n");
      return;
    }
    else {
      data_bmap[inode_block] = bitmapSwap( data_bmap[ inode_block ], offset );
    }
    //////////////////
    lseek(fd, BLOCK_OFFSET( root->i_block[i] ), SEEK_SET);
    read(fd, block, block_size);
    entry = (void *) block;
    offset = 0;
    while( offset < root->i_size ) 
    {
      char fname[255];
      memcpy(fname, entry->name, entry->name_len);
      fname[entry->name_len] = 0;
      printf("Inode: %4.d, Size: %4.d, Type: %d, Name: %s\n", entry->inode, entry->rec_len, entry->file_type, fname); 
      if( fname[0] != '.' && entry->file_type == 2 )
      {
        uint group_id = (entry->inode - 1)/(super->s_inodes_per_group);
        uint inode_offset = ((entry->inode - 1) % super->s_inodes_per_group)*sizeof(struct ext2_inode);
        lseek( fd, BLOCK_OFFSET(group[group_id].bg_inode_table) + inode_offset, SEEK_SET ); 
        read( fd, inode, sizeof(struct ext2_inode) );
        run( fd, super, group, inode, inode_bmap, data_bmap ); 
        
      }
      offset += entry->rec_len;
      entry = (void *) entry + entry->rec_len;
    } 
  }
  printf("----------------------------\n");
}

void check_dup_files( int fd, struct ext2_inode *root, struct ext2_group_desc * group, struct ext2_inode * inode, uint n_inode, char * data_bmap)
{
  uint inodes_per_block = block_size / sizeof(struct ext2_inode);
  uint inode_block; 
  uint offset; 
  for( uint i = 0; i < 15; i++ ) {
    inode_block = inode->i_block[i] / inodes_per_block;
    offset      = inode->i_block[i] % inodes_per_block;
    if( bitmapGet( data_bmap[inode_block], offset ) ) {
      printf("Repetido\n");
    }
    else{
      data_bmap[inode_block] = bitmapSwap( data_bmap[inode_block], offset );
    }
  }
}


void start_cleaning( int fd ) {
  struct ext2_super_block * super = malloc(sizeof(struct ext2_super_block));

  lseek(fd, BASE_OFFSET, SEEK_SET);
  read(fd, super, sizeof(struct ext2_super_block));

  block_size = 1024 << super->s_log_block_size;

  uint inodes_per_block = block_size / sizeof(struct ext2_inode);
  uint itable_blocks    = super->s_inodes_per_group / inodes_per_block;
  uint group_count      = 1 + (super->s_blocks_count-1) / super->s_blocks_per_group;

  struct ext2_group_desc * group = malloc(sizeof(struct ext2_group_desc)*group_count);
  lseek(fd, BASE_OFFSET + block_size, SEEK_SET);
  read(fd, group, group_count*sizeof(struct ext2_group_desc));

  struct ext2_inode * root = malloc(sizeof(struct ext2_inode));
  lseek(fd, BLOCK_OFFSET( group[0].bg_inode_table ) + sizeof(struct ext2_inode), SEEK_SET);
  read(fd, root, sizeof(struct ext2_inode));
  
  unsigned char * block = malloc( block_size );
  struct ext2_dir_entry_2 * entry;

  struct ext2_inode * inode = malloc(sizeof(struct ext2_inode));

  unsigned char * data_bmap = calloc( block_size , group_count );
  unsigned char * inode_bmap = calloc( block_size , group_count );

  for( int i=0; i<12; i++ ){
    if( root->i_block[i] == 0 )
      continue;
    // DUPLICATE BLOCK
    uint inode_block = root->i_block[i] / inodes_per_block;  
    uint offset      = root->i_block[i] % inodes_per_block;

    lseek(fd, BLOCK_OFFSET( root->i_block[i] ), SEEK_SET);
    read(fd, block, block_size);
    entry = (void *) block;
    offset = 0;
    while( offset < root->i_size ) {
      char fname[255];
      memcpy(fname, entry->name, entry->name_len);
      fname[entry->name_len] = 0;
      printf("Inode: %4.d, Size: %4.d, Type: %d, Name: %s\n", entry->inode, entry->rec_len, entry->file_type, fname); 
      if( fname[0] != '.' && entry->file_type == 2 && entry->inode != 11 ) {
        uint group_id = (entry->inode - 1)/(super->s_inodes_per_group);
        uint inode_offset = ((entry->inode - 1) % super->s_inodes_per_group)*sizeof(struct ext2_inode);
        lseek( fd, BLOCK_OFFSET(group[group_id].bg_inode_table) + inode_offset, SEEK_SET ); 
        read( fd, inode, sizeof(struct ext2_inode) );
        run( fd, super, group, inode, inode_bmap, data_bmap); 
      }
      else {
        uint group_id = (entry->inode - 1)/(super->s_inodes_per_group);
        uint inode_offset = ((entry->inode - 1) % super->s_inodes_per_group)*sizeof(struct ext2_inode);
        lseek( fd, BLOCK_OFFSET(group[group_id].bg_inode_table) + inode_offset, SEEK_SET ); 
        read( fd, inode, sizeof(struct ext2_inode) );
        check_dup_files( fd, root, group, inode, entry->inode, data_bmap );
      }
      offset += entry->rec_len;
      entry = (void *) entry + entry->rec_len;
    } 
  }

}

// Shit Try
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
            // Bloco com mais de 1 ref // Remover inode
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

int main(int agrc, const char * argv[])
{
  int fd;
  fd = open(argv[1], O_RDWR);

  printf("----------SUPERBLOCK CHECK--------\n");
  superblock_fix( fd );
  printf("----------------------------------\n");

  //multiple_inode( fd );
  printf("----------CLEANING ERROS----------\n");
  start_cleaning( fd );
  printf("-------------END------------------\n");

  return 0;
}
