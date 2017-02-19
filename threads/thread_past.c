#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"

typedef enum ThreadStatus{
    run,
    ready,
    exited
}ThreadStatus;

/* This is the thread control block */
typedef struct thread {
    Tid threadid;
    ucontext_t state;
    ThreadStatus status;
    void * thread_stack;
}thread;

typedef struct Linker{
    thread *data;
    struct Linker *next;
}Linker;


/*queue data structure*/
typedef struct Queue{
    Linker *first;
    Linker *last;
    int size;
}Queue;


/*functions involving queue*/
Queue* new_queue(){
    Queue *new_q = (Queue *)malloc(sizeof(Queue));
    new_q->first = NULL;
    new_q->last = NULL;
    new_q->size = 0;
    return new_q;
}

/*push new_ele into m_queue*/
int push_queue(Queue *m_queue, thread *new_data){
    //there is an invalid input
    if (new_data == NULL)
        return -1;
    
    Linker *new_ele = (Linker *)malloc(sizeof(Linker));
    new_ele->data = new_data;
    new_ele->next = NULL;
    
    //there is nothing in the list
    if (m_queue->size == 0){
        m_queue->first = new_ele;
        m_queue->last = new_ele;
        m_queue->size++;
        return 1;
    }
    
    //if there is something in the queue, push new_ele to the end of the queue
    m_queue->last->next = new_ele;
    m_queue->last = m_queue->last->next;
    m_queue->size++;
    return 1;
}

/*pop the first element's data into the queue*/
thread* pop_queue(Queue *m_queue){
    //there is no element in the list
    if (m_queue->size == 0)
        return NULL;
    
    thread* first_data = m_queue->first->data;
    Linker *first_ele = m_queue->first;
    m_queue->first = m_queue->first->next;
    m_queue->size--;
    free(first_ele);
    return first_data;
}

/*destroy all the elements in the queue*/
void destroy_queue(Queue *m_queue){
    if (m_queue->size == 0)
        return;
    
    while (m_queue->size > 0){
        Linker *second = m_queue->first->next;
        m_queue->first->next = NULL;
        free(m_queue->first->data->thread_stack);
        free(m_queue->first->data);
        free(m_queue->first);
        m_queue->size--;
        m_queue->first = second;
    }
    free(m_queue);
}

/*find the required tid and take it out from the queue*/
thread* find_element(Queue *m_queue, Tid target_tid){
    Linker *prev = NULL;
    Linker *curr = m_queue->first;
    int found = 0;
    while (curr != NULL){
        if (curr->data == NULL)
            break;
        if (curr->data->threadid == target_tid){
            found = 1;
            break;
        }
        else {
            prev = curr;
            curr = curr->next;
        }
    }
    
    if (found == 0)     return NULL;
    else {
        thread *result = curr->data;
        m_queue->size--;
        if (curr == m_queue->first) {
            if (m_queue->size == 1){
                m_queue->first = NULL;
                m_queue->last = NULL;
                return result;
            }
            m_queue->first = curr->next;
            free(curr);
            return result;
        }
        else if (curr == m_queue->last) {
            m_queue->last = prev;
            free(curr);
            return result;
        }
        else{
            prev->next = curr->next;
            free(curr);
            return result;
        }
    }
}


/*global variable for the thread scheduler*/
Queue *ready_queue = NULL;          //the ready queue
Queue *exit_queue = NULL;           //the exit queue
thread *running_thread = NULL;      //pointer pointing to the running thread

int threadid_list[THREAD_MAX_THREADS] = {0};
thread* thread_index[THREAD_MAX_THREADS] = { NULL };

/*find the first non existing thread id and assign it*/
int tid_generator(){
    int i;
    for (i = 0; i < THREAD_MAX_THREADS; i++){
        if (threadid_list[i] == 0){
            threadid_list[i] = 1;
            return i;
        }
    }
    return -1;
}


void
thread_init(void)
{
    ready_queue = new_queue();
    exit_queue = new_queue();
    running_thread = (thread *)malloc(sizeof(thread));
    running_thread->threadid = 0;
    running_thread->status = run;
    getcontext(&(running_thread->state));
    threadid_list[running_thread->threadid] = 1;
    thread_index[running_thread->threadid] = running_thread;
}

Tid
thread_id()
{
    if (running_thread != NULL)
        return running_thread->threadid;
    else
        return THREAD_INVALID;
}

int destroy_trd(thread *curr_trd){
    if (curr_trd == NULL)
        return -1;
    else{
        free(curr_trd->thread_stack);
        free(curr_trd);
        return 1;
    }
}

void thread_stub(void (*fn)(void *), void *arg){
    
    Tid ret;
    
    fn(arg);
    ret = thread_exit();
    
    assert(ret == THREAD_NONE);
    exit(0);
    
}

