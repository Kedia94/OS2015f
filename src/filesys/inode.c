#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define INDIRECT_SIZE 65536			/* 512*128. */

struct inode_disk* change_to_double_inode (struct inode_disk *disk_inode);

struct indirect_blocks
  {
	block_sector_t blocks[128];
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* Returns the number of indirect blocks to allocate for and inode SIZE bytes long. */
static inline size_t
bytes_to_indirect (off_t size)
{
	return DIV_ROUND_UP(size, INDIRECT_SIZE);
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
//	printf("(debug): started byte_to_sector %d, %d of %d\n", inode->sector, pos, inode->data.length);
//	printf("(debug)byte_to_sector from %d\n", inode->data.indirect_blocks);
  ASSERT (inode != NULL);

  int total_id = inode->data.length/BLOCK_SECTOR_SIZE;
//printf("id: %d, totalid: %d, pos: %d\n", bytes_to_sectors(pos), total_id, pos);
  if (inode->data.length == 0)
  	  return -1;
  if (pos <= inode->data.length)
  {
  	int id = pos/BLOCK_SECTOR_SIZE;
  	if ((total_id/128)*128 == (id/128)*128)
	{
		struct indirect_blocks block;
		block_read(fs_device, inode->data.indirect_blocks, &block);
//		printf("(debug) indirect:%d\n", inode->data.indirect_blocks);
//		if (block.blocks[id%128] == 0){
//		if(inode->sector == 111)
//			printf("(debug)1 ended byte_to_sector %d\n", block.blocks[id%128]);
//			printf("sector: %d, id: %d, total: %d, pos/size: %d/%d\n", inode->sector, id, total_id, pos, inode->data.length);
		
//			int j;
//			for (j=0;j<129;j++)
//			{
//				printf("%d: %d\n", j, block.blocks[j]);
//			}
//		}
		return block.blocks[id%128];
	}
	else
	{
		struct indirect_blocks block;
		block_read(fs_device, inode->data.double_indirect_blocks, &block);
		int sectorid = id/128;
		block_read(fs_device, block.blocks[sectorid], &block);
//		if (block.blocks[id]==0)
//		{
//		if(inode->sector == 111)
//		printf("(debug)2 ended byte_to_sector %d\n", block.blocks[id]);
//			printf("sector: %d, id: %d, total: %d, pos/size: %d/%d\n", inode->sector, id, total_id, pos, inode->data.length);
//		}
		return block.blocks[id%128];
	}
  }
  else
  {
//  	printf("(debug): ended byte_to_sector -1\n");
    return -1;
  }
}

/* Return true if successful allocating, return false if else. */

bool expand_inode (struct inode_disk *disk_inode, off_t new_size)
{
//	printf("(debug): started expand_inode: %d, size %d to %d\n",disk_inode->sector, disk_inode->length, new_size);
	bool result = false;
	/* Check if size is decreased. */
	ASSERT(new_size >= disk_inode->length);

	static char zeros[BLOCK_SECTOR_SIZE];
	int i;


	size_t new_sector = bytes_to_sectors (new_size) - bytes_to_sectors(disk_inode->length);
	size_t previous_sector = bytes_to_sectors(disk_inode->length);

	int indirect_index = (int)previous_sector%128;
	int double_index = bytes_to_indirect(disk_inode->length);
	
	/* don't have to expand. */
	if (new_sector == 0){
//		printf("(debug): ended expand_inode: not changed\n");
		disk_inode->length = new_size;
		block_write(fs_device, disk_inode->sector, disk_inode);
		return true;
	}
	
	struct indirect_blocks block;
	if (disk_inode->length == 0){
		free_map_allocate(1, &disk_inode->indirect_blocks);
		block_write(fs_device, disk_inode->indirect_blocks, zeros);
//		if (disk_inode->sector == 111)
//		printf("(debug)expand_inode: %d's indirect: %d\n", disk_inode->sector, disk_inode->indirect_blocks);
	}
	block_read(fs_device, disk_inode->indirect_blocks, &block);

	for (i=0;i<new_sector;i++)
		{
//			printf("%d\n", disk_inode->indirect_blocks);
			if (disk_inode->indirect_blocks == 0)
			{
				free_map_allocate(1, &disk_inode->indirect_blocks);
				block_write(fs_device, disk_inode->indirect_blocks, zeros);
				block_read(fs_device, disk_inode->indirect_blocks, &block);
//				if (disk_inode->sector == 111)
//					printf("(debug)expand_inode: %d's indirect: %d\n", disk_inode->sector, disk_inode->indirect_blocks);
			}
//			printf("%d\n", disk_inode->indirect_blocks);
			if (free_map_allocate(1, &block.blocks[(indirect_index+i)%128]))
			{
				block_write(fs_device, block.blocks[(indirect_index+i)%128], zeros);
			}
//			printf("%d\n", disk_inode->indirect_blocks);
			if ((indirect_index+i+1)%128 == 0)
			{
				block_write(fs_device, disk_inode->indirect_blocks, &block);
				disk_inode = change_to_double_inode(disk_inode);
			}
//			printf("indirect: %d\n", disk_inode->indirect_blocks);
//			if (disk_inode->sector == 111){
//			printf("(debug) allocate sector: %d to %d\n", block.blocks[(indirect_index+i)%128], disk_inode->sector);
//			}
		}
	block_write(fs_device, disk_inode->indirect_blocks, &block);
//	printf("allo: %d\n",block.blocks[0]);
//	printf("sector: %d, indirect: %d\n", disk_inode->sector, disk_inode->indirect_blocks);
	disk_inode->length = new_size;
	block_write(fs_device, disk_inode->sector, disk_inode);
	result = true;
//	printf("(debug): ended expand_inode\n");
	return result;
}

struct inode_disk* change_to_double_inode (struct inode_disk *disk_inode)
{
//	printf("(debug): started inode_disk %d, leng: %d\n", disk_inode->sector, disk_inode->length);
//	ASSERT(bytes_to_sectors(disk_inode->length) %128 == 0);
	static char zeros[BLOCK_SECTOR_SIZE];

	if (disk_inode->double_indirect_blocks == 0)
	{
		struct indirect_blocks block;
		if (free_map_allocate(1, &disk_inode->double_indirect_blocks))
		{
			block_write(fs_device, disk_inode->double_indirect_blocks, zeros);
		}
		block_read(fs_device, disk_inode->double_indirect_blocks, &block);
		block.blocks[0] = disk_inode->indirect_blocks;
		disk_inode-> indirect_blocks = 0;
		block_write(fs_device, disk_inode->double_indirect_blocks, &block);
//		if(disk_inode->sector == 111)
//		printf("(debug): ended inode_disk: doubleindir: %d \n", disk_inode->double_indirect_blocks);
		return disk_inode;
	}
	else
	{
		struct indirect_blocks block;
		block_read(fs_device, disk_inode->double_indirect_blocks, &block);
		size_t double_index=0;
		while(block.blocks[double_index] == 0){
			double_index++;
		}
		block.blocks[double_index] = disk_inode->indirect_blocks;
		disk_inode->indirect_blocks = 0;
		block_write(fs_device, disk_inode->double_indirect_blocks, &block);
//		printf("(debug): ended inode_disk\n");
		return disk_inode;
	}
}

/*		
bool expand_inode_double (struct inode_disk *disk_inode, off_t new_size)
{
	else if (expand_size <= 128)
	{
		struct indirect_blocks block;
		if (disk_inode->length == 0){
			free_map_allocatge(1, &disk_inode->indirect_blocks);
			block_write(fs_device, disk_inode->indirect_blocks, zeros);
		}
		block_read(fs_device, disk_inode->indirect_blocks, &block);

		for (i=nonblank_indirect; i<128; i++)
		{
			if (free_map_allocate(1, &block.blocks[i]))
			{
				block_write(fs_device, block.blocks[i], zeros);
			}
		}
		if (disk_inode->length <=128)
		{
			if (free_map_allocate(1, &disk_inode->double_indirect_blocks))
			{
				block_write(fs_device, disk_inode->double_indirect_blocks, zeros);
			}
		}
		struct indirect_blocks indirect;
		block_read(fs_device, disk_inode->double_indirect_blocks, &indirect);

		indirect.blocks[bytes_to_indirect (disk_inode->length)] = disk_inode->indirect_blocks;
		if (free_map_allocate(1, &disk_inode->indirect_blocks))
		{
			block_write(fs_device, disk_inode->indirect_blocks, zeros);
		}
		block_read(fs_device, disk_inode->indirect_blocks, &block);

		for (i=0;i<expand_size - nonblank_indirect; i++)
		{
			if (free_map_allocate(1, &block.blocks[i]))
			{
				block_write(fs_device, block.blocks[i],zeros);
			}
		}
		block_write(fs_device, disk_inode->indirect_blocks, &block);
		block_write(fs_device, disk_inode->sector, disk_inode);
		result = true;
	}
	else
	{
		ASSERT (false);
	}
	return result;
}
*/
/*
bool allocate_inode (struct inode_disk *disk_inode)
{
	static char zeros[BLOCK_SECTOR_SIZE];
	size_t indirect = bytes_to_indirect (disk_inode->length);
	int i, j;
	
	struct indirect_blocks *points_block = NULL;
	struct indirect_blocks *indirect_block = NULL;
	indirect_block = calloc(1, sizeof(struct indirect_blocks));

	if (indirect_block != NULL)
	{
		for (i=0;i<indirect-1;i++)
		{
			free_map_allocate(1, &indirect_block->blocks[i]);
			points_block = calloc(1, sizeof(struct indirect_blocks));
			if (points_block == NULL)
			{
				return false;
			}
		}
	}
	return true;

			
}
*/
/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool type_dir)
{
	if (length > 8*1024*1024)
	{
		length = 8*1024*1024;
	}
//	printf("(debug): started inode_create %d, size: %d\n", sector, length);
  struct inode_disk *disk_inode = NULL;
  bool success = false;
  int i;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
	 //printf("size of disk_inode : %d\n", sizeof *disk_inode);
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
	  disk_inode->is_dir = type_dir;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->sector = sector;
      disk_inode->length = 0;
      disk_inode->indirect_blocks = 0;
      disk_inode->double_indirect_blocks = 0;
	  if (expand_inode(disk_inode, length))
	  {
	  	  success = true;
	  }
	  free (disk_inode);

	}
