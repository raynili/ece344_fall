#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"

typedef enum ThreadStatus{
    running,
    ready,
    killed        
}ThreadStatus;

/* This is the thread control block */
typedef struct thread {
    Tid threadid;
    ucontext_t state;
    ThreadStatus status;
    void * thread_stack;
    void * original;
}thread;

//volatile int setcontext_called = 0;

/*global variable for the thread scheduler*/
thread* thread_index[THREAD_MAX_THREADS] = { NULL };

/*whether set context has been called before*/
int setcontext_called[THREAD_MAX_THREADS] = { 0 };


/*find the first non existing thread id and assign it*/
int tid_generator(){
    int i;
    int exist = 0;
    for (i = 0; i < THREAD_MAX_THREADS; i++){
        if (thread_index[i] == NULL)
            return i;
    }
    if (exist == 0)
        return -1;
    return -1;
}
int find_running(){
    int i;
    for (i = 0; i < THREAD_MAX_THREADS; i++){
        if (thread_index[i] == NULL)
            continue;
        if (thread_index[i]->status == running)
            return i;
    }
    return THREAD_INVALID;
}
int find_ready(){
    //int curr_running = find_running();
    int i;
    for (i = 0; i < THREAD_MAX_THREADS; i++){
        if (thread_index[i] == NULL){
            continue;
        }
        if (thread_index[i]->status == ready)
            return i;
    }
    return THREAD_INVALID;
}
int free_killed(){
    
    int i;
    for (i = 0; i < THREAD_MAX_THREADS; i++){
        if (thread_index[i] == NULL)
            continue;
        if (thread_index[i]->status == killed){
            free(thread_index[i]->original);
            free(thread_index[i]);
            thread_index[i] = NULL;
        }
    }
    return 0;
}
thread dummy;
void
thread_init(void)
{
    interrupts_set(0);
    dummy.threadid = 0;
    dummy.status = running;
    thread_index[0] = &dummy;
    getcontext(&(thread_index[0]->state));
    interrupts_set(1);
}

Tid
thread_id()
{
    int i;
    for (i = 0; i < THREAD_MAX_THREADS; i++){
        if (thread_index[i] == NULL)
            continue;
        if (thread_index[i]->status == running)
            return i;
    }
    return THREAD_INVALID;
}


void thread_stub(void (*fn)(void *), void *arg){
    
    interrupts_set(1);
    
    Tid ret;
    
    fn(arg);
    ret = thread_exit();
    
    assert(ret == THREAD_NONE);
    exit(0);
    
}

Tid
thread_create(void (*fn) (void *), void *parg)
{
    int prev_interrupt_state = interrupts_set(0);
    
    int new_place = tid_generator();
    if (new_place == -1) {
        interrupts_set(prev_interrupt_state);
        return THREAD_NOMORE;
    }
    
    thread_index[new_place] = (thread *)malloc(sizeof(thread));
    if (thread_index[new_place] == NULL){
        free(thread_index[new_place]);
        thread_index[new_place] = NULL;
        interrupts_set(prev_interrupt_state);
        return THREAD_NOMEMORY;
    }
    
    thread_index[new_place]->threadid = new_place;
    thread_index[new_place]->status = ready;
    
    getcontext(&(thread_index[new_place]->state));
    assert(!interrupts_enabled());
    
    //allocate new stack
    void *new_stack = (void *)malloc(THREAD_MIN_STACK + 32);
    
    if (new_stack == NULL){
        free(new_stack);
        free(thread_index[new_place]);
        thread_index[new_place] = NULL;
        interrupts_set(prev_interrupt_state);
        return THREAD_NOMEMORY;
    }
    
    thread_index[new_place]->original = new_stack;
    
    new_stack = new_stack + THREAD_MIN_STACK + 31;
    new_stack = (void *)((long long)new_stack / 16 * 16 - 8);
    
    thread_index[new_place]->thread_stack = new_stack;
    
    setcontext_called[new_place] = 0;
    
    //pc points to program counter
    thread_index[new_place]->state.uc_mcontext.gregs[REG_RIP] = (long long int)(thread_stub);
    thread_index[new_place]->state.uc_mcontext.gregs[REG_RSP] = (long long int)new_stack;
    thread_index[new_place]->state.uc_mcontext.gregs[REG_RDI] = (long long int)(fn);
    thread_index[new_place]->state.uc_mcontext.gregs[REG_RSI] = (long long int)(parg);
    
    interrupts_set(prev_interrupt_state);
    return thread_index[new_place]->threadid;
    
}



