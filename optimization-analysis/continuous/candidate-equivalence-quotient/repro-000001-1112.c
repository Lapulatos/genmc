#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

static atomic_int x;
static atomic_int y;
static atomic_int r0;
static atomic_int r1;

static void *thread0(void *arg)
{
	atomic_store_explicit(&x, 1, memory_order_seq_cst);
	int value = atomic_load_explicit(&x, memory_order_seq_cst);
	atomic_store_explicit(&r0, value, memory_order_seq_cst);
	atomic_store_explicit(&x, 2, memory_order_seq_cst);
	return arg;
}

static void *thread1(void *arg)
{
	atomic_store_explicit(&x, 1, memory_order_seq_cst);
	int value = atomic_load_explicit(&x, memory_order_seq_cst);
	atomic_store_explicit(&r1, value, memory_order_seq_cst);
	atomic_store_explicit(&y, 2, memory_order_seq_cst);
	return arg;
}

int main(void)
{
	pthread_t t0;
	pthread_t t1;
	pthread_create(&t0, 0, thread0, 0);
	pthread_create(&t1, 0, thread1, 0);
	pthread_join(t0, 0);
	pthread_join(t1, 0);
	int final_x = atomic_load_explicit(&x, memory_order_seq_cst);
	int final_y = atomic_load_explicit(&y, memory_order_seq_cst);
	int final_r0 = atomic_load_explicit(&r0, memory_order_seq_cst);
	int final_r1 = atomic_load_explicit(&r1, memory_order_seq_cst);
	if (final_r0 == 1 && final_r1 == 1 && final_x == 1 && final_y == 2)
		assert(0);
	return 0;
}
