#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>

#ifndef RVF_OUTCOME
#error RVF_OUTCOME must be defined
#endif

static atomic_int x, y, z, seen_y2, seen_z3, seen_x3, seen_y4;

static void *thread_1(void *unused)
{
	atomic_store_explicit(&x, 1, memory_order_relaxed);
	atomic_store_explicit(&y, 1, memory_order_release);
	return NULL;
}

static void *thread_2(void *unused)
{
	int value = atomic_load_explicit(&y, memory_order_relaxed);
	atomic_store_explicit(&seen_y2, value, memory_order_relaxed);
	atomic_store_explicit(&z, value, memory_order_relaxed);
	return NULL;
}

static void *thread_3(void *unused)
{
	int value_z = atomic_load_explicit(&z, memory_order_relaxed);
	atomic_thread_fence(memory_order_acq_rel);
	int value_x = atomic_load_explicit(&x, memory_order_relaxed);
	atomic_store_explicit(&seen_z3, value_z, memory_order_relaxed);
	atomic_store_explicit(&seen_x3, value_x, memory_order_relaxed);
	return NULL;
}

static void *thread_4(void *unused)
{
	int value = atomic_load_explicit(&y, memory_order_acquire);
	atomic_store_explicit(&seen_y4, value, memory_order_relaxed);
	return NULL;
}

int main(void)
{
	pthread_t t1, t2, t3, t4;
	if (pthread_create(&t1, NULL, thread_1, NULL) ||
	    pthread_create(&t2, NULL, thread_2, NULL) ||
	    pthread_create(&t3, NULL, thread_3, NULL) ||
	    pthread_create(&t4, NULL, thread_4, NULL) || pthread_join(t1, NULL) ||
	    pthread_join(t2, NULL) || pthread_join(t3, NULL) || pthread_join(t4, NULL))
		abort();
	unsigned outcome = (unsigned)atomic_load_explicit(&seen_y2, memory_order_relaxed) |
		((unsigned)atomic_load_explicit(&seen_z3, memory_order_relaxed) << 1U) |
		((unsigned)atomic_load_explicit(&seen_x3, memory_order_relaxed) << 2U) |
		((unsigned)atomic_load_explicit(&seen_y4, memory_order_relaxed) << 3U);
	if (outcome == RVF_OUTCOME)
		assert(0);
	return 0;
}
