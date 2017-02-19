#include "request.h"
#include "server_thread.h"
#include "common.h"
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

/*---------------queue data structure starts-----------------------*/
typedef struct Node {
    int data;
    struct Node *next;
} Node;

/*---------------queue data structure ends-----------------------*/


struct server {
    int nr_threads;
    int max_requests;
    int max_cache_size;
    /* add any other parameters you need */

    int waiting_requests;
    Node* head; /*work as the start of the queue*/

    int* wait_array; /*change the wait queue to be an array*/
    int start_ptr;
    int end_ptr;

    pthread_mutex_t lock;
    pthread_cond_t produce_cv;
    pthread_cond_t consume_cv;
};

/* initialize file data */
static struct file_data *
file_data_init(void) {
    struct file_data *data;

    data = Malloc(sizeof (struct file_data));
    data->file_name = NULL;
    data->file_buf = NULL;
    data->file_size = 0;
    return data;
}

/* free all file data */
static void
file_data_free(struct file_data *data) {
    free(data->file_name);
    free(data->file_buf);
    free(data);
}

static void
do_server_request(struct server *sv, int connfd) {
    int ret;
    struct request *rq;
    struct file_data *data;

    data = file_data_init();

    /* fills data->file_name with name of the file being requested */
    rq = request_init(connfd, data);
    if (!rq) {
        file_data_free(data);
        return;
    }
    /* reads file, 
     * fills data->file_buf with the file contents,
     * data->file_size with file size. */
    ret = request_readfile(rq);
    if (!ret)
        goto out;
    /* sends file to client */
    request_sendfile(rq);
out:
    request_destroy(rq);
    file_data_free(data);
}

/* entry point functions */

void* request_stub(void* _sv) {

    struct server* sv = (struct server*) _sv;

start:

    pthread_mutex_lock(&(sv->lock));

    for (; sv->waiting_requests == 0;) {
        pthread_cond_wait(&(sv->consume_cv), &(sv->lock));
    }

    int connfd;
    
    if (sv->waiting_requests == sv->max_requests){
        pthread_cond_signal(&(sv->produce_cv));
    }

    sv->waiting_requests--;
    connfd = sv->wait_array[sv->start_ptr];
    sv->start_ptr = (sv->start_ptr + 1) % sv->max_requests;
    /*
    if (sv->waiting_requests == 0) {
        pthread_cond_signal(&(sv->produce_cv));
    }*/

    pthread_mutex_unlock(&(sv->lock));
    do_server_request(sv, connfd);

    goto start;
    return sv;
}

struct server *
server_init(int nr_threads, int max_requests, int max_cache_size) {
    struct server *sv;

    sv = Malloc(sizeof (struct server));
    sv->nr_threads = nr_threads;
    sv->max_requests = max_requests;
    sv->max_cache_size = max_cache_size;

    if (nr_threads > 0 || max_requests > 0 || max_cache_size > 0) {

        sv->waiting_requests = 0; //queue implementation
        sv->head = NULL;

        sv->wait_array = (int*) malloc(sizeof (int) * max_requests);
        sv->start_ptr = 0;
        sv->end_ptr = 0;

        pthread_mutex_init(&(sv->lock), NULL);
        pthread_cond_init(&(sv->produce_cv), NULL);
        pthread_cond_init(&(sv->consume_cv), NULL);

        //create a list of threads
        pthread_t* thread_list = (pthread_t*) malloc(sizeof (pthread_t) * nr_threads);
        int i;
        for (i = 0; i < nr_threads; i++) {
            pthread_create(thread_list + i, NULL, request_stub, (void*) sv);
        }
    }

    /* Lab 4: create queue of max_request size when max_requests > 0 */
    /* Lab 5: init server cache and limit its size to max_cache_size */
    /* Lab 4: create worker threads when nr_threads > 0 */

    return sv;
}

void
server_request(struct server *sv, int connfd) {
    if (sv->nr_threads == 0) { /* no worker threads */
        do_server_request(sv, connfd);
    } else {

        pthread_mutex_lock(&(sv->lock));

        while (sv->waiting_requests == sv->max_requests) {
            pthread_cond_wait(&(sv->produce_cv), &(sv->lock));
        }

        if (sv->waiting_requests == 0){
            pthread_cond_signal(&(sv->consume_cv));
        }

        //adding the new request to the wait array
        sv->waiting_requests++; //increment the size of the waiting queue
        sv->wait_array[sv->end_ptr] = connfd;
        sv->end_ptr = (sv->end_ptr + 1) % sv->max_requests;

        /*
        //signal all the threads, that can start requesting
        if (sv->waiting_requests == sv->waiting_requests) {
            pthread_cond_signal(&(sv->consume_cv));
        }*/
        pthread_mutex_unlock(&(sv->lock));

    }
}
