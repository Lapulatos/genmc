#include <stdatomic.h>

static atomic_int flag = 1;
static atomic_int last_nonzero;

static int await_zero(void)
{
	int last = 0;
	int observed = 0;
	for (; (observed = atomic_load_explicit(&flag, memory_order_relaxed)) != 0;) {
		last = observed;
	}
	return last;
}

int main(void)
{
	atomic_store_explicit(&last_nonzero, await_zero(), memory_order_relaxed);
}
