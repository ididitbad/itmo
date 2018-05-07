#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <sys/time.h>
#include <sys/sysinfo.h>

#define A (7 * 6 * 9) //  Каширин Кирилл Сергеевич

time_t start;
char progress = 0;
char done = 0;

void print_array(double *arr, int n) {
	for (int i = 0; i < n; i++)
		printf("%f ", arr[i]);
	printf("\n");
}

double get_double_rand_r(double min, double max, unsigned int *seedp) {
	int rand = rand_r(seedp);
	return ((double)rand / (double)RAND_MAX * (max - min) + min);
} 

void merge(double* arr, int l, int m, int r) {
	int i, j, k;
	int n1 = m - l + 1;
	int n2 = r - m;
 
	double *L = malloc(n1 * sizeof(double));
	double *R = malloc(n2 * sizeof(double));
 
	for (i = 0; i < n1; i++)
		L[i] = arr[l + i];
	
	for (j = 0; j < n2; j++)
		R[j] = arr[m + 1 + j];
 
	i = 0;
	j = 0;
	k = l;
	while (i < n1 && j < n2) {
		if (L[i] <= R[j]) {
			arr[k] = L[i];
			i++;
		}
		else {
			arr[k] = R[j];
			j++;
		}
		k++;
	}
 
	while (i < n1) {
		arr[k] = L[i];
		i++;
		k++;
	}
 
	while (j < n2) {
		arr[k] = R[j];
		j++;
		k++;
	}
	
	free(L);
	free(R);
}

void heapify(double *arr, int n, int i) {
	int largest = i;  
	int l = 2 * i + 1;  
	int r = 2 * i + 2; 
 
	if (l < n && arr[l] > arr[largest])
		  largest = l;
 
	if (r < n && arr[r] > arr[largest])
		  largest = r;
 
	if (largest != i) {
		double tmp;
	 	tmp = arr[largest];
		arr[largest] = arr[i];
		arr[i] = tmp;
 
		heapify(arr, n, largest);
	}
}
 
void heapSort(double *arr, int n) {
	int i;
	
	for (i = n / 2 - 1; i >= 0; i--)
		heapify(arr, n, i);
 
	for (i = n - 1; i >= 0; i--) {
		double tmp;
		tmp = arr[0];
		arr[0] = arr[i];
		arr[i] = tmp;
 
		heapify(arr, i, 0);
	}
}

struct cs_args {
	unsigned int proc_num;
	unsigned int ms;
};

void* cpu_stat(void *arg) {

	struct cs_args *args = (struct cs_args*) arg;

	long** cpu_stats = malloc(args->proc_num * sizeof(long*));
	for (int i = 0; i < args->proc_num; i++) 
		cpu_stats[i] = calloc(10, sizeof(long));

	struct timespec ts = { args->ms / 1000, (args->ms % 1000) * 1000000 };
	
	FILE* fw = fopen("cpu_load.txt", "a");
	FILE* fr = fopen("/proc/stat", "r");
	if (fr == NULL || fw == NULL)
		pthread_exit((void*) 1);
	
	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	while (!done) {

		freopen("/proc/stat", "r", fr);

		int proc_count = 0;
		fprintf(fw, "time_s: %3ld", (long)(time(NULL) - start));
		while ((read = getline(&line, &len, fr)) != -1 && proc_count < args->proc_num) {
			if (line[0] == 'c' && line[1] == 'p' && line[2] == 'u' && line[3] != ' ') {
				int old_space = 0, count = 0;
				for (int i = 0; i < read; i++) {
					if (line[i] == ' ' || i == (read - 1)) {
						if (old_space == 0) {
							old_space = i;
							continue;
						}
						cpu_stats[proc_count][count] = strtol(&line[old_space + 1], NULL, 10);
						old_space = i;
						count++;
					}
				}
				long cpu_sum = 0;
				for (int i = 0; i < 10; i++)
					cpu_sum += cpu_stats[proc_count][i];
				double cpu_load = (double)(cpu_sum - cpu_stats[proc_count][3]) / (double)cpu_sum;
				fprintf(fw, ", cpu[%d]: %10.6f%%", proc_count, cpu_load * 100);
				proc_count++;
			}
		}

		fprintf(fw, "\n");
		fflush(fw);
		nanosleep(&ts, NULL);
	
	}

	for (int i = 0; i < args->proc_num; i++) 
		free(cpu_stats[i]);
	free(cpu_stats);
	free(line);
	fclose(fr); 
	fclose(fw); 

	pthread_exit(0);
}

void* progress_bar(void* arg) {

	while (!done) {
		fprintf(stderr, "progress %3d%%\r", progress);
		fflush(stderr);
		sleep(1);
	}

	pthread_exit(0);
}

struct cc_args {
	pthread_barrier_t *barrier;
	pthread_mutex_t *mutex_X; 
	pthread_mutex_t *mutex_sin;
	unsigned int thread_num;
	unsigned int proc_num;
	long N;
	double* M1;
	double* M2;
	double* min_sin;
	double* X;
};

