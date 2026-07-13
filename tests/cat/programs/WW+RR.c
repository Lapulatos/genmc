/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 * You may choose to use, distribute, or modify this software under either license.
 *
 * Apache License 2.0:
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * MIT License:
 *     https://opensource.org/licenses/MIT
 */

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>

static atomic_int x;
static atomic_int y;

/* Cross-location stores remain ordered by TSO but may propagate separately in PSO. */
static void *writer(void *unused)
{
	atomic_store_explicit(&x, 1, memory_order_relaxed);
	atomic_store_explicit(&y, 1, memory_order_relaxed);
	return unused;
}

/* Read order prevents the target outcome from arising through load reordering. */
static void *reader(void *unused)
{
	const int readY = atomic_load_explicit(&y, memory_order_relaxed);
	const int readX = atomic_load_explicit(&x, memory_order_relaxed);
	assert(!(readY == 1 && readX == 0));
	return unused;
}

int main(void)
{
	pthread_t writerThread;
	pthread_t readerThread;

	if (pthread_create(&writerThread, NULL, writer, NULL))
		abort();
	if (pthread_create(&readerThread, NULL, reader, NULL))
		abort();
	return 0;
}
