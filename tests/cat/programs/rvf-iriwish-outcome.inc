#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>

#ifndef RVF_IRIWISH_OUTCOME
#error RVF_IRIWISH_OUTCOME must be defined
#endif

static atomic_int x;
static atomic_int y;
static atomic_int t2_x;
static atomic_int t3_x;
static atomic_int t3_y;
static atomic_int t4_y;
static atomic_int t4_x;

#ifdef RVF_IRIWISH_WITH_MUTEX
static pthread_mutex_t rvf_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static void *thread_1(void *unused)
{
	atomic_store_explicit(&x, 1, memory_order_relaxed);
	return NULL;
}

static void *thread_2(void *unused)
{
	int r1 = atomic_load_explicit(&x, memory_order_relaxed);
	atomic_store_explicit(&t2_x, r1, memory_order_relaxed);
	atomic_store_explicit(&y, r1, memory_order_release);
	return NULL;
}

static void *thread_3(void *unused)
{
	int r1 = atomic_load_explicit(&x, memory_order_relaxed);
	atomic_thread_fence(memory_order_acq_rel);
	int r2 = atomic_load_explicit(&y, memory_order_relaxed);
	atomic_store_explicit(&t3_x, r1, memory_order_relaxed);
	atomic_store_explicit(&t3_y, r2, memory_order_relaxed);
	return NULL;
}

static void *thread_4(void *unused)
{
	int r1 = atomic_load_explicit(&y, memory_order_relaxed);
	atomic_thread_fence(memory_order_acq_rel);
	int r2 = atomic_load_explicit(&x, memory_order_relaxed);
	atomic_store_explicit(&t4_y, r1, memory_order_relaxed);
	atomic_store_explicit(&t4_x, r2, memory_order_relaxed);
	return NULL;
}

int main(void)
{
#ifdef RVF_IRIWISH_WITH_MUTEX
	pthread_mutex_lock(&rvf_mutex);
	pthread_mutex_unlock(&rvf_mutex);
#endif
	pthread_t threads[4];
	if (pthread_create(&threads[0], NULL, thread_1, NULL) ||
	    pthread_create(&threads[1], NULL, thread_2, NULL) ||
	    pthread_create(&threads[2], NULL, thread_3, NULL) ||
	    pthread_create(&threads[3], NULL, thread_4, NULL))
		abort();
	if (pthread_join(threads[0], NULL) || pthread_join(threads[1], NULL) ||
	    pthread_join(threads[2], NULL) || pthread_join(threads[3], NULL))
		abort();

	unsigned outcome = (unsigned)atomic_load_explicit(&t2_x, memory_order_relaxed) |
		((unsigned)atomic_load_explicit(&t3_x, memory_order_relaxed) << 1U) |
		((unsigned)atomic_load_explicit(&t3_y, memory_order_relaxed) << 2U) |
		((unsigned)atomic_load_explicit(&t4_y, memory_order_relaxed) << 3U) |
		((unsigned)atomic_load_explicit(&t4_x, memory_order_relaxed) << 4U);
	if (outcome == RVF_IRIWISH_OUTCOME)
		assert(0);
	return 0;
}