void* cycle(void *arg) {

	struct cc_args *args = (struct cc_args*) arg;
	pthread_barrier_t *barrier = args->barrier;
	pthread_mutex_t *mutex_X = args->mutex_X; 
	pthread_mutex_t *mutex_sin = args->mutex_sin;
	unsigned int thread_num = args->thread_num;
	unsigned int proc_num = args->proc_num;
	long N = args->N;
	double* M1 = args->M1;
	double* M2 = args->M2;

	/*
	// 1. Generate ---> 3 <--------------------------------------------
	
	for (long j = N / proc_num * (thread_num - 1); j < N / proc_num * thread_num; j++) {
		M1[j] = get_double_rand_r(1, A, &seed);
	} 
	for (long j = N / 2 / proc_num * (thread_num - 1); j < N / 2 / proc_num * thread_num; j++) {
		M2[j] = get_double_rand_r(A, 10 * A, &seed); 
	}
	*/
	
	// 2. Map ---> 3 <------------------------------------------------
	
	//printf("i am thread %d doing M1[%ld..%ld] and M2[%ld..%ld]\n",
	//	thread_num, N * (thread_num - 1) / proc_num, N * thread_num / proc_num - 1,
	//	N * (thread_num - 1) / 2 / proc_num, N * thread_num / 2 / proc_num - 1);
		
	for (long j = (thread_num - 1) * N / proc_num; j < thread_num * N / proc_num; j++) 
		M1[j] = tanh(M1[j]) - 1;
	for (long j = (thread_num - 1) * N / 2 / proc_num; j < thread_num * N / 2 / proc_num; j++) {
		//M2[j] += (j == 0) ? 0 : M2[j - 1];
		M2[j] = fabs(tan(M2[j]));
	}

	//printf("2.Map\n"); 
	//printf("M1:\n"); 
	//print_array(M1, N);
	//printf("M2:\n"); 
	//print_array(M2, N / 2);

	pthread_barrier_wait(&barrier[0]);

	// 3. Merge ---> 3 <---------------------------------------------

	for (long j = (thread_num - 1) * N / 2 / proc_num; j < thread_num * N / 2 / proc_num; j++) 
		M2[j] *= M1[j];
			
	pthread_barrier_wait(&barrier[1]);

	//printf("3.Merge\n"); 
	//printf("M2:\n"); 
	//print_array(M2, N / 2);

	// 4. Sort ---> 3 <---------------------------------------------

	heapSort(M2 + (thread_num - 1) * N / 2 / proc_num, N / 2 / proc_num);

	pthread_barrier_wait(&barrier[2]);
	// Merging in main thread....
	pthread_barrier_wait(&barrier[3]);

	//printf("4.Sort\n"); 
	//printf("M2:\n"); 
	//print_array(M2, N / 2);

	// 5. Reduce --------------------------------------------------------
	
	double min_sin = DBL_MAX;
	for (long j = (thread_num - 1) * N / 2 / proc_num; j < thread_num * N / 2 / proc_num; j++) 
		if (M2[j] != 0 && M2[j] < min_sin)
			min_sin = M2[j];
	
	//pthread_mutex_lock(mutex_sin);
	if (min_sin < *(args->min_sin))
		*(args->min_sin) = min_sin;
	//pthread_mutex_unlock(mutex_sin);

	pthread_barrier_wait(&barrier[4]);

	double X = 0;
	for (long j = (thread_num - 1) * N / 2 / proc_num; j < thread_num * N / 2 / proc_num; j++)
		if (((int)(M2[j] / min_sin)) % 2 == 0)
			X += sin(M2[j]);
	
	//pthread_mutex_lock(mutex_X);
	*(args->X) += X;
	//pthread_mutex_unlock(mutex_X);

	pthread_barrier_wait(&barrier[5]);

	pthread_exit(0);
}

