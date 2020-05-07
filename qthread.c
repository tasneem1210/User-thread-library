
/*
 * file:        qthread.c
 * description: assignment - simple emulation of POSIX threads
 * class:       CS 5600, Fall 2019
 */

/* a bunch of includes which will be useful */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <errno.h>
#include "qthread.h"

/* prototypes for stack.c and switch.s
 * see source files for additional details
 */
extern void switch_to(void **location_for_old_sp, void *new_value);
extern void *setup_stack(int *stack, void *func, void *arg1, void *arg2);

void *main_stack_ptr; 

struct qthread *current;
struct threadq active;
struct threadq sleepers;
    
/* Mutex and cond structures - @allocate them in qthread_mutex_create / 
 * qthread_cond_create and free them in @the corresponding _destroy functions.
 */

/* qthread_create - see hints @for how to implement it, especially the
 * reference to a "wrapper" function
 */
//Return false if q is not empty. Return true if q is empty.
int tq_empty(struct threadq *q)
{
	if (q->head!=NULL) {
        return 0;
    }
    else {
        return 1;
    }
}

//Append qthread to the end of the thread q.
void tq_append(struct threadq *q,struct qthread *temp)
{
        if (q == NULL) {
            return;
        }
        if (temp==NULL) {
            return;
        }
        temp->next = NULL;
        if(q->head != NULL) {
            q->tail->next = temp;
            q->tail = temp;
        }
        else
        {
            q->head = q->tail = temp;
        }
}

//Pops out the first element of the q.
struct qthread* tq_pop(struct threadq *q)
{
    if (q == NULL) {
        return NULL;
    }
	if (q->head == NULL) {
		return NULL;
    }
	struct qthread *temp = q->head;
	q->head = temp->next;
	if(q->head == NULL) {
		q->tail = NULL;
    }
	return temp;
}

qthread_t create_2arg_thread(f_2arg_t f,void *arg1, void *arg2)
{
	struct qthread *temp = malloc(sizeof(*temp));
	memset(temp,0,sizeof(*temp));
	temp->saved_stack = malloc(8192);
	temp->sp = setup_stack(temp->saved_stack + 8192, f, arg1, arg2);
	
	tq_append(&active,temp);
	return temp;
}

void wrapper(void *arg1, void *arg2)
{
	f_1arg_t f = arg1;
	void *temp = f(arg2);
	qthread_exit(temp);
}

qthread_t qthread_create(f_1arg_t f, void *arg1)
{
    return create_2arg_thread(wrapper,f,arg1);
}

/* I suggest factoring your code so that you have a 'schedule'
 * function which selects the next thread to run and @switches to it,
 * or goes to sleep if there aren't any threads left to run.
 *
 * NOTE - if you end up switching back to the same thread, do *NOT*
 * use do_switch - check for this case and return from schedule(), 
 * or else @you'll crash.
 */


/* qthread_init - set up a thread structure for the main (OS-provided) thread
 */
void schedule(void *save_location){
    // TA Office Hour
	qthread_t self = current;
theStart:
	current = tq_pop(&active);
	// define a int variable and get the status of sleepers queue.
	int sleeperEmpty = tq_empty(&sleepers);
	if(current == self)
		return;
	if(current == NULL && sleeperEmpty)
		switch_to(NULL,main_stack_ptr);
	else if(current == NULL && !sleeperEmpty)
	{
		usleep(20000);
		// test the sleeper queue and update the sleeperEmpty status in the following while loop
		while(!sleeperEmpty){
			tq_append(&active,tq_pop(&sleepers));
			sleeperEmpty = tq_empty(&sleepers);
		}

		goto theStart;
	}
	switch_to(save_location,current->sp);
}


// qthread_init has to do is to allocate a blank thread structure and point 'current' to it
void qthread_init(void) {
    current = calloc(1, sizeof(*current));
}

/* qthread_yield - yield to the next @runnable thread.
 */
void qthread_yield(void)
{	
    // Use the tq_append function to add the current thread into the 'active' stack(?queue?) and this thread waits to be continued.
    // 将要停止的线程放到active队列末尾去，调用append函数。
    tq_append(&active, current);
    // Just use the schedule function to call the next thread. 
    // Specifically, the given location here is same as the current thread location. 
    //	Because the new scheduling thread location starts from the old(current) thread's yield position.
    
    // 因为在schedule中，第一行语句为‘current = tq_pop(&active);’。
    //	所以，转换到下一个thread是在schedule中进行的。
    //	也就是说：schedule函数的目的是schedule a new thread at the given save location（这里的save location为当前要yield的函数位置，
    //	因此也就是n这里yield了一个旧的thread，新schedule的thread也会在这里开始）
    schedule(&current->sp);
}

