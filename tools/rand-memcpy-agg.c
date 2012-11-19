#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <string.h>
#include <numa.h>
#include <numaif.h>
#include <assert.h>

#define NUM_THREADS 64
#define PAGE_SIZE 4096
#define ENTRY_SIZE PAGE_SIZE
#define ARRAY_SIZE 1073741824

off_t *offset;
unsigned int nentries;
int nthreads;
struct timeval global_start;
char *array;
char *dst_arr;

struct thread_id
{
	int node_id;
	int tid;
};

void permute_offset(off_t *offset, int num)
{
	int i;
	for (i = num - 1; i >= 1; i--) {
		int j = random() % i;
		off_t tmp = offset[j];
		offset[j] = offset[i];
		offset[i] = tmp;
	}
}

float time_diff(struct timeval time1, struct timeval time2)
{
	return time2.tv_sec - time1.tv_sec
			+ ((float)(time2.tv_usec - time1.tv_usec))/1000000;
}

void rand_read(void *arg)
{
	int fd;
	ssize_t ret;
	int i, j, start_i, end_i;
	ssize_t read_bytes = 0;
	struct timeval start_time, end_time;
	struct thread_id *id = (struct thread_id *) arg;
	printf("node %d, thread %d\n", id->node_id, id->tid);

	/* bind to node 0. */
	nodemask_t nodemask;
	nodemask_zero(&nodemask);
	nodemask_set_compat(&nodemask, id->node_id);
	unsigned long maxnode = NUMA_NUM_NODES;
	if (set_mempolicy(MPOL_BIND,
				(unsigned long *) &nodemask, maxnode) < 0) {
		perror("set_mempolicy");
		exit(1);
	}

	array = malloc(ARRAY_SIZE / nthreads);
	dst_arr = malloc(ARRAY_SIZE / nthreads);

	printf("run on node %d\n", id->node_id);
	if (numa_run_on_node(id->node_id) < 0) {
		perror("numa_run_on_node");
		exit(1);
	}

	start_i = nentries / nthreads * id->tid;
	end_i = start_i + nentries / nthreads;
	gettimeofday(&start_time, NULL);
	for (j = 0; j < 8; j++) {
		for (i = 0; i < nentries / nthreads; i++) {
			memcpy(dst_arr + offset[i], array + offset[i], ENTRY_SIZE);
			read_bytes += ENTRY_SIZE;
		}
	}
	gettimeofday(&end_time, NULL);
	printf("read %ld bytes, start at %f seconds, takes %f seconds\n",
			read_bytes, time_diff(global_start, start_time),
			time_diff(start_time, end_time));
	
	pthread_exit((void *) read_bytes);
}

int main(int argc, char *argv[])
{
	int ret;
	int i;
	struct timeval start_time, end_time;
	ssize_t read_bytes = 0;
	pthread_t threads[NUM_THREADS];
	/* the number of entries the array can contain. */
	int num_nodes;

	if (argc != 3) {
		fprintf(stderr, "read num_nodes num_threads\n");
		exit(1);
	}
	num_nodes = atoi(argv[1]);
	nthreads = atoi(argv[2]);

	nentries = ARRAY_SIZE / ENTRY_SIZE;
	offset = malloc(sizeof(*offset) * nentries / nthreads);
	for(i = 0; i < nentries / nthreads; i++) {
		offset[i] = ((off_t) i) * ENTRY_SIZE;
	}
	permute_offset(offset, nentries / nthreads);

#if 0
	int ncpus = numa_num_configured_cpus();
	printf("there are %d cores in the machine\n", ncpus);
	for (i = 0; i < ncpus; i++) {
		printf("cpu %d belongs to node %d\n",
			i, numa_node_of_cpu(i));
	}
#endif
	assert(nthreads % num_nodes == 0);
	if (nthreads > NUM_THREADS) {
		fprintf(stderr, "too many threads\n");
		exit(1);
	}

	ret = setpriority(PRIO_PROCESS, getpid(), -20);
	if (ret < 0) {
		perror("setpriority");
		exit(1);
	}

	struct thread_id ids[nthreads];
	int nthreads_per_node = nthreads / num_nodes;
	printf("num threads per node: %d\n", nthreads_per_node);
	for (i = 0; i < nthreads; i++) {
		ids[i].node_id = i / nthreads_per_node;
		ids[i].tid = i;
	}

	gettimeofday(&start_time, NULL);
	global_start = start_time;
	for (i = 0; i < nthreads; i++) {
		ret = pthread_create(&threads[i], NULL,
				rand_read, (void *) &ids[i]);
		if (ret) {
			perror("pthread_create");
			exit(1);
		}
	}

	for (i = 0; i < nthreads; i++) {
		ssize_t size;
		ret = pthread_join(threads[i], (void **) &size);
		if (ret) {
			perror("pthread_join");
			exit(1);
		}
		read_bytes += size;
	}
	gettimeofday(&end_time, NULL);
	printf("read %ld bytes, takes %f seconds\n",
			read_bytes, end_time.tv_sec - start_time.tv_sec
			+ ((float)(end_time.tv_usec - start_time.tv_usec))/1000000);
}