Tid
thread_create(void (*fn) (void *), void *parg)
{
    //reach max
    Tid new_tid = tid_generator();
    if (new_tid == -1)  return THREAD_NOMORE;
    
    //no more mem
    thread *new_trd = (thread *)malloc(sizeof(thread));
    if (new_trd == NULL)    return THREAD_NOMEMORY;
    
    new_trd->threadid = new_tid;
    new_trd->status = ready;
    getcontext(&(new_trd->state));
    
    //allocate new stack
    void *new_stack = (void *)malloc(3 * THREAD_MIN_STACK);
    if (new_stack == NULL){
        free(new_stack);
        free(new_trd);
        return THREAD_NOMEMORY;
    }
    new_trd->thread_stack = new_stack;
    new_stack += 3 * THREAD_MIN_STACK;
    
    new_stack = new_stack - ((unsigned long long)new_stack % (unsigned long long)16);
    new_stack -= (unsigned long long)8;
    
    //make sure that the stack is aligned to 16
    unsigned long long sp = (unsigned long long)new_stack;
    
    
    //pc points to program counter
    new_trd->state.uc_mcontext.gregs[REG_RIP] = (unsigned long long)(&thread_stub);
    new_trd->state.uc_mcontext.gregs[REG_RSP] = sp;
    new_trd->state.uc_mcontext.gregs[REG_RDI] = (unsigned long long)(fn);
    new_trd->state.uc_mcontext.gregs[REG_RSI] = (unsigned long long)(parg);
    
    push_queue(ready_queue, new_trd);
    
    return new_trd->threadid;
    
}

Tid
thread_yield(Tid want_tid)
{
    destroy_queue(exit_queue);
    
    int err = getcontext(&(running_thread->state));
    assert(!err);
    
    unsigned long long *cur_rbp = (unsigned long long*)(running_thread->state.uc_mcontext.gregs[REG_RBP]);
    unsigned long long old_rbp = *cur_rbp;
    unsigned long long old_rip = *(cur_rbp + 1);
    unsigned long long *old_rsp = cur_rbp + 2;
    
    running_thread->state.uc_mcontext.gregs[REG_RBP] = (unsigned long long)old_rbp;
    running_thread->state.uc_mcontext.gregs[REG_RIP] = (unsigned long long)old_rip;
    running_thread->state.uc_mcontext.gregs[REG_RSP] = (unsigned long long)old_rsp;
    if (running_thread->status == exited)
        running_thread->status = ready;
    
    if (want_tid == THREAD_SELF || want_tid == running_thread->threadid){
        err = setcontext(&(running_thread->state));
        return running_thread->threadid;
    }
    
    if (want_tid == THREAD_ANY){
        //The ready queue is empty
        if (ready_queue->size == 0) return THREAD_NONE;
        else{       //run the first element in the queue
            if (running_thread->status == exited)
                push_queue(exit_queue, running_thread);
            running_thread = pop_queue(ready_queue);
            running_thread->status = run;
            err = setcontext(&(running_thread->state));
            return running_thread->threadid;
        }
    }
    
    //now want_tid should indicate an id
    if (want_tid < 0 || want_tid >= THREAD_MAX_THREADS) return THREAD_INVALID;
    else if (threadid_list[want_tid] != 1) return THREAD_INVALID;
    else {
        
        thread* to_be_run_trd = find_element(ready_queue, want_tid);
        if (to_be_run_trd == NULL) {//the tid is in the exit_queue, destroy the exit_queue entirely
            //destroy_queue(exit_queue);
            return running_thread->threadid;
        }
        push_queue(ready_queue, running_thread);
        running_thread = to_be_run_trd;
        running_thread->status = run;
        return running_thread->threadid;
    }
    
    
    return THREAD_FAILED;
}

Tid
thread_exit()
{
    if (ready_queue->size == 0)  return THREAD_NONE;
    else {
        threadid_list[running_thread->threadid] = 0;    //making the trd_id available again
        Tid deleted_id = running_thread->threadid;
        running_thread->status = exited;
        push_queue(exit_queue, running_thread);
        thread_yield(THREAD_ANY);
        
        return deleted_id;
    }
    
    return THREAD_FAILED;
}

Tid
thread_kill(Tid tid)
{
    if (tid < 0 || tid >= THREAD_MAX_THREADS)   return THREAD_INVALID;
    if (threadid_list[tid] == 0)                return THREAD_INVALID;
    
    threadid_list[tid] = 0;
    thread *killed_trd = find_element(ready_queue, tid);
    if (killed_trd == NULL){
        if (running_thread->threadid == tid){
            thread *to_be_killed = running_thread;
            running_thread = pop_queue(ready_queue);
            destroy_trd(to_be_killed);
            return tid;
        }
        return THREAD_INVALID;
    }
        
    else{
        destroy_trd(killed_trd);
        //push_queue(exit_queue, killed_trd);
        return tid;
    }
    
    return THREAD_FAILED;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* This is the wait queue structure */
struct wait_queue {
	/* ... Fill this in ... */
};

struct wait_queue *
wait_queue_create()
{
	struct wait_queue *wq;

	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	TBD();

	return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{
	TBD();
	free(wq);
}

Tid
thread_sleep(struct wait_queue *queue)
{
	TBD();
	return THREAD_FAILED;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
	TBD();
	return 0;
}

struct lock {
	/* ... Fill this in ... */
};

struct lock *
lock_create()
{
	struct lock *lock;

	lock = malloc(sizeof(struct lock));
	assert(lock);

	TBD();

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);

	TBD();

	free(lock);
}

void
lock_acquire(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

void
lock_release(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

struct cv {
	/* ... Fill this in ... */
};

struct cv *
cv_create()
{
	struct cv *cv;

	cv = malloc(sizeof(struct cv));
	assert(cv);

	TBD();

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);

	TBD();

	free(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}