//  printf("(debug): finished inode_create\n");
  return success;
  /*
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      if (free_map_allocate (sectors, &disk_inode->start)) 
        {
          block_write (fs_device, sector, disk_inode);
          if (sectors > 0) 
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                block_write (fs_device, disk_inode->start + i, zeros);
            }
         //// success = true; 
        } 
      free (disk_inode);
    }
  return success;
  */
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  lock_init(&inode->lock);
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
//	printf("(debug): started inode_close: %d, size: %d\n", inode->sector, inode->data.length);
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
//      printf("removed: %d\n", inode->removed);
      if (inode->removed) 
        {
//        	printf("removing\n");
          free_map_release (inode->sector, 1);
		
		  int i, j;
		  struct indirect_blocks block;
//		  printf("inode indir: %d, double: %d\n", inode->data.indirect_blocks, inode->data.double_indirect_blocks);
		  if (inode->data.indirect_blocks != 0)
		  {
		  	  block_read (fs_device, inode->data.indirect_blocks, &block);
		  	  for (i=0;i<128;i++)
			  {
			  	  if (block.blocks[i] != 0)
				  {
//				  	  printf("(debug): deallocate sector %x\n", block.blocks[i]);
				  	  free_map_release(block.blocks[i], 1);
				  }
			  }
		  }
		  if (inode->data.double_indirect_blocks != 0)
		  {
			  block_read (fs_device, inode->data.double_indirect_blocks, &block);
			  struct indirect_blocks indirect;
			  for (i=0;i<128;i++)
			  {
			  	  if (block.blocks[i] != 0)
				  {
				  	  block_read (fs_device, block.blocks[i], &indirect);
				  	  for (j=0;j<128;j++)
					  {
					  	  if (indirect.blocks[j] != 0)
						  {
//						  	  printf("(debug): deallocate sector %x\n", indirect.blocks[j]);
						  	  free_map_release(indirect.blocks[j], 1);
						  }
					  }
				  }
			  }
		  }
        }

      free (inode); 
    }
//  printf("(debug): ended inode_close\n");
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
//	printf("(debug) inode_write_at started %d to %d\n", inode->data.length, offset+size);
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  if (offset + size > inode->data.length)
  {
  	  lock_acquire(&inode->lock);
  	  expand_inode(&inode->data, offset + size);
  	  lock_release(&inode->lock);
  }
  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
