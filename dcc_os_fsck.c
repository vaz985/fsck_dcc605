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
void rm_inode_from_dir( unsigned char * block, uint i_pos, uint inode ) {
  struct ext2_dir_entry_2 * entry;
  struct ext2_dir_entry_2 * last_entry;
  entry = (void *) block;
  unsigned char * new_b = malloc(sizeof(block_size));
  uint offset = 0;
  __u16 acc = 0;
  
  while( offset < block_size ) {
    if( entry->rec_len + offset + acc == block_size ) {
      if( entry->inode == inode ) {
        __u16 last_rec = last_entry->rec_len;
        __u16 rec_len = (__u16)( last_entry->rec_len + entry->rec_len );
        memcpy( &last_entry->rec_len, &rec_len, 2 );
        memcpy( new_b + offset - last_rec , last_entry, 64+last_entry->name_len);
        memcpy((void*) block, (void*)new_b, block_size);
        return;
      }
      __u16 rec_len = (__u16)( entry->rec_len + acc );
      memcpy( &entry->rec_len, &rec_len, 2 );
      memcpy( new_b + offset, entry, 64+entry->name_len);
      memcpy((void*) block, (void*)new_b, block_size);
      return;
    }
    if( entry->inode == inode ) {
      acc += entry->rec_len;
      entry = (void *) entry + entry->rec_len;
      continue;
    }
    memcpy( (void*)new_b + offset, entry, entry->rec_len );  
    offset += entry->rec_len;
    last_entry = entry;
    entry = (void *) entry + entry->rec_len;
  }
}

