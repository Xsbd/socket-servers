/* thradpool implementaion */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "threadpool.h"

/* job struct */
typedef struct work_st{
	void (*routine) (void*);
	void * arg;
	struct work_st* next;
} work_t;

/* Internal representation of threadpool */
/* cast to type "tpool_t" before it given out to callers */
typedef struct _threadpool_st {
	int num_threads;		/* number of threads */
	int qsize;			/* queue size */
	pthread_t *threads;		/* ptr to threads */
	work_t* qhead;			/* queue head ptr */
	work_t* qtail;			/* queue tail ptr */
	pthread_mutex_t qlock;		/* mutex lock to use on queue */
	pthread_cond_t q_not_empty;	/* check queue emptiness */
	pthread_cond_t q_empty;
	int shutdown;
	int dont_accept;
} _threadpool;

/* Thread pool queue management */
void* do_work(tpool_t p) {
	_threadpool * pool = (_threadpool *) p;
	work_t* cur;
	int k;

	/* selecting job from the queue for current thread */
	while(1) {
		pool->qsize = pool->qsize;
		pthread_mutex_lock(&(pool->qlock));	/* lock critical section */

		while( pool->qsize == 0) {		/* nothing in queue */
			if(pool->shutdown) {		/* thread pool is shutdown, unlock and exit thread */
				pthread_mutex_unlock(&(pool->qlock));
				pthread_exit(NULL);
			}
			/* give up lock and wait for something to arrive in queue */
			pthread_mutex_unlock(&(pool->qlock)); 
			pthread_cond_wait(&(pool->q_not_empty),&(pool->qlock));

			/* check for shutdown condition */
			if(pool->shutdown) {
				pthread_mutex_unlock(&(pool->qlock));
				pthread_exit(NULL);
			}
		}

		cur = pool->qhead;		/* select job at head of queue */

		/* remove it from queue since it will be dispached */
		pool->qsize--;
		if(pool->qsize == 0) {
			pool->qhead = NULL;
			pool->qtail = NULL;
		}
		else {
			pool->qhead = cur->next;
		}

		if(pool->qsize == 0 && ! pool->shutdown) {	/* signal that queue is empty */
			pthread_cond_signal(&(pool->q_empty));
		}
		pthread_mutex_unlock(&(pool->qlock));		/* free the lock */
		(cur->routine) (cur->arg);			/* perform the task */
		free(cur);				/* free the memory associated with completed task */
	}
}

/* Thread pool creation */
tpool_t tpool_create(int n_threads) {
	_threadpool *pool;
	int i;

	/* Can't create threads more than a max */
	if ((n_threads <= 0) || (n_threads > MAX_THREADS))
		return NULL;

	/* Allocate memory for threadpool */
	pool = (_threadpool *) malloc(sizeof(_threadpool));
	if (pool == NULL) {
		fprintf(stderr, "Not enough memory to create threadpool!\n");
		return NULL;
	}
	pool->threads = (pthread_t*) malloc (sizeof(pthread_t) * n_threads);
	if(!pool->threads) {
		fprintf(stderr, "Not enough memory to create threadpool!\n");
		return NULL;	
	}

	/* Populate the threadpool structure */
	pool->num_threads = n_threads; 
	pool->qsize = 0;
	pool->qhead = NULL;
	pool->qtail = NULL;
	pool->shutdown = 0;
	pool->dont_accept = 0;

	/* initialize mutex and condition variables. */
	if(pthread_mutex_init(&pool->qlock,NULL)) {
		fprintf(stderr, "Mutex initiation error!\n");
		return NULL;
	}
	if(pthread_cond_init(&(pool->q_empty),NULL)) {
		fprintf(stderr, "CV initiation error!\n");	
		return NULL;
	}
	if(pthread_cond_init(&(pool->q_not_empty),NULL)) {
		fprintf(stderr, "CV initiation error!\n");	
		return NULL;
	}

	/* make threads */
	for (i = 0;i < n_threads; i++) {
		if(pthread_create(&(pool->threads[i]),NULL,do_work,pool)) {
			fprintf(stderr, "Thread initiation error!\n");	
			return NULL;		
		}
	}
	return (tpool_t)pool;
}

/* Dispatching jobs to the job queue */
void tpool_dispatch(tpool_t tpool, dispatch_fn d_func, void *arg) {
	_threadpool *pool = (_threadpool *) tpool;
	work_t * cur;
	int k;

	k = pool->qsize;

	/* creating work structure */
	cur = (work_t*) malloc(sizeof(work_t));
	if(cur == NULL) {
		fprintf(stderr, "Out of memory creating a work struct!\n");
		return;	
	}

	/* populating the structure */
	cur->routine = d_func;
	cur->arg = arg;
	cur->next = NULL;

	pthread_mutex_lock(&(pool->qlock));	/* lock before accessing thread pool */

	if(pool->dont_accept) {			/* pool configured not to accept any more */
		free(cur);
		return;
	}
	if(pool->qsize == 0) {			/* dispatch immediately if queue empty */
		pool->qhead = cur;
		pool->qtail = cur;
		pthread_cond_signal(&(pool->q_not_empty));	/* signal queue empty */
	} else {				/* put at end of queue if threads all used */
		pool->qtail->next = cur;
		pool->qtail = cur;			
	}
	pool->qsize++;
	pthread_mutex_unlock(&(pool->qlock));	/* release lock */
}

/* Destroy the threadpool */
void tpool_destroy(tpool_t destroyme) {
	_threadpool *pool = (_threadpool *) destroyme;
	void* nothing;
	int i = 0;

	free(pool->threads);

	pthread_mutex_destroy(&(pool->qlock));
	pthread_cond_destroy(&(pool->q_empty));
	pthread_cond_destroy(&(pool->q_not_empty));
	return;
}