/* qthread_exit, qthread_join - exit argument is returned by
 * qthread_join. Note that join blocks if the thread hasn't exited
 * yet, and is allowed to crash @if the thread doesn't exist.
 */
void qthread_exit(void *val)
{
   struct qthread *temp = current;
   temp->exited_flag=1;
   temp->return_val=val;
   if(temp->waiting!=NULL) {
	   tq_append(&active,temp->waiting);
	   temp->waiting = NULL;;
   }
   schedule(&current->sp);
}

void *qthread_join(qthread_t thread)
{
	if (!thread->exited_flag) {
		thread->waiting = current;
		schedule(&current->sp);
	}
	void *val_return=thread->return_val;
	free(thread);
	return val_return;
}

/* Mutex functions
 */
qthread_mutex_t *qthread_mutex_create(void)
{
    struct qthread_mutex *newMutex = malloc(sizeof(*newMutex));
    memset(newMutex, 0, sizeof(*newMutex));
    newMutex->waiters.head = NULL;
    newMutex->waiters.tail = NULL;
    newMutex->locked = 0;
    return newMutex;
}
//free a mutex
void qthread_mutex_destroy(qthread_mutex_t *mutex)
{
    if (mutex==NULL) {
        return;
    }
    mutex->waiters.head = NULL;
    mutex->waiters.tail = NULL;
    mutex->locked = 0;
    free(mutex);
}

//If a mutex is unlocked, set locked to 1. If a mutex is locked, append to waiters.
void qthread_mutex_lock(qthread_mutex_t *mutex)
{
    if (mutex == NULL) {
        return;
    }
    if (mutex->locked==1) {
        tq_append(&mutex->waiters,current);
        schedule(&current->sp);
    } else {
        mutex->locked = 1;
    }
}

//Set unlock to 0 if locked.Otherwise, append waiting to current.
void qthread_mutex_unlock(qthread_mutex_t *mutex)
{
    if (mutex == NULL) {
        return;
    }
    tq_empty(&mutex->waiters)?mutex->locked = 0:tq_append(&active, tq_pop(&mutex->waiters));
}

/* Condition variable functions
 */
qthread_cond_t *qthread_cond_create(void)
{
    struct qthread_cond *cond = malloc(sizeof(qthread_cond_t));
    cond->waiters.head = NULL;
    cond->waiters.tail = NULL;
    return cond;
}

//Free a cond
void qthread_cond_destroy(qthread_cond_t *cond)
{
    if(cond == NULL) return;
    
    cond->waiters.head = (qthread_t) NULL;
    cond->waiters.tail = (qthread_t) NULL;
    free(cond);
}

//Block wait in cond.
void qthread_cond_wait(qthread_cond_t *cond, qthread_mutex_t *mutex)
{
    tq_append(&cond->waiters,current);
    qthread_mutex_unlock(mutex);
    schedule(&current->sp);
    qthread_mutex_lock(mutex);
}

//Unblocks thread blocked on a condition variable.
void qthread_cond_signal(qthread_cond_t *cond)
{
    int tq_isEmpty = tq_empty(&cond->waiters);
    if(tq_isEmpty)
        return;
    tq_append(&active,tq_pop(&cond->waiters));
}

//Unblocks all threads blocked on a condition variable.
void qthread_cond_broadcast(qthread_cond_t *cond)
{
    int tq_isNotEmpty = 1;
    while(tq_isNotEmpty){
    	tq_isNotEmpty = !tq_empty(&cond->waiters);
    	if(!tq_isNotEmpty){
    	            break;
    	        }
        tq_append(&active, tq_pop(&cond->waiters));
    }
}

/* Helper function for the POSIX replacement API - you'll need to tell
 * time in order to implement qthread_usleep. WARNING - store the
 * return value in 'unsigned long' (64 bits), not 'unsigned' (32 bits)
 */
static unsigned long get_usecs(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000000 + tv.tv_usec;
}


/* POSIX replacement API. This semester we're only implementing 'usleep'
 *
 * If there are no runnable threads, your scheduler needs to block
 * waiting for a thread blocked in 'qthread_usleep' to wake up. 
 */

/* qthread_usleep - yield to next runnable thread, making arrangements
 * to be put back on the active list after 'usecs' timeout. 
 */
void qthread_usleep(long int usecs)
{
    unsigned long int timeEnd = get_usecs() + usecs;
    unsigned long time = get_usecs();
    
    while (time<timeEnd) {
        tq_append(&sleepers, current);
        time = get_usecs();
        schedule(&current->sp);
    }
}

