#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

#ifndef RVF_ASSUME_OUTCOME
#define RVF_ASSUME_OUTCOME 0
#endif

extern void __VERIFIER_assume_internal(_Bool, char);

static atomic_int x = 1;
static atomic_int y = 1;
static atomic_int observed;

static void *double_reader(void *arg)
{
	(void)arg;
	int first = atomic_load_explicit(&x, memory_order_seq_cst);
	__VERIFIER_assume_internal(first == 1, 0);
	int second = atomic_load_explicit(&y, memory_order_seq_cst);
	__VERIFIER_assume_internal(second == 1, 0);
	atomic_store_explicit(&observed, 1, memory_order_seq_cst);
	return 0;
}

int main(void)
{
	pthread_t reader;
	pthread_create(&reader, 0, double_reader, 0);
	pthread_join(reader, 0);
	int result = atomic_load_explicit(&observed, memory_order_seq_cst);
	assert(result != RVF_ASSUME_OUTCOME);
	return 0;
}
