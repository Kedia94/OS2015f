#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/directory.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/input.h"
#define VADDR_LIMIT 0x08048000
static void syscall_handler (struct intr_frame *);

/* Projects 2 and later. */
void halt (void);
void exit (int status);
tid_t exec (const char *file);
int wait (tid_t);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
int write (int fd, const void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

/* Project 4 */
bool chdir (const char *dir);
bool mkdir (const char *dir);
bool readdir (int fd, char *name);
bool isdir (int fd);
int inumber (int fd);


//modified for prj2
struct lock syscall_lock;
bool is_ptr_valid (const int *vaddr);
int ptr_user_to_kernel(const void *vaddr);

void
syscall_init (void) 
{
  lock_init(&syscall_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}


static void
syscall_handler (struct intr_frame *f UNUSED) 
{
//	printf("PHYS_BASE: %x\n", PHYS_BASE);
	
//	hex_dump(0xbffffea0,0xbffffea0, 80, true);
  /*
  printf("syscall handler\n");
  thread_exit ();
  */
//  	printf("\n syscall handler %x \n",f->esp);
  int* esp = f->esp;
  
//  printf("is_ptr_valid %d ",is_user_vaddr(esp+1) );
//  printf("%d \n", esp < VADDR_LIMIT);
//  printf("result: %d\n",is_ptr_valid(esp+1));
//printf("is_user_vaddr: %d %d\n", is_user_vaddr(esp), is_user_vaddr(esp+1));
//  if (esp+1 == 0x20101238)
//  	  exit(-1);
  if (!is_ptr_valid (esp)) 
  {
//  	  printf("esp\n");
  	exit (-1);
//  	printf("exited\n");
	return;
  }

//printf("string: %d\n", *esp); 
// printf("string: %d %d\n", *esp, *(esp+1));
  switch (*esp){
  	case SYS_HALT:
		halt ();
		break;

	case SYS_EXIT:
		if (is_ptr_valid(esp+1)==0){
			exit(-1);
		}
		exit (*(esp + 1));
		break;

	case SYS_EXEC:		
		*(int*)(esp +1) = ptr_user_to_kernel((const void*) *(int*)(esp +1));
		f->eax = exec (*(char **)(esp + 1));
		break;

	case SYS_WAIT:
		f->eax = wait (*(tid_t*)(esp + 1));
		break;

	case SYS_CREATE:
		if (*(char**)(esp + 1) == NULL) exit (-1);
		if (*(off_t*)(esp + 2) < 0) exit (-1);
		*(int*)(esp +1) = ptr_user_to_kernel((const void*) *(int*)(esp +1));
		f->eax = create (*(char**)(esp + 1), *(off_t*)(esp + 2));
		break;

	case SYS_REMOVE:
		if (*(char**)(esp + 1) == NULL) exit (-1);
		*(int*)(esp +1) = ptr_user_to_kernel((const void*) *(int*)(esp + 1));
		f->eax = remove (*(char**)(esp + 1));
		break;

	case SYS_OPEN:
		if (*(char**)(esp + 1) == NULL) exit (-1);	
		*(int*)(esp +1) = ptr_user_to_kernel((const void*) *(int*)(esp +1));
		f->eax = open (*(char**)(esp + 1));
		break;

	case SYS_FILESIZE:
		if (*(esp + 1) < 3) exit (-1);
		f->eax = filesize (*(esp + 1));
		break;

	case SYS_READ:
		if (*(esp + 2) == NULL || *(unsigned*)(esp + 3) < 0) exit (-1);
//		if ((!is_ptr_valid(esp+1)) || (!is_ptr_valid(esp+2)) || (!is_ptr_valid(esp+3)))
//		{
//			exit(-1);
//		}
		*(int*)(esp +2) = ptr_user_to_kernel((const void*) *(int*)(esp +2));	
//		*(int*)(esp+1) = ptr_user_to_kernel((const void*)*(int*)(esp+1));
		f->eax = read (*(esp + 1), *(void**)(esp + 2), *(unsigned*)(esp + 3));
		break;

	case SYS_WRITE:
		if (*(esp + 2) == NULL || *(unsigned*)(esp + 3) < 0) exit (-1);
//		if ((!is_ptr_valid(esp+1)) ||(!is_ptr_valid(esp+2))|| (!is_ptr_valid(esp+3)))
//		{
//		exit(-1);
//		}	
		*(int*)(esp +2) = ptr_user_to_kernel((const void*) *(int*)(esp +2));
//		*(int*)(esp+1) = ptr_user_to_kernel((const void*)*(int*)(esp+1));
		f->eax = write (*(esp + 1), *(void**)(esp + 2), *(unsigned*)(esp + 3));
//		printf("Write %d %d %d %d\n", f->eax, *(esp+1), (esp+2), *(esp+3));
		break;
	case SYS_SEEK:
		if (*(esp + 1) < 3) exit (-1);
		seek (*(esp + 1), *(unsigned*)(esp + 2));
		break;
	case SYS_TELL:
		if (*(esp + 1) < 3) exit (-1);
		f->eax = tell (*(esp + 1));
		break;
	case SYS_CLOSE:
		if (*(esp + 1) < 3) exit (-1);
		close (*(esp + 1));
		break;
	case SYS_CHDIR:
//		if (*(esp + 1) < 3) exit (-1);
		f->eax = chdir (*(esp + 1));
		break;
	case SYS_MKDIR:
//		if (*(esp + 1) < 3) exit (-1);
		f->eax = mkdir (*(esp + 1));
		break;
	case SYS_READDIR:
//		if (*(esp + 1) < 3) exit (-1);
		f->eax = readdir (*(esp + 1), *(esp + 2));
		break;
	case SYS_ISDIR:
//		if (*(esp + 1) < 3) exit (-1);
		f->eax = isdir (*(esp + 1));
		break;
	case SYS_INUMBER:
//		if (*(esp + 1) < 3) exit (-1);
		f->eax = inumber (*(esp + 1));
		break;
  }
//  printf("syscall handler end\n");
}

/* Projects 2 and later. */
void halt (void)
{
	shutdown_power_off();
}

void exit (int status)
{
	struct thread *t = thread_current();
//	printf("exit: %d %d\n", status, t->tid);
	printf("%s: exit(%d)\n", t->name, status);
//	printf("par: %d, %d \n", t->parent, t->parent->tid);
//	printf("hi!\n");
	t->cp->alive = false;
	t->cp->status = status;
//	printf("end_exit1\n");
	thread_exit();
//	printf("end exit\n");
}
tid_t exec (const char *file)
{
//	printf("exec\n");
	tid_t tid = process_execute(file);
//	printf("exec check 1\n");
	struct child_process *cp = find_cp(thread_current(), tid);
//	while(1);
//	printf("exec end\n");
	while(cp->start ==0)
	{
		barrier();
	}
	if (cp->start == 2)
	{
		return -1;
	}

	return tid;
}

int wait (tid_t tid)
{
//	printf("wait\n");
	return process_wait(tid);
}

/* Code below here doesn't work perfectly */

bool create (const char *file, unsigned initial_size)
{
	/*
	bool result = filesys_create (file, initial_size);
	return result;
	*/
//	printf("create\n");
//	p_filename(file);
	lock_acquire(&syscall_lock);
	bool result = filesys_create(file, initial_size, false);
	lock_release(&syscall_lock);
	return result;
}

bool remove (const char *file)
{
	/*
	bool result = filesys_remove(file);
	return result;
	*/
	lock_acquire(&syscall_lock);
	bool result = filesys_remove(file);
	lock_release(&syscall_lock);
//	printf("return: %d\n", result);
	return result;
}

int open (const char *file)
{
	//return filesys_open(file);
	lock_acquire (&syscall_lock);
	struct thread* curr = thread_current();
	struct file* file_open = filesys_open(file);
	if(file_open == NULL)
	{
		lock_release(&syscall_lock);
		return -1;
	}
	file_open->fd = fd_count;
	++fd_count;
	list_push_back(&curr->my_open_files, &file_open->elem_private);
	lock_release(&syscall_lock);
	return file_open->fd;
}

int filesize (int fd)
{
	//return file_length(fd);
	lock_acquire (&syscall_lock);
	struct file* file_msrd = get_file (fd);
	if(file_msrd == NULL)
	{
		lock_release (&syscall_lock);
		return -1;
	}
	int size = file_length (file_msrd);
	//acquire() to release(). fixed.
	lock_release (&syscall_lock);
	return size;
}
int read (int fd, void *buffer, unsigned length)
{
	//return file_read(fd, buffer, length);
//	printf("read start\n");
	int i;
	char* buffer_read = (char*) buffer;

	if (fd == STDIN_FILENO){	//0 for standard input
		for(i = 0; i < length; i++)
		{
			buffer_read[i] = input_getc();
		}
//		printf("read end 1\n");
		return length;
	}
	else if (isdir(fd))
	{
		return -1;
	}
	else
	{
		lock_acquire (&syscall_lock);
		struct file* file_r = get_file (fd);
		if (file_r == NULL)
		{
			lock_release (&syscall_lock);
//			printf("read end 2\n");
			return -1;
		}
		int length_read = file_read (file_r, buffer, length);
		lock_release (&syscall_lock);
//		printf("read end 3\n");
		return length_read;
	}
}

int write (int fd, const void *buffer, unsigned length)
{
//	printf("start write %d\n",fd);
	//return file_write(fd, buffer, length);
	if (fd == STDOUT_FILENO)
	{
		putbuf (buffer, length);
//		printf("write end 1 %d\n", length);
		return length;
	}
	else if (isdir(fd))
	{
		return -1;
	}
	else
	{
//		printf("else\n");
		lock_acquire (&syscall_lock);
		struct file* file_written = get_file (fd);
		if (file_written == NULL)
		{
			lock_release (&syscall_lock);
//			printf("write end 2\n");
			return -1;
		}
		int length_written = file_write (file_written, buffer, length);
		lock_release (&syscall_lock);
//		printf("write end 3 %d\n",length_written);
		return length_written;
	}
}

void seek (int fd, unsigned position)
{
	//file_seek(fd, position);
	lock_acquire (&syscall_lock);
	struct file* file_s = get_file (fd);
	if (file_s == NULL)
	{
		lock_release (&syscall_lock);
		return -1;
	}
	file_seek(file_s, position);
	lock_release (&syscall_lock);
	return;
}

unsigned tell (int fd)
{
	//return file_tell(fd);
	lock_acquire (&syscall_lock);
	struct file* file_told = get_file (fd);
	if (file_told == NULL)
	{
		lock_release (&syscall_lock);
		return -1;
	}
	unsigned offset = file_tell (file_told);
	lock_release (&syscall_lock);
	return offset;
}

void close (int fd)
{
	//file_close(fd);
	lock_acquire (&syscall_lock);
	struct file* file_c = get_file (fd);
	if (file_c == NULL)
	{
		lock_release (&syscall_lock);
		return -1;
	}
	list_remove (&file_c->elem_private);
	file_close(file_c);
	lock_release (&syscall_lock);
	return;
}

/* Project 4 */
bool chdir (const char *dir)
{
	//return false;
	lock_acquire (&syscall_lock);
	struct thread* curr = thread_current();
	struct dir* dir_dst = dir_getdir (dir);
	
	if (dir_dst == NULL){
		lock_release (&syscall_lock);
		return false;
	}
	dir_close (curr->current_dir);
	curr->current_dir = dir_dst;
	lock_release (&syscall_lock);
	return true;
}

bool mkdir (const char *dir)
{
	//return false;
	lock_acquire (&syscall_lock);
	bool result = filesys_create (dir, 0, true);
	lock_release (&syscall_lock);
	return result;
}

bool readdir (int fd, char *name)
{
	//return false;
//	printf("start readdir\n");
	lock_acquire (&syscall_lock);
	if (!isdir (fd)){
		lock_release (&syscall_lock);
//		printf("check 1\n");
		return false;
	}
//	printf("check 1.5\n");
	struct file* file_t = get_file (fd);

	if (file_t == NULL){
		lock_release (&syscall_lock);
//		printf("check 2\n");
		return false;
	}

	struct dir dir_copy;
	dir_copy.inode = file_t->inode;
	dir_copy.pos = file_t->pos;

	bool result = dir_readdir (&dir_copy, name);
	if (result) file_t->pos = dir_copy.pos;
	lock_release (&syscall_lock);
//	printf("check end: %d\n",result);
	return result;
}

bool isdir (int fd)
{
	//return false;
//	lock_acquire (&syscall_lock);
	struct file* file_t = get_file (fd);
	
	if (file_t == NULL){
//		lock_release (&syscall_lock);
		return false;
	}

	struct inode* inode_t = file_t->inode;

	if (inode_t == NULL){
//		lock_release (&syscall_lock);
		return false;
	}

	bool result = (inode_t->data).is_dir;
//	lock_release (&syscall_lock);
	return result;
}

int inumber (int fd)
{
	lock_acquire (&syscall_lock);
	struct file* file_t = get_file (fd);

	if (file_t == NULL){
		lock_release (&syscall_lock);
		return -1;
	}

	struct inode* inode_t = file_t->inode;

	if (inode_t == NULL){
		lock_release (&syscall_lock);
		return -1;
	}

	block_sector_t inumber_t = inode_t->sector;
	lock_release (&syscall_lock);
	return inumber_t;
}

bool is_ptr_valid (const int *vaddr)
{
//	printf("%d %d\n", is_user_vaddr(vaddr), vaddr<VADDR_LIMIT);
	return (is_user_vaddr(vaddr) && !(vaddr < VADDR_LIMIT));
	//	return (is_user_vaddr(vaddr) && (int)vaddr < VADDR_LIMIT && pagedir_get_page(thread_current()->pagedir, vaddr) != NULL);
}

int ptr_user_to_kernel(const void *vaddr)
{
	if (!is_ptr_valid(vaddr))
		exit(-1);

	void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
	if (ptr != NULL)
		 return (int) ptr;
	else exit(-1);
}
/*
bool is_buffer_valid (const void *buf, unsigned length)
{
	unsigned i;
	bool k = true;
	for (i = 0; i < length; i++)
	{
		if (!is_ptr_valid(buf++))
			k = false;
			break;
	}
	return k;
}

*/
/*
static struct file*
get_file (int fd)
{
	struct list* file_list = &(thread_current()->my_open_files);
	struct list_elem* e;
	struct file* fe;

	for (e = list_begin (file_list); e != list_end (file_list); e = list_next (e))
	{
		fe = list_entry (e, struct file, elem_private);
		if (fe->fd == fd)
		{
			return fe;
		}
	}
	return NULL;
}
*/
