#include <assert.h>
#include <stdatomic.h>

static atomic_int ticket = 1;
static atomic_int flag;

int main(void)
{
	int observed = atomic_fetch_add_explicit(&ticket, 0, memory_order_relaxed);
	while (observed != 0)
		observed = atomic_load_explicit(&flag, memory_order_relaxed);
	assert(0 && "the body load must be observed by the next header condition");
}
