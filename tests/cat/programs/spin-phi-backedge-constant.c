#include <assert.h>
#include <stdatomic.h>

static atomic_int flag = 1;

int main(void)
{
	int local = 0;
	while (atomic_load_explicit(&flag, memory_order_relaxed) != 0 && local == 0)
		local = 1;
	assert(0 && "a loop-carried constant must not turn this finite loop into a spin block");
}
