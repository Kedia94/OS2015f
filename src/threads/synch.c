/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

//0928_Hwang : 우선순위 비교용
static bool priority_more(const struct list_elem *a_in, const struct list_elem *b_in, void *aux);
static bool priority_more_cond(const struct list_elem* a_in, const struct list_elem* b_in, void* aux);
static bool priority_more_locks(const struct list_elem* a_in, const struct list_elem* b_in, void* aux);
/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0) 
  {
	//0928_Hwang -> 1003 Hyejin
        list_push_back (&sema->waiters, &thread_current ()->elem);
  	list_sort(&sema->waiters, priority_more,NULL);
	//list_insert_ordered(&sema->waiters, &thread_current ()->elem, priority_more, NULL);
	sema->max_priority_of_waits = list_entry(list_front(&sema->waiters), struct thread, elem)->priority;
      	thread_block ();
  }
  sema->value--;
  intr_set_level (old_level);
}

//0928_Hwang : 우선순위 비교 함수
static bool
priority_more(const struct list_elem* a_in, const struct list_elem* b_in, void* aux)
{
	const struct thread* a = list_entry(a_in, struct thread, elem);
	const struct thread* b = list_entry(b_in, struct thread, elem);

	return (a->priority > b->priority)? true : false;
}

//0929_Hwang : 락 전용
static bool
priority_more_locks(const struct list_elem* a_in, const struct list_elem* b_in, void* aux)
{
	const struct lock* a = list_entry(a_in, struct lock, elem);
	const struct lock* b = list_entry(b_in, struct lock, elem);

	return (&a->semaphore.max_priority_of_waits > &b->semaphore.max_priority_of_waits)?
	true : false;
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;
  //0929_Hwang
  struct thread* curr = thread_current ();
  bool need_yield = false;
  //

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  //0929_Hwang
  if(!list_empty(&sema->waiters))
  {
	list_sort(&sema->waiters, priority_more, NULL);
  	struct thread* thread_unblocked = list_entry(list_pop_front(&sema->waiters), struct thread, elem);

	thread_unblock (thread_unblocked);
    if(thread_unblocked->priority > curr->priority)
		need_yield = true;
  }
  //
 //if (!list_empty (&sema->waiters)) 
 //  thread_unblock (list_entry (list_pop_front (&sema->waiters),
 //                             struct thread, elem));
  sema->value++;
  intr_set_level (old_level);

  //0929_Hwang
  if(need_yield)
  {
  	if(!intr_context ()) thread_yield ();
	else intr_yield_on_return();
  }
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);
}

//0928_Hwang : 기여도 기부 함수
void
lock_donation (struct lock* target_lock)
{
	if(target_lock->holder != NULL) {
    		struct thread* curr = thread_current ();
    		struct thread* owner_now = target_lock->holder;

		if(owner_now->priority < curr->priority) {
    			if(owner_now->donated_times == 0)
				owner_now->ori_priority = owner_now->priority;
     			owner_now->priority = curr->priority;
     			owner_now->donated_times++;
    			if(owner_now->wait_for_lock != NULL)
				lock_donation(owner_now->wait_for_lock);
		}
		
	}
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));
  
  //0929_Hwang
  struct thread* curr = thread_current ();

  curr->wait_for_lock = lock;
  lock_donation (lock);
  sema_down(&lock->semaphore);
  lock->holder = curr;
  list_insert_ordered(&curr->holding_locks, &curr->wait_for_lock->elem, priority_more_locks, NULL);
  curr->wait_for_lock = NULL;
  //
  //sema_down (&lock->semaphore);
  //lock->holder = current_thread ();
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;
  struct thread* curr = thread_current();

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));
  
  
  success = sema_try_down (&lock->semaphore);
  if (success) {
    //1003_Hyejin
    curr->wait_for_lock = lock;
    lock_donation (lock);
    sema_down(&lock->semaphore);
    lock->holder = curr;
    list_insert_ordered(&curr->holding_locks, &curr->wait_for_lock->elem, priority_more_locks, NULL);
    curr->wait_for_lock = NULL;
    //1003
  }
  return success;
}

//0929_Hwang
void
donation_back (struct lock* lock)
{
	struct thread* curr = thread_current ();
	struct semaphore sema = lock->semaphore;
	struct list waiters_s = sema.waiters;

	if (curr->donated_times > 0)
	{
		if(!list_empty (&waiters_s))
		{
			struct thread* donor_thread = list_entry(list_front(&waiters_s), struct thread, elem);
			
			if(curr->priority == donor_thread->priority)
			{
				if(curr->donated_times == 1)
				{
					curr->priority = curr->ori_priority;
					curr->donated_times--;
				}
				else
				{
					if(!list_empty(&curr->holding_locks))
					{
						
						struct semaphore sema_2 = list_entry(list_max(&curr->holding_locks, priority_more, NULL), struct lock, elem)->semaphore;
						curr->priority = sema_2.max_priority_of_waits;
						curr->donated_times--;
					}
					else
					{
						curr->priority = curr->ori_priority;
						curr->donated_times--;
					}
				}
			}
		}
	}
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  //0929_Hwang
  list_remove (&lock->elem);
  donation_back (lock);
  lock->holder = NULL;
  sema_up (&lock->semaphore);
  //lock->holder = NULL;
  //sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

//1003_Hyejin : Condition 전용
static bool
priority_more_cond(const struct list_elem* a_in, const struct list_elem* b_in, void* aux)
{
        const struct semaphore_elem *sa = list_entry(a_in, struct semaphore_elem, elem);
        const struct semaphore_elem *sb = list_entry(b_in, struct semaphore_elem, elem);

        if (list_empty(&sb->semaphore.waiters))
                return true;
        else if (list_empty(&sa->semaphore.waiters))
                return false;
        else {
                list_sort(&sa->semaphore.waiters, priority_more, NULL);
                list_sort(&sb->semaphore.waiters, priority_more, NULL);

                struct thread *ta = list_entry(list_front(&sa->semaphore.waiters), struct thread, elem);
                struct thread *tb = list_entry(list_front(&sb->semaphore.waiters), struct thread, elem);

                return (ta->priority > tb->priority)? true: false;
        }
}
//1003_Hyejin


/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  //0930 Jin
  //list_push_back (&cond->waiters, &waiter.elem);
  list_insert_ordered (&cond->waiters, &waiter.elem, priority_more_cond, NULL);
  //0930 Jin
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)) {
    //1003_Hyejin
    list_sort(&cond->waiters, priority_more_cond, NULL);
    //1003_Hyejin
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
  }
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}

