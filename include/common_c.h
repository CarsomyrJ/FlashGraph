#ifndef __COMMON_C_H__
#define __COMMON_C_H__

/**
 * Copyright 2013 Da Zheng
 *
 * This file is part of SAFSlib.
 *
 * SAFSlib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SAFSlib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SAFSlib.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <numa.h>
#include <assert.h>
#include <execinfo.h>

#define gettid() syscall(__NR_gettid)

#define ROUND(off, base) (((long) off) & (~((long) (base) - 1)))
#define ROUNDUP(off, base) (((long) off + (base) - 1) & (~((long) (base) - 1)))

#define ROUND_PAGE(off) (((long) off) & (~((long) PAGE_SIZE - 1)))
#define ROUNDUP_PAGE(off) (((long) off + PAGE_SIZE - 1) & (~((long) PAGE_SIZE - 1)))

#define PRINT_BACKTRACE()							\
	do {											\
		void *buf[100];								\
		char **strings;								\
		int nptrs = backtrace(buf, 100);			\
		strings = backtrace_symbols(buf, nptrs);	\
		if (strings == NULL) {						\
			perror("backtrace_symbols");			\
			exit(EXIT_FAILURE);						\
		}											\
		for (int i = 0; i < nptrs; i++)	{			\
			char syscom[256];						\
			printf("[bt] #%d %s\n", i, strings[i]);	\
			sprintf(syscom,"addr2line %p -e %s", buf[i], program_invocation_name);\
			assert(system(syscom) == 0);			\
		}											\
		free(strings);								\
	} while (0)

#define ASSERT_TRUE(x)								\
	if (!(x)) {										\
		PRINT_BACKTRACE();							\
		assert(x);									\
	}

#ifdef __cplusplus
extern "C" {
#endif

inline static float time_diff(struct timeval time1, struct timeval time2)
{
	return time2.tv_sec - time1.tv_sec
			+ ((float)(time2.tv_usec - time1.tv_usec))/1000000;
}

inline static long time_diff_us(struct timeval time1, struct timeval time2)
{
	return (time2.tv_sec - time1.tv_sec) * 1000000
			+ (time2.tv_usec - time1.tv_usec);
}

inline static long min(long v1, long v2)
{
	return v1 > v2 ? v2 : v1;
}

inline static long max(long v1, long v2)
{
	return v1 < v2 ? v2 : v1;
}

inline static int power2(int num)
{
	if (num < 0)
		num = -num;
	if (num == 1)
		return 0;
	while (num > 1) {
		if (num % 2)
			return 0;
		num = num / 2;
	}
	return 1;
}

inline static long get_curr_ms()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return ((long) tv.tv_sec) * 1000 + tv.tv_usec / 1000;
};

inline static long get_curr_us()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return ((long) tv.tv_sec) * 1000 * 1000 + tv.tv_usec;
}

int isnumeric(char *str);

static const int CONST_A = 27644437;
static const long CONST_P = 68718952447L;

static inline int universal_hash(off_t v, int modulo)
{
	return (v * CONST_A) % CONST_P % modulo;
}

void permute_offsets(int num, int repeats, int stride, off_t start,
		off_t offsets[]);

/**
 * This returns the first node id where the process can allocate memory.
 */
int numa_get_mem_node();

void bind2node_id(int node_id);
void bind_mem2node_id(int node_id);
void bind2cpu(int cpu_id);

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#ifdef __cplusplus
}
#endif

#endif
