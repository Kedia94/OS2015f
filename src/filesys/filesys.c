#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  init_file();

  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

static struct file*
filesys_getfile  (const char *name)
{
	if (name == NULL || strlen (name) == 0) return NULL;

	char* dirname = NULL;
	char* filename = NULL;

	dir_split (name, &dirname, &filename);

	struct dir* target_dir = (dirname == NULL)?
		dir_reopen (thread_current()->current_dir) : dir_getdir (dirname);
	
	if (target_dir != NULL)
	{
		if (strcmp (filename, "") == 0)
		{
			return file_open (target_dir->inode);
		}
		struct inode* file_inode = NULL;
		dir_lookup (target_dir, filename, &file_inode);
		dir_close (target_dir);
		return file_open (file_inode);
	}
	return NULL;
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool type_dir) 
{
  /*
  block_sector_t inode_sector = 0;
  struct dir *dir = dir_open_root ();
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
  */
  
  bool success = false;
  char *filename;
  char *dirname;

  if (strlen (name) > NAME_MAX) return false;
  if (name == NULL || strcmp (name, "") == 0 || !dir_split (name, &dirname, &filename))
  {
  	return false;
  }
  
  struct dir* parent;
  if (dirname == NULL)
  {
  	parent = dir_reopen (thread_current()->current_dir);
  }
  else
  {
  	parent = dir_getdir (dirname);
  }

  struct inode* parent_inode = parent->inode;

  struct inode* temp_inode;
  if (parent == NULL || dir_lookup (parent, name, &temp_inode))
  {	
	inode_close (temp_inode);
  }
  else
  {
  	block_sector_t sector;
	ASSERT (free_map_allocate (1, &sector));

	if (type_dir)
	{
		ASSERT (dir_create (sector, parent_inode->sector));
	}
	else
	{
		ASSERT (inode_create (sector, initial_size, false));
	}
	dir_add (parent, filename, sector);
	success = true;
  }
  dir_close (parent);
  free (dirname);
  free (filename);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
/*
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  dir_close (dir);

  return file_open (inode);
*/
	return filesys_getfile (name);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
//	printf("remove: %s\n", name);
	/*
  struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
  */

  struct file* file_t = filesys_open (name);

  if (file_t == NULL) return false;

  bool success = false;

  int root = 0;
  char *parent_dir;
  char *file_dir;
  dir_split (name, &parent_dir, &file_dir);
//printf("parent: %s, file: %s\n", parent_dir, file_dir);
  struct file *parent_file;
  if (strcmp (parent_dir, "") == 0)
  	parent_file = file_open (thread_current()->current_dir->inode);
  else if (strcmp (parent_dir, "/") == 0){
  	  root = 1;
  	  parent_file = filesys_open(parent_dir);
  	  //dir_open_root();
  }
  else
  	parent_file = filesys_open (parent_dir);

  struct dir parent;
  parent.inode = parent_file->inode;
  parent.pos = parent_file->pos;

  struct inode* inode_t = file_t->inode;
  bool type_dir = file_t->inode->data.is_dir;

  if (type_dir)
  {
  	struct dir dir_temp;
	dir_temp.inode = inode_t;
	dir_temp.pos = 0;

	block_sector_t sector = inode_t->sector;
	block_sector_t curr_dir_sector = thread_current()->current_dir->inode->sector;
	block_sector_t root_sector = dir_open_root()->inode->sector;

	if (sector == curr_dir_sector || sector == root_sector) return false;
//	printf("dir_is_empty: %d, dir_temp.inode: %d\n", dir_is_empty(&dir_temp), dir_temp.inode->open_cnt <= 1);
	success = dir_remove(&parent, file_dir);
//	printf("succ: %d\n",success);
//	printf("success\n");
//	inode_remove (file_t->inode);
//	success = true;
  }
  else
  {
  	success = dir_remove(&parent, file_dir);
//  	printf("succ: %d\n", success);
  }
  file_close (file_t);
  if (root ==0)
	  file_close (parent_file);
//	printf("result: %d\n",success);
  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
