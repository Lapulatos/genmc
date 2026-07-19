#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

atomic_int x;
atomic_int y;
atomic_int observed_x;
atomic_int observed_y;

static void *thread_x(void *unused)
{
	(void)unused;
	atomic_store_explicit(&x, 1, memory_order_seq_cst);
	atomic_store_explicit(&observed_y, atomic_load_explicit(&y, memory_order_seq_cst),
			      memory_order_seq_cst);
	return 0;
}

static void *thread_y(void *unused)
{
	(void)unused;
	atomic_store_explicit(&y, 1, memory_order_seq_cst);
	atomic_store_explicit(&observed_x, atomic_load_explicit(&x, memory_order_seq_cst),
			      memory_order_seq_cst);
	return 0;
}

int main(void)
{
	pthread_t tx;
	pthread_t ty;
	pthread_create(&tx, 0, thread_x, 0);
	pthread_create(&ty, 0, thread_y, 0);
	pthread_join(tx, 0);
	pthread_join(ty, 0);
	if (atomic_load(&observed_x) == EXPECT_X &&
	    atomic_load(&observed_y) == EXPECT_Y)
		assert(0);
}
