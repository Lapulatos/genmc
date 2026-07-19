#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

#ifndef RVF_ASSUME_OUTCOME
#define RVF_ASSUME_OUTCOME 0
#endif

extern void __VERIFIER_assume_internal(_Bool, char);

static atomic_int x;
static atomic_int observed;

static void *write_one_a(void *arg)
{
	(void)arg;
	atomic_store_explicit(&x, 1, memory_order_seq_cst);
	return 0;
}

static void *write_one_b(void *arg)
{
	(void)arg;
	atomic_store_explicit(&x, 1, memory_order_seq_cst);
	return 0;
}

static void *read_after_assume(void *arg)
{
	(void)arg;
	int value = atomic_load_explicit(&x, memory_order_seq_cst);
	__VERIFIER_assume_internal(value == 1, 0);
	atomic_store_explicit(&observed, 1, memory_order_seq_cst);
	return 0;
}

int main(void)
{
	pthread_t a;
	pthread_t b;
	pthread_t reader;
	pthread_create(&a, 0, write_one_a, 0);
	pthread_join(a, 0);
	pthread_create(&reader, 0, read_after_assume, 0);
	pthread_create(&b, 0, write_one_b, 0);
	pthread_join(reader, 0);
	pthread_join(b, 0);
	int result = atomic_load_explicit(&observed, memory_order_seq_cst);
	assert(result != RVF_ASSUME_OUTCOME);
	return 0;
}
