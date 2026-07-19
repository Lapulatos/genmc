#include <stdatomic.h>

extern void __VERIFIER_assume_internal(_Bool, char);

static atomic_int x;
static atomic_int y;

int main(void)
{
	int first = atomic_load_explicit(&x, memory_order_seq_cst);
	int second = atomic_load_explicit(&y, memory_order_seq_cst);
	__VERIFIER_assume_internal(first == second, 0);
	return 0;
}
