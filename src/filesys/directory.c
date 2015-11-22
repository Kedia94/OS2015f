#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, block_sector_t parent)
{
  //return inode_create (sector, entry_cnt * sizeof (struct dir_entry));

  bool success = true;
  struct dir *new_dir = malloc (sizeof (struct dir));

  success = inode_create (sector, 0, true);
  //third parameter 'type_dir = true'
  new_dir->inode = inode_open (sector);
  new_dir->pos = 0;
  success = (success)? dir_add (new_dir, ".", sector) : false;
  success = (success)? dir_add (new_dir, "..", parent) : false;
  dir_close (new_dir);

  return success;
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
//	printf("start dir_remove: %d %s\n",dir, name);
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  lock_acquire(&dir->inode->lock);
//  printf("locked\n");
  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;
//printf("check1\n");
  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;
//printf("check0\n");
  if (inode->data.is_dir)
  {
//  	  printf("check2\n");
  	  if (inode->open_cnt > 2){
//  	  	  printf("h0: %d\n",inode->open_cnt);
  	  	  goto done;
	  }
//	  printf("check3\n");
  	  if (!dir_is_empty(dir->inode)){
//		  	printf("h1\n");
  	  	  goto done;
	  }
  }
//printf("check1\n");
  /* Erase directory entry. */
  e.in_use = false;
//  printf("%s: %d\n", e.name, e.in_use);
//  printf("dir: %x, inode: %x\n", dir, dir->inode);
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;
//printf("success: %d %d\n", success, e.in_use);
 done:
//  printf("remove done\n");
  inode_close (inode);
  lock_release(&dir->inode->lock);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;
//  printf("dir_readdir: %x\n", name);
  lock_acquire(&dir->inode->lock);
//  printf("lockes\n");
  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
//    	printf("read %s\n", e.name);
      dir->pos += sizeof e;
      if (e.name[0] == '.')
      	  continue;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          lock_release(&dir->inode->lock);
          return true;
        } 
    }
  lock_release(&dir->inode->lock);
  return false;
}

bool
dir_split (const char* route, char** dir, char** file)
{
  size_t route_len = strlen (route);
  char* dir_and_file = malloc (route_len + 1);
  strlcpy (dir_and_file, route, route_len + 1);

  if (dir_and_file[route_len - 1] == '/') dir_and_file[route_len - 1] = '\0';
  //"pintos/src/filesys/run/" -> "pintos/src/filesys/run"

  char* separator = strrchr (dir_and_file, '/');

  if (separator == NULL)
  {
  	//file at current directory
	*file = dir_and_file;
	*dir = calloc (1, sizeof (char));
	return true;
  }
  else if (separator == dir_and_file)
  {
    //file at root directory
	unsigned int dnf_len = strlen (dir_and_file);
	*file = malloc (dnf_len);
	strlcpy (*file, dir_and_file + 1,  dnf_len);
	*dir = calloc (2, sizeof (char));
	(*dir)[0] = '/';
  }
  else
  {
  	//normal. ex) "pintos/src/filesys/run"
	unsigned int dir_len = separator - dir_and_file;
	*dir = malloc (dir_len + 1);
	strlcpy (*dir, dir_and_file, dir_len + 1);
	unsigned int dnf_len = strlen (dir_and_file);
	unsigned int file_len = dnf_len - dir_len;
	*file = malloc (file_len + 1);
	strlcpy (*file, separator + 1, file_len + 1);
  }
  free (dir_and_file);
  return true;
 }

 struct dir*
 dir_getdir (const char *dir)
 {
 	if (dir == NULL) return thread_current() -> current_dir;

	struct dir* curr_dir;
	unsigned int dir_len = strlen (dir);

	if (dir[0] == '/')
	{
		//root
		curr_dir = dir_open_root();
	}
	else
	{
		curr_dir = dir_reopen (thread_current() -> current_dir);
	}
	char* dir_copy = malloc (dir_len + 1);
	strlcpy (dir_copy, dir, dir_len + 1);
	char* next;
	char* save_ptr;
	struct inode* next_inode = NULL;

	for (next = strtok_r (dir_copy, "/", &save_ptr); next != NULL;
		next = strtok_r (NULL, "/", &save_ptr))
	{
		if (dir_lookup (curr_dir, next, &next_inode))
		{
			struct dir *temp_dir = dir_open (next_inode);
			dir_close (curr_dir);
			curr_dir = temp_dir;
		}
		else
		{
			//WTF? directory not found?
			dir_close (curr_dir);
			return NULL;
		}
	}
	return curr_dir;
}

bool dir_is_empty (struct dir* dir_t)
{
//	printf("(start) dir_is_empty\n");
	off_t ori_pos = dir_t->pos;
//printf("dir_t: %x\n", dir_t->inode);
	struct dir_entry e;
	struct inode *inode = dir_t->inode;
	off_t pos = 0;
	while(inode_read_at (inode, &e, sizeof e, pos) == sizeof e)
	{
		pos += sizeof e;
		if (e.name[0] == '.')
			continue;
		if (e.name[0]< '0' || e.name[0] > 'z')
			break;
		if (e.in_use){
//			printf("here %d, %d, %s, %d\n",pos, sizeof e, e.name, e.in_use);
			return false;
		}
	}
	return true;
/*
	char *temp_name = malloc (NAME_MAX + 1);

	dir_t->pos = 0;
	printf("check1\n");
	bool empty = !dir_readdir (dir_t, temp_name);
printf("check2\n");
	dir_t->pos = ori_pos;
	free (temp_name);
*/
//	return empty;
}
