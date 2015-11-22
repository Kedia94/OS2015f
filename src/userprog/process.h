#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

#define START_SUCCESS 1
#define START_FAIL 2
tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

/* struct thread (thread.h) refers struct child_process */
struct child_process {
	tid_t tid;
	bool alive;				/* true if not exited */
//	bool wait;
	int start;
	int status;
	struct list_elem elem;
};

struct child_process * make_cp(tid_t tid);						/* make child_process of thread_current() with tid */
struct child_process * find_cp(struct thread *t, tid_t tid);	/* find t's child_process by tid */
void remove_cp (struct child_process *cp);						/* remove and free memory cp */
void remove_all_cp (void);										/* remove and free memory of all child_process of thread_current() */
#endif /* userprog/process.h */
