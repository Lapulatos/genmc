#ifndef SVCOMP_GENMC_COMPAT_H
#define SVCOMP_GENMC_COMPAT_H

/*
 * Compatibility layer for SV-Benchmarks sources compiled with GenMC's
 * runtime headers.  SV-COMP atomic sections are represented by one private
 * mutex, preserving mutual exclusion without changing the benchmark source.
 */
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

#endif