int main(int argc, char* argv[]) {

	start = time(NULL);

	unsigned int proc_num = get_nprocs();

	pthread_t helper[2]; 
	pthread_t thread[proc_num];
	struct cc_args cc_args[proc_num];

	unsigned int barrier_num = 6;
	pthread_barrier_t *barrier = malloc(barrier_num * sizeof(pthread_barrier_t));

	pthread_mutex_t mutex_X = PTHREAD_MUTEX_INITIALIZER; 
	pthread_mutex_t mutex_sin = PTHREAD_MUTEX_INITIALIZER; 

	const int count = 10;
	const int stages = 5;
	unsigned int seed = 42;
	unsigned int verbose = 0;
	double X = 0, min_sin = DBL_MAX;
		
	struct timeval T1, T2;
	long time_ms;
	
	long times[stages][count];
	long min_times[stages]; 
	for (int i = 0; i < stages; i++) min_times[i] = LONG_MAX;

	long N = 0;
	if (argc > 1)
		N = strtol(argv[1], NULL, 10);	
	if (N <= 0) {
		printf("usage: %s N\nwhere N > 0\n", argv[0]);
		return 1;
	}
	if (argc > 2 && argv[2][0] == '-' && argv[2][1] == 'v')
		verbose = 1;

	double *M1 = malloc(sizeof(double) * N);
	double *M2 = malloc(sizeof(double) * N / 2);
	
	srand(seed);  

	struct cs_args cs_args = {proc_num, 1000};
	pthread_create(&helper[0], NULL, progress_bar, NULL);
	pthread_create(&helper[1], NULL, cpu_stat    , &cs_args);

	for (int i = 0; i < count; i++) {
		
		X = 0;
		min_sin = DBL_MAX;
		// ---------------------------STAGE 1--------------------------------------
			
		gettimeofday(&T1, NULL);  

		for (long j = 0; j < N; j++) {
			M1[j] = get_double_rand_r(1, A, &seed);
		} 
		for (long j = 0; j < N / 2; j++) {
			M2[j] = get_double_rand_r(A, 10 * A, &seed); 
		}

		gettimeofday(&T2, NULL);  
		time_ms = 1000 * (T2.tv_sec - T1.tv_sec) + (T2.tv_usec - T1.tv_usec) / 1000;

		if (time_ms < min_times[0])	  
			min_times[0] = time_ms;
	
		times[0][i] = time_ms;
			
		if (verbose) {
			printf("1.Generate\n"); 
			printf("M1:\n"); 
			print_array(M1, N);
			printf("M2:\n"); 
			print_array(M2, N / 2);
		}

		// ---------------------------THREADS CREATION-----------------------------

		for (int i = 0; i < barrier_num; i++)
			pthread_barrier_init(&barrier[i], NULL, proc_num + 1);
		for (int i = 0; i < proc_num; i++) {
			cc_args[i].barrier = barrier;
			cc_args[i].mutex_X = &mutex_X;
			cc_args[i].mutex_sin = &mutex_sin;
			cc_args[i].thread_num = i + 1;
			cc_args[i].proc_num = proc_num;
			cc_args[i].N = N;
			cc_args[i].M1 = M1;
			cc_args[i].M2 = M2;
			cc_args[i].min_sin = &min_sin;
			cc_args[i].X = &X;
			pthread_create(&thread[i], NULL, cycle, &cc_args[i]);
		}
	
		// ---------------------------STAGE 2--------------------------------------

		gettimeofday(&T1, NULL);  

		pthread_barrier_wait(&barrier[0]);

		gettimeofday(&T2, NULL);  
		time_ms = 1000 * (T2.tv_sec - T1.tv_sec) + (T2.tv_usec - T1.tv_usec) / 1000;

		if (time_ms < min_times[1])	  
			min_times[1] = time_ms;
	
		times[1][i] = time_ms;

		// ---------------------------STAGE 3--------------------------------------

		gettimeofday(&T1, NULL);  

		pthread_barrier_wait(&barrier[1]);

		gettimeofday(&T2, NULL);  
		time_ms = 1000 * (T2.tv_sec - T1.tv_sec) + (T2.tv_usec - T1.tv_usec) / 1000;

		if (time_ms < min_times[2])	  
			min_times[2] = time_ms;
	
		times[2][i] = time_ms;
				
		// ---------------------------STAGE 4--------------------------------------

		gettimeofday(&T1, NULL);  

		pthread_barrier_wait(&barrier[2]);
		
		for (long k = 0; k < proc_num - 1; k++) {
			merge(M2, k * N / 2 / proc_num, (k + 1) * N / 2 / proc_num - 1, (k + 2) * N / 2 / proc_num - 1);
		}					

		pthread_barrier_wait(&barrier[3]);

		gettimeofday(&T2, NULL);  
		time_ms = 1000 * (T2.tv_sec - T1.tv_sec) + (T2.tv_usec - T1.tv_usec) / 1000;

		if (time_ms < min_times[3])	  
			min_times[3] = time_ms;
	
		times[3][i] = time_ms;
	
		// ---------------------------STAGE 5--------------------------------------

		gettimeofday(&T1, NULL);  

		pthread_barrier_wait(&barrier[4]);
		pthread_barrier_wait(&barrier[5]);

		gettimeofday(&T2, NULL);  
		time_ms = 1000 * (T2.tv_sec - T1.tv_sec) + (T2.tv_usec - T1.tv_usec) / 1000;

		if (time_ms < min_times[4])	  
			min_times[4] = time_ms;
	
		times[4][i] = time_ms;
		
		// ---------------------------THREADS DESTRUCTION----------------------------

		for (int i = 0; i < proc_num; i++) 
			pthread_join(thread[i], NULL);
		for (int i = 0; i < barrier_num; i++)
			pthread_barrier_destroy(&barrier[i]);

		progress = 100 * i / count;
	}
	done = 1;

	long sum_time = 0;
	for (int i = 0; i < stages; i++) sum_time += min_times[i];
	printf("X=%f, N=%ld. All time=%ld\n", X, N, sum_time);
	for (int i = 0; i < stages; i++) {
		printf("Stage %d. Best time (ms): %ld\nTimes: ", i, min_times[i]);
		for (int j = 0; j < count; j++) 
			printf("%ld ", times[i][j]);
		printf("\n");
	}
	
	free(M1);
	free(M2);

	pthread_join(helper[0], NULL);	
	pthread_join(helper[1], NULL);	

	return 0;
}
