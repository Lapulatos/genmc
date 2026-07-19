#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

#ifndef RVF_ASSUME_OUTCOME
#define RVF_ASSUME_OUTCOME 0
#endif

extern void __VERIFIER_assume_internal(_Bool, char);

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_int x = 1;
static atomic_int observed;

static void *mutex_reader(void *arg)
{
	(void)arg;
	pthread_mutex_lock(&lock);
	int value = atomic_load_explicit(&x, memory_order_seq_cst);
	__VERIFIER_assume_internal(value == 1, 0);
	atomic_store_explicit(&observed, 1, memory_order_seq_cst);
	pthread_mutex_unlock(&lock);
	return 0;
}

int main(void)
{
	pthread_t reader;
	pthread_create(&reader, 0, mutex_reader, 0);
	pthread_join(reader, 0);
	int result = atomic_load_explicit(&observed, memory_order_seq_cst);
	assert(result != RVF_ASSUME_OUTCOME);
	return 0;
}
