#include <assert.h>
#include <stdatomic.h>

extern void __VERIFIER_assume_internal(_Bool, char);

static atomic_int x = 1;

int main(void)
{
	int value = atomic_load_explicit(&x, memory_order_seq_cst);
	__VERIFIER_assume_internal(value == 1, 0);
	assert(0);
	return 0;
}