// O MESMO INODE PODE TER BLOCOS REPETIDOS
void check_dup_blocks( int fd, struct ext2_inode *root, struct ext2_group_desc * group, struct ext2_inode * inode, uint n_inode, char * data_bmap, char * inode_bmap)
{
  struct ext2_super_block super;
  lseek( fd, 1024, SEEK_SET );
  read( fd, &super, sizeof(struct ext2_super_block) );
  if( n_inode == 2 )
    return;
  uint inodes_per_block = block_size / sizeof(struct ext2_inode);
  uint inode_block; 
  uint offset; 
  unsigned char * block = malloc( block_size );
  struct ext2_dir_entry_2 * entry;
  for( uint i = 0; i < 12; i++ ) {
    if( inode->i_block[i] == 0 )
      continue;
    inode_block = inode->i_block[i] / inodes_per_block;
    offset      = inode->i_block[i] % inodes_per_block;
    if( bitmapGet( data_bmap[inode_block], offset ) ) {
      printf("Bloco %d repetido\n", inode->i_block[i]);
      struct ext2_inode dir_inode;
      struct ext2_dir_entry_2 * new_block = malloc(sizeof(struct ext2_dir_entry_2));
    //inode_block =  (n_inode-1) / super.s_inodes_per_group;
    //offset      = ((n_inode-1) % super.s_inodes_per_group) * sizeof(struct ext2_inode);
      for( uint j = 0; j < 12; j++ ) {
        if( root->i_block[j] != 0 ) {
          lseek(fd, BLOCK_OFFSET( root->i_block[i] ), SEEK_SET);
          read(fd, block, block_size);
          entry = (void *) block;
          offset = 0;
          while( offset < root->i_size ) {
            char fname[255];
            memcpy(fname, entry->name, entry->name_len);
            fname[entry->name_len] = 0;
            if( entry->inode == n_inode ) {
              rm_inode_from_dir( block, offset, n_inode );
              lseek(fd, BLOCK_OFFSET( root->i_block[i] ), SEEK_SET);
              write(fd, block, block_size);
              printf("Adicionar swap do bitmap\n");
              return;
            } 
            offset += entry->rec_len;
            entry = (void *) entry + entry->rec_len;
          }
        }
      }
      printf("Inode Removido\n");
    }
    else{
      data_bmap[inode_block] = bitmapSwap( data_bmap[inode_block], offset );
    }
  }
}
void run( int fd, struct ext2_super_block * super, struct ext2_group_desc * group, struct ext2_inode * root, unsigned char * inode_bmap, unsigned char * data_bmap) 
{
  uint inodes_per_block = block_size / sizeof(struct ext2_inode);
  unsigned char * block = malloc( block_size );
  struct ext2_dir_entry_2 * entry;
  struct ext2_inode * inode = malloc(sizeof(struct ext2_inode));

  printf("-------------DIR------------\n");
  if( all_zero_blocks( root->i_block ) ){
    printf("---------EMPTY DIR----------\n");
    printf("----------------------------\n");
    return;
  }
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
      if( fname[0] != '.' && entry->file_type == 2)  {
        printf("Inode: %4.d, Size: %4.d, Type: %d, Name: %s\n", entry->inode, entry->rec_len, entry->file_type, fname); 
        uint group_id = (entry->inode - 1)/(super->s_inodes_per_group);
        uint inode_offset = ((entry->inode - 1) % super->s_inodes_per_group)*sizeof(struct ext2_inode);
        lseek( fd, BLOCK_OFFSET(group[group_id].bg_inode_table) + inode_offset, SEEK_SET ); 
        read( fd, inode, sizeof(struct ext2_inode) );
        run( fd, super, group, inode, inode_bmap, data_bmap ); 
      }
      else if( fname[0] != '.' ){
        printf("Inode: %4.d, Size: %4.d, Type: %d, Name: %s\n", entry->inode, entry->rec_len, entry->file_type, fname); 
        uint group_id = (entry->inode - 1)/(super->s_inodes_per_group);
        uint inode_offset = ((entry->inode - 1) % super->s_inodes_per_group)*sizeof(struct ext2_inode);
        lseek( fd, BLOCK_OFFSET(group[group_id].bg_inode_table) + inode_offset, SEEK_SET ); 
        read( fd, inode, sizeof(struct ext2_inode) );
        check_dup_blocks( fd, root, group, inode, entry->inode, data_bmap, inode_bmap );
      }
      offset += entry->rec_len;
      entry = (void *) entry + entry->rec_len;
    } 
  }
  printf("----------------------------\n");
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
      if( fname[0] != '.' && entry->file_type == 2 && entry->inode > 11 ) {
        printf("Inode: %4.d, Size: %4.d, Type: %d, Name: %s\n", entry->inode, entry->rec_len, entry->file_type, fname); 
        uint group_id     = (entry->inode - 1) / (super->s_inodes_per_group);
        uint inode_offset = ((entry->inode - 1) % super->s_inodes_per_group)*sizeof(struct ext2_inode);
        lseek( fd, BLOCK_OFFSET(group[group_id].bg_inode_table) + inode_offset, SEEK_SET ); 
        read( fd, inode, sizeof(struct ext2_inode) );
        run( fd, super, group, inode, inode_bmap, data_bmap); 
      }
      else if (fname[0] != '.'&& entry->inode > 11){
        printf("Inode: %4.d, Size: %4.d, Type: %d, Name: %s\n", entry->inode, entry->rec_len, entry->file_type, fname); 
        uint group_id = (entry->inode - 1)/(super->s_inodes_per_group);
        uint inode_offset = ((entry->inode - 1) % super->s_inodes_per_group)*sizeof(struct ext2_inode);
        lseek( fd, BLOCK_OFFSET(group[group_id].bg_inode_table) + inode_offset, SEEK_SET ); 
        read( fd, inode, sizeof(struct ext2_inode) );
        check_dup_blocks( fd, root, group, inode, entry->inode, data_bmap, inode_bmap );
      }
      offset += entry->rec_len;
      entry = (void *) entry + entry->rec_len;
    } 
  }

}


int main(int agrc, const char * argv[])
{
  int fd;
  fd = open(argv[1], O_RDWR);

  printf("##########SUPERBLOCK CHECK########\n");
  superblock_fix( fd );
  printf("##################################\n");

  printf("##########CLEANING ERROS##########\n");
  start_cleaning( fd );
  printf("#############END##################\n");

  return 0;
}
