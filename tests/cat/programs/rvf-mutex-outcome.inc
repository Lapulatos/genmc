#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

#ifndef RVF_MUTEX_OUTCOME
#error "RVF_MUTEX_OUTCOME must be defined"
#endif

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static atomic_int value;
static atomic_int observed;

static void *writer(void *unused)
{
	(void)unused;
	pthread_mutex_lock(&mutex);
	atomic_store_explicit(&value, 1, memory_order_seq_cst);
	pthread_mutex_unlock(&mutex);
	return 0;
}

static void *reader(void *unused)
{
	(void)unused;
	pthread_mutex_lock(&mutex);
	int result = atomic_load_explicit(&value, memory_order_seq_cst);
	pthread_mutex_unlock(&mutex);
	atomic_store_explicit(&observed, result, memory_order_seq_cst);
	return 0;
}

int main(void)
{
	pthread_t first;
	pthread_t second;
	pthread_t third;
	pthread_create(&first, 0, writer, 0);
	pthread_create(&second, 0, writer, 0);
	pthread_create(&third, 0, reader, 0);
	pthread_join(first, 0);
	pthread_join(second, 0);
	pthread_join(third, 0);
	assert(atomic_load_explicit(&observed, memory_order_seq_cst) != RVF_MUTEX_OUTCOME);
	return 0;
}
