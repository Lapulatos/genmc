#ifndef SVCOMP_GENMC_COMPAT_V2_H
#define SVCOMP_GENMC_COMPAT_V2_H

/*
 * Compatibility declarations for SV-Benchmarks APIs that GenMC's runtime
 * implements but its public pthread shim does not currently expose.
 */
#include <genmc.h>
#include <pthread.h>

static pthread_mutex_t __genmc_svcomp_atomic_mutex = PTHREAD_MUTEX_INITIALIZER;

void __VERIFIER_atomic_begin(void)
{
	(void)pthread_mutex_lock(&__genmc_svcomp_atomic_mutex);
}

void __VERIFIER_atomic_end(void)
{
	(void)pthread_mutex_unlock(&__genmc_svcomp_atomic_mutex);
}

#ifndef PTHREAD_RWLOCK_INITIALIZER
#define PTHREAD_RWLOCK_INITIALIZER { { { 0 } } }
#endif

#endif
