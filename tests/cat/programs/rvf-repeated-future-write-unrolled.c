#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

static atomic_int x, y, r0, r1;

#define STEP0()                                                                            \
	do {                                                                                 \
		atomic_store_explicit(&x, 1, memory_order_seq_cst);                            \
		atomic_store_explicit(&r0, atomic_load_explicit(&x, memory_order_seq_cst),     \
				      memory_order_seq_cst);                                      \
		atomic_store_explicit(&x, 2, memory_order_seq_cst);                            \
	} while (0)
#define STEP1()                                                                            \
	do {                                                                                 \
		atomic_store_explicit(&y, 1, memory_order_seq_cst);                            \
		atomic_store_explicit(&r1, atomic_load_explicit(&x, memory_order_seq_cst),     \
				      memory_order_seq_cst);                                      \
		atomic_store_explicit(&x, 2, memory_order_seq_cst);                            \
	} while (0)

static void *thread0(void *arg)
{
	STEP0();
	STEP0();
	return arg;
}

static void *thread1(void *arg)
{
	STEP1();
	STEP1();
	return arg;
}

int main(void)
{
	pthread_t t0, t1;
	pthread_create(&t0, 0, thread0, 0);
	pthread_create(&t1, 0, thread1, 0);
	pthread_join(t0, 0);
	pthread_join(t1, 0);
	if (atomic_load_explicit(&r0, memory_order_seq_cst) == 2 &&
	    atomic_load_explicit(&r1, memory_order_seq_cst) == 1 &&
	    atomic_load_explicit(&x, memory_order_seq_cst) == 2 &&
	    atomic_load_explicit(&y, memory_order_seq_cst) == 1)
		assert(0);
	return 0;
}
