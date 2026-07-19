#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

static atomic_int x;
static atomic_int observed;

static void *write_zero(void *unused)
{
	atomic_store_explicit(&x, 0, memory_order_seq_cst);
	return 0;
}

static void *reader(void *unused)
{
	int value = atomic_load_explicit(&x, memory_order_seq_cst);
	for (int i = 0; i != 2; ++i)
		atomic_store_explicit(&observed, value, memory_order_seq_cst);
	return 0;
}

int main(void)
{
	pthread_t w, r;
	pthread_create(&w, 0, write_zero, 0);
	pthread_create(&r, 0, reader, 0);
	pthread_join(w, 0);
	pthread_join(r, 0);
	assert(atomic_load_explicit(&observed, memory_order_seq_cst) == 0);
	return 0;
}
