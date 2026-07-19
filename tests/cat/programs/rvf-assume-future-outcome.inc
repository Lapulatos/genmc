#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

#ifndef RVF_ASSUME_OUTCOME
#define RVF_ASSUME_OUTCOME 0
#endif

extern void __VERIFIER_assume_internal(_Bool, char);

static atomic_int x;
static atomic_int ready;
static atomic_int observed;

static void *future_reader(void *arg)
{
	(void)arg;
	atomic_store_explicit(&ready, 1, memory_order_seq_cst);
	int value = atomic_load_explicit(&x, memory_order_seq_cst);
	__VERIFIER_assume_internal(value == 1, 0);
	atomic_store_explicit(&observed, 1, memory_order_seq_cst);
	return 0;
}

static void *future_writer(void *arg)
{
	(void)arg;
	int value = atomic_load_explicit(&ready, memory_order_seq_cst);
	__VERIFIER_assume_internal(value == 1, 0);
	atomic_store_explicit(&x, 1, memory_order_seq_cst);
	return 0;
}

int main(void)
{
	pthread_t reader;
	pthread_t writer;
	pthread_create(&reader, 0, future_reader, 0);
	pthread_create(&writer, 0, future_writer, 0);
	pthread_join(reader, 0);
	pthread_join(writer, 0);
	int result = atomic_load_explicit(&observed, memory_order_seq_cst);
	assert(result != RVF_ASSUME_OUTCOME);
	return 0;
}
