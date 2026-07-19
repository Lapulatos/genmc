#include <pthread.h>
#include <stdatomic.h>

extern void __VERIFIER_assume_internal(_Bool, char);

static atomic_int x;

static void *wwrace_writer(void *arg)
{
	(void)arg;
	atomic_store_explicit(&x, 1, memory_order_seq_cst);
	return 0;
}

static void *wwrace_reader(void *arg)
{
	(void)arg;
	int value = atomic_load_explicit(&x, memory_order_seq_cst);
	__VERIFIER_assume_internal(value == 1, 0);
	return 0;
}

int main(void)
{
	pthread_t a;
	pthread_t b;
	pthread_t reader;
	pthread_create(&reader, 0, wwrace_reader, 0);
	pthread_create(&a, 0, wwrace_writer, 0);
	pthread_create(&b, 0, wwrace_writer, 0);
	pthread_join(reader, 0);
	pthread_join(a, 0);
	pthread_join(b, 0);
	return 0;
}
