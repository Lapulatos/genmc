#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>

#ifndef RVF_OUTCOME
#error RVF_OUTCOME must be defined
#endif

static atomic_int x, y, seen_x2, seen_y3, seen_x3;

static void *thread_1(void *unused)
{
	atomic_store_explicit(&x, 1, memory_order_relaxed);
	return NULL;
}

static void *thread_2(void *unused)
{
	int value = atomic_load_explicit(&x, memory_order_relaxed);
	atomic_store_explicit(&seen_x2, value, memory_order_relaxed);
	atomic_store_explicit(&y, value, memory_order_relaxed);
	return NULL;
}

static void *thread_3(void *unused)
{
	int value_y = atomic_load_explicit(&y, memory_order_relaxed);
	atomic_thread_fence(memory_order_acq_rel);
	int value_x = atomic_load_explicit(&x, memory_order_relaxed);
	atomic_store_explicit(&seen_y3, value_y, memory_order_relaxed);
	atomic_store_explicit(&seen_x3, value_x, memory_order_relaxed);
	return NULL;
}

int main(void)
{
	pthread_t t1, t2, t3;
	if (pthread_create(&t1, NULL, thread_1, NULL) ||
	    pthread_create(&t2, NULL, thread_2, NULL) ||
	    pthread_create(&t3, NULL, thread_3, NULL) ||
	    pthread_join(t1, NULL) || pthread_join(t2, NULL) || pthread_join(t3, NULL))
		abort();
	unsigned outcome = (unsigned)atomic_load_explicit(&seen_x2, memory_order_relaxed) |
		((unsigned)atomic_load_explicit(&seen_y3, memory_order_relaxed) << 1U) |
		((unsigned)atomic_load_explicit(&seen_x3, memory_order_relaxed) << 2U);
	if (outcome == RVF_OUTCOME)
		assert(0);
	return 0;
}