Tid
thread_yield(Tid want_tid)
{
    int prev_interrupt_state = interrupts_set(0);
    
    int running_thread = find_running();
    
    if (want_tid == THREAD_SELF || running_thread == want_tid){
        interrupts_set(prev_interrupt_state);
        return running_thread;
    }
    
    
    
    //yield to any
    if (want_tid == THREAD_ANY){
        int ready_thread = find_ready();
        if (ready_thread < 0){
            interrupts_set(prev_interrupt_state);
            return THREAD_NONE;
        }
        getcontext(&(thread_index[running_thread]->state));
        assert(!interrupts_enabled());
        free_killed();
        if (setcontext_called[running_thread] == 0){
            setcontext_called[running_thread] = 1;
            thread_index[running_thread]->status = ready;
            thread_index[ready_thread]->status = running;
            setcontext(&(thread_index[ready_thread]->state));
        }
        else{
            setcontext_called[running_thread] = 0;
            interrupts_set(prev_interrupt_state);
            return ready_thread;
        }
    }
    
    //yield to a specific thread
    if (want_tid < 0 || want_tid >= THREAD_MAX_THREADS){
        interrupts_set(prev_interrupt_state);
        return THREAD_INVALID;
    }
    if (thread_index[want_tid] == NULL){
        interrupts_set(prev_interrupt_state);
        return THREAD_INVALID;
    }
    else{
        getcontext(&(thread_index[running_thread]->state));
        assert(!interrupts_enabled());
        free_killed();
        if (setcontext_called[running_thread] == 0){
            setcontext_called[running_thread] = 1;
            thread_index[running_thread]->status = ready;
            thread_index[want_tid]->status = running;
            setcontext(&(thread_index[want_tid]->state));
        }
        else{
            setcontext_called[running_thread] = 0;
            interrupts_set(prev_interrupt_state);
            return want_tid;
        }
    }
    interrupts_set(prev_interrupt_state);
    return THREAD_NONE;
}

Tid
thread_exit()
{
    int prev_interrupt_state = interrupts_set(0);
    int running_thread = find_running();
    if (running_thread < 0)
        interrupts_set(prev_interrupt_state);
        return THREAD_FAILED;
    
    int ready_thread = find_ready();
    if (ready_thread == THREAD_INVALID) {
        interrupts_set(prev_interrupt_state);
        return THREAD_NONE;
    }
    
    
    thread_index[running_thread]->status = killed;
    
    thread_index[ready_thread]->status = running;
    interrupts_set(prev_interrupt_state);
    setcontext(&(thread_index[ready_thread]->state));
    
    
    return THREAD_FAILED;
}

Tid
thread_kill(Tid tid)
{
    int interrupt_enabled = interrupts_set(0);
    if (tid < 0 || tid >= THREAD_MAX_THREADS) {
        interrupts_set(interrupt_enabled);
        return THREAD_INVALID;
    }
    if (thread_index[tid] == NULL) {
        interrupts_set(interrupt_enabled);
        return THREAD_INVALID;
    } 
    if (tid == find_running()){
        interrupts_set(interrupt_enabled);
        return THREAD_INVALID;
    }
        
    else{
        thread_index[tid]->status = killed;
        free(thread_index[tid]->original);
        free(thread_index[tid]);
        thread_index[tid] = NULL;
        interrupts_set(interrupt_enabled);
        return tid;
    }
    interrupts_set(interrupt_enabled);
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
