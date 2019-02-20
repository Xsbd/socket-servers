/* Header file for threadpool implementation */

#ifndef THREADPOOL_H
#define THREADPOOL_H

#define MAX_THREADS 32

/* threadpool structure */
/* hide internal structure from users */
typedef void *tpool_t;

/* Creates threadpool */
tpool_t tpool_create(int n_threads);

/* Dispatch jobs immediately if thread limit not reached
 * blocks when all threads in pools are busy */
typedef void (*dispatch_fn)(void *);
void tpool_dispatch(tpool_t tp, dispatch_fn d_func,
		void *arg);

/* Destroys the threadpool */
void tpool_destroy(tpool_t destroyme);

#endif /* THREADPOOL_H */
