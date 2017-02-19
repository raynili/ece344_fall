#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"

typedef enum ThreadStatus{
    running,
    ready,
    sleep,        
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

/*global variable for the thread scheduler*/
int curr_running;
thread* thread_index[THREAD_MAX_THREADS] = { NULL };

//testing functions that count created threads
int running_thread_num(){
    int i;
    int count = 0;
    for (i = 0; i < THREAD_MAX_THREADS; i++){
        if (thread_index[i] != NULL){
            count++;
        }
    }
    return count;
}

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
    return curr_running;
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
    
    int i = (curr_running + 1) % THREAD_MAX_THREADS;
    int j;
    for (j = 0; j < THREAD_MAX_THREADS; j++){
        if (thread_index[i] == NULL){
            i = (i + 1 + THREAD_MAX_THREADS) % THREAD_MAX_THREADS;
            continue;
        }
        
        if (thread_index[i]->status == ready){
            return i;
        }
        else
            i = (i + 1 + THREAD_MAX_THREADS) % THREAD_MAX_THREADS;
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
    int interrupts_status = interrupts_set(0);
    curr_running = 0;
    dummy.threadid = 0;
    dummy.status = running;
    thread_index[0] = &dummy;
    getcontext(&(thread_index[0]->state));
    interrupts_set(interrupts_status);
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
    int interrupts_status = interrupts_set(0);
    int new_place = tid_generator();
    
    if (new_place == -1) {
        interrupts_set(interrupts_status);
        return THREAD_NOMORE;
    }
    
    thread_index[new_place] = (thread *)malloc(sizeof(thread));
    if (thread_index[new_place] == NULL){
        free(thread_index[new_place]);
        thread_index[new_place] = NULL;
        interrupts_set(interrupts_status);
        return THREAD_NOMEMORY;
    }
    
    thread_index[new_place]->threadid = new_place;
    thread_index[new_place]->status = ready;
    getcontext(&(thread_index[new_place]->state));
    
    //allocate new stack
    void *new_stack = (void *)malloc(THREAD_MIN_STACK + 32);
    
    if (new_stack == NULL){
        free(new_stack);
        free(thread_index[new_place]);
        thread_index[new_place] = NULL;
        interrupts_set(interrupts_status);
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
    
    interrupts_set(interrupts_status);
    return thread_index[new_place]->threadid;
    
}



Tid
thread_yield(Tid want_tid)
{
    int interrupts_status = interrupts_set(0);
    
    int running_thread = find_running();
    
    if (want_tid == THREAD_SELF || running_thread == want_tid){
        interrupts_set(interrupts_status);
        return running_thread;
    }
    //yield to any
    if (want_tid == THREAD_ANY){
        int ready_thread = find_ready();
        if (ready_thread < 0){
            interrupts_set(interrupts_status);
            return THREAD_NONE;
        }
        getcontext(&(thread_index[running_thread]->state));
        free_killed();
        if (setcontext_called[running_thread] == 0){
            setcontext_called[running_thread] = 1;
            thread_index[running_thread]->status = ready;
            thread_index[ready_thread]->status = running;
            curr_running = ready_thread;
            setcontext(&(thread_index[ready_thread]->state));
        }
        else{
            assert(!interrupts_enabled());
            setcontext_called[running_thread] = 0;
            interrupts_set(interrupts_status);
            return ready_thread;
        }
    }
    
    //yield to a specific thread
    if (want_tid < 0 || want_tid >= THREAD_MAX_THREADS){
        interrupts_set(interrupts_status);
        return THREAD_INVALID;
    }
    if (thread_index[want_tid] == NULL){
        interrupts_set(interrupts_status);
        return THREAD_INVALID;
    }
    else{
        getcontext(&(thread_index[running_thread]->state));
        free_killed();
        if (setcontext_called[running_thread] == 0){
            setcontext_called[running_thread] = 1;
            thread_index[running_thread]->status = ready;
            thread_index[want_tid]->status = running;
            curr_running = want_tid;
            setcontext(&(thread_index[want_tid]->state));
        }
        else{
            setcontext_called[running_thread] = 0;
            interrupts_set(interrupts_status);
            return want_tid;
        }
    }
    
    interrupts_set(interrupts_status);
    return THREAD_NONE;
}


Tid
thread_exit()
{
    int interrupts_status = interrupts_set(0);
    
    int running_thread = find_running();
    if (running_thread < 0){
        interrupts_set(interrupts_status);
        return THREAD_FAILED;
    }
        
    int ready_thread = find_ready();
    if (ready_thread == THREAD_INVALID) {
        interrupts_set(interrupts_status);
        return THREAD_NONE;
    }
    
    
    thread_index[running_thread]->status = killed;
    
    thread_index[ready_thread]->status = running;
    curr_running = ready_thread;
    
    
    setcontext(&(thread_index[ready_thread]->state));
    
    interrupts_set(interrupts_status);
    return THREAD_FAILED;
}

Tid
thread_kill(Tid tid)
{
    if (tid < 0 || tid >= THREAD_MAX_THREADS) {
        return THREAD_INVALID;
    }
    if (thread_index[tid] == NULL) {
        return THREAD_INVALID;
    } 
    if (tid == find_running()){
        return THREAD_INVALID;
    }
        
    else{
        int interrupts_status = interrupts_set(0);
        thread_index[tid]->status = killed;
        free(thread_index[tid]->original);
        free(thread_index[tid]);
        thread_index[tid] = NULL;
        interrupts_set(interrupts_status);
        return tid;
    }
    
    return THREAD_FAILED;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

typedef struct Node{
    Tid thread_id;
    struct Node *next;
}Node;

/* This is the wait queue structure */
struct wait_queue {
    Node *first;
    Node *last;
};

void printqueue(struct wait_queue *wq){
    Node *curr = wq->first;
    while (curr != NULL){
        printf("%d ", curr->thread_id);
        printf("%p ", &thread_index[curr->thread_id]->state);
        printf("\n");
        curr = curr->next;
    }
    printf("\n");
}

struct wait_queue *
wait_queue_create()
{
    struct wait_queue *wq;

    wq = malloc(sizeof(struct wait_queue));
    assert(wq);

    wq->first = NULL;
    wq->last = NULL;
    return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{
    
    while(wq->first != NULL){
        Node *curr = wq->first;
        wq->first = wq->first->next;
        free(curr);
    }
    free(wq);
}

void push_ele(struct wait_queue *wq, Node *new_ele){
    assert(!interrupts_enabled());
    
    //printqueue(wq);
    
    if (wq->first == NULL){
        wq->first = new_ele;
        wq->last = new_ele;
        return;
    }
    else{
        wq->last->next = new_ele;
        wq->last = new_ele;
        return;
    }
}

//make the assumptions that there are no negative values in the queue
int pop_ele(struct wait_queue *wq){
    assert(!interrupts_enabled());
    if (wq->first == NULL){
        return -1;  //return an invalid number if the queue is empty
    }
    
    Node *first_node = wq->first;
    wq->first = first_node->next;
    int result = first_node->thread_id;
    free(first_node);
    return result;
}

Tid
thread_sleep(struct wait_queue *queue)
{
    
    int interrupts_status = interrupts_set(0);
    //check if sleep the current thread is a valid move
    if (queue == NULL){
        interrupts_set(interrupts_status);
        return THREAD_INVALID;  
    }
    //no other thread available for running
    int any_ready_thread = find_ready();
    assert(curr_running != any_ready_thread);
    if (any_ready_thread == THREAD_INVALID){
        interrupts_set(interrupts_status);
        return THREAD_NONE;
    }
    
    
    //suspend the current running thread and then start to run a ready thread
    //push to the waiting list
    thread_index[curr_running]->status = sleep;
    Node *new_sleep_node = (Node *)malloc(sizeof(Node));
    new_sleep_node->thread_id = curr_running;
    new_sleep_node->next = NULL;
    push_ele(queue, new_sleep_node);
    
    assert(!interrupts_enabled());
    getcontext(&(thread_index[curr_running]->state));
    assert(!interrupts_enabled());
    //free_killed();
    
    if (setcontext_called[curr_running] == 0){
        setcontext_called[curr_running] = 1;
        thread_index[any_ready_thread]->status = running;
        curr_running = any_ready_thread;
        assert(!interrupts_enabled());
        setcontext(&(thread_index[any_ready_thread]->state));
    }
    else{
        setcontext_called[curr_running] = 0;
        interrupts_set(interrupts_status);
        return any_ready_thread;
    }
    
    interrupts_set(interrupts_status);
    return THREAD_FAILED;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
    int interrupts_status = interrupts_set(0);
    //empty not valid or queue empty
    if (queue == NULL){
        interrupts_set(interrupts_status);
        return 0;
    }
    if (queue->first == NULL){
        interrupts_set(interrupts_status);
        return 0;
    }
    
    //check the condition of all
    if (all == 0){      //pop only one element
        int sleep_thread_id = pop_ele(queue);
        assert(sleep_thread_id != -1);
        thread_index[sleep_thread_id]->status = ready;
        interrupts_set(interrupts_status);
        return 1;
    }
    else if (all == 1){ //pop all the elements to make the wait queue empty
        int count = 0;
        while (queue->first != NULL){
            int sleep_thread_id = pop_ele(queue);
            assert(sleep_thread_id != -1);
            thread_index[sleep_thread_id]->status = ready;
            count++;
        }
        assert(!(queue->first));
        interrupts_set(interrupts_status);
        return count;
    }
    else{
        interrupts_set(interrupts_status);
        return 0;
    }
    
    interrupts_set(interrupts_status);
    return 0;
}

/*the possibilities of lock status*/
typedef enum LockStatus{
    LOCKED,
    FREE
}LockStatus;


struct lock {
    volatile LockStatus lockstatus;
    volatile Tid locked_tid;
    struct wait_queue *lock_wait_queue;
};

struct lock *
lock_create()
{
	struct lock *lock;

	lock = malloc(sizeof(struct lock));
	assert(lock);

    lock->lockstatus = FREE;
    lock->locked_tid = -1;      //given a meaningless value
    lock->lock_wait_queue = wait_queue_create();

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);

    wait_queue_destroy(lock->lock_wait_queue);
	free(lock);

    return;
}

void
lock_acquire(struct lock *lock)
{
    int interrupts_status = interrupts_set(0);

    assert(lock != NULL); //checking if the lock is available

    while(lock->lockstatus == LOCKED){
        assert(!interrupts_enabled());
        thread_sleep(lock->lock_wait_queue);
    }
    lock->lockstatus = LOCKED;
    lock->locked_tid = curr_running;

    interrupts_set(interrupts_status);

    return;
}

void
lock_release(struct lock *lock)
{
    int interrupts_status = interrupts_set(0);
	assert(lock != NULL);

    lock->lockstatus = FREE;
    lock->locked_tid = -1;
    thread_wakeup(lock->lock_wait_queue, 1);

    interrupts_set(interrupts_status);
}


/*data structure for the conditional variable*/
struct cv {
    struct wait_queue *cv_wq;
};

struct cv *
cv_create()
{
	struct cv *cv;

	cv = malloc(sizeof(struct cv));
	assert(cv);

    cv->cv_wq = wait_queue_create();

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);
    wait_queue_destroy(cv->cv_wq);
        
	free(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
    int interrupts_status = interrupts_set(0);

	assert(cv != NULL);
	assert(lock != NULL);

    if (lock->lockstatus == LOCKED && lock->locked_tid == curr_running){
        lock_release(lock);
        thread_sleep(cv->cv_wq);
        lock_acquire(lock);
    }
    else{
        printf("not expectednot expectednot expectednot expected\n");
    }
    interrupts_set(interrupts_status);
    return;
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
    int interrupts_status = interrupts_set(0);

	assert(cv != NULL);
	assert(lock != NULL);

    if (lock->locked_tid == curr_running && lock->lockstatus == LOCKED){
        thread_wakeup(cv->cv_wq, 0);
    }

    interrupts_set(interrupts_status);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
    int interrupts_status = interrupts_set(0);

	assert(cv != NULL);
	assert(lock != NULL);

    if (lock->locked_tid == curr_running && lock->lockstatus == LOCKED){
        thread_wakeup(cv->cv_wq, 1);
    }
    
    interrupts_set(interrupts_status);
	
}
