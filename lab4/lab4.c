#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <malloc.h>
#include <sys/time.h>

#define A (7 * 6 * 9) //  Каширин Кирилл Сергеевич

#ifdef _OPENMP
	#include <omp.h>
#else
	#define omp_get_num_procs() 1
	#define omp_get_num_threads() 1
	#define omp_get_thread_num() 1
	#define omp_set_num_threads(x) {while(0);} 
	#define omp_set_nested(x) {while(0);} 
#endif

void print_array(double *arr, int n) {
	for (int i = 0; i < n; i++)
		printf("%f ", arr[i]);
	printf("\n");
}

double get_double_rand_r(double min, double max, unsigned int *seedp) {
	int rand = rand_r(seedp);
	return ((double)rand / (double)RAND_MAX * (max - min) + min);
} 

void heapify(double *arr, int n, int i)
{
	int largest = i;  
	int l = 2 * i + 1;  
	int r = 2 * i + 2; 
 
	if (l < n && arr[l] > arr[largest])
		  largest = l;
 
	if (r < n && arr[r] > arr[largest])
		  largest = r;
 
	if (largest != i)
	{
		double tmp;
	 	tmp = arr[largest];
		arr[largest] = arr[i];
		arr[i] = tmp;
 
		heapify(arr, n, largest);
	}
}
 
void heapSort(double *arr, int n)
{
	int i;
	
	for (i = n / 2 - 1; i >= 0; i--)
		heapify(arr, n, i);
 
	for (i = n - 1; i >= 0; i--)
	{
		double tmp;
		tmp = arr[0];
		arr[0] = arr[i];
		arr[i] = tmp;
 
		heapify(arr, i, 0);
	}
}

int main(int argc, char* argv[]) {

	unsigned int seed = 42;
	unsigned int i = 0, j = 0, verbose = 0;
	int progress = 0, done = 0;
	double X;
	#ifdef _OPENMP
		double T1, T2;
	#else
		struct timeval T1, T2;
	#endif
	long time_ms, minimal_time_ms = LONG_MAX;
	int N = 0;

	if (argc > 1)
		N = atoi(argv[1]);	
	if (N <= 0) {
		printf("usage: %s N [-v]\nwhere N > 0\n", argv[0]);
		return 1;
	}
	if (argc > 2 && argv[2][0] == '-' && argv[2][1] == 'v')
		verbose = 1;

	double *M1 = malloc(sizeof(double) * N);
	double *M2 = malloc(sizeof(double) * N / 2);

	srand(seed);  

	omp_set_num_threads(omp_get_num_procs());
	
	omp_set_nested(1);

	#pragma omp parallel num_threads(2)
	{
	
		if (omp_get_thread_num() == 0) {
			#pragma omp parallel num_threads(1) shared(progress,done)
			{
				//printf("procs=%d, count=%d, num=%d\n", 
				//	omp_get_num_procs(), omp_get_num_threads(), omp_get_thread_num());
				while (!done) {
					printf("progress %d%%\r", progress);
					fflush(stdout);
					sleep(1);
				}
			}
		}
		else {
			#pragma omp parallel num_threads(omp_get_num_procs() - 1) shared(progress,done)
			{
				#pragma omp single
				for (i = 0; i < 10; i++) {
					
					#pragma omp critical
					progress = i * 10;
					
					//printf("procs=%d, count=%d, num=%d\n", 
					//	omp_get_num_procs(), omp_get_num_threads(), omp_get_thread_num());
					
					#ifdef _OPENMP
						T1 = omp_get_wtime();
					#else
						gettimeofday(&T1, NULL);  
					#endif
					
					// 1. Generate ---> 3
					//#pragma omp parallel for default(none) private(j) shared(M1,N,seed)
					for (j = 0; j < N; j++) {
						//srand(j);  
						//M1[j] = get_double_rand_r(1, A, &j);
						//srand(seed);  
						M1[j] = get_double_rand_r(1, A, &seed);
					} 
					//#pragma omp parallel for default(none) private(j) shared(M2,N,seed)
					for (j = 0; j < N / 2; j++) {
						//srand(j);  
						//M2[j] = get_double_rand_r(A, 10 * A, &j); 
						//srand(seed);  
						M2[j] = get_double_rand_r(A, 10 * A, &seed); 
					}
					
					if (verbose) {
						printf("1.Generate\n"); 
						printf("M1:\n"); 
						print_array(M1, N);
						printf("M2:\n"); 
						print_array(M2, N/2);
					}

					// 2. Map ---> 3
					#pragma omp parallel for default(none) private(j) shared(M1,N)
					for (j = 0; j < N; j++)
						M1[j] = tanh(M1[j]) - 1;
					#pragma omp parallel for default(none) private(j) shared(M2,N)
					for (j = 0; j < N / 2; j++) { 
						//M2[j] += (j == 0) ? 0 : M2[j - 1];
						M2[j] = fabs(tan(M2[j]));
					}

					if (verbose) {
						printf("2.Map\n"); 
						printf("M1:\n"); 
						print_array(M1, N);
						printf("M2:\n"); 
						print_array(M2, N/2);
					}

					// 3. Merge ---> 3
					#pragma omp parallel for default(none) private(j) shared(M1,M2,N)
					for (j = 0; j < N / 2; j++) 
						M2[j] *= M1[j];
						
					if (verbose) {
						printf("3.Merge\n"); 
						printf("M2:\n"); 
						print_array(M2, N/2);
					}
					
					// 4. Sort ---> 3
					heapSort(M2, N / 2);

					if (verbose) {
						printf("4.Sort\n"); 
						printf("M2:\n"); 
						print_array(M2, N/2);
					}
					
					// 5. Reduce
					double min_sin = DBL_MAX;
					X = 0;
					#pragma omp parallel for default(none) private(j) shared(M2,N) reduction(min:min_sin)
					for (j = 0; j < N / 2; j++)
						if (M2[j] != 0 && M2[j] < min_sin)
							min_sin = M2[j];
					#pragma omp parallel for default(none) private(j) shared(M2,N,min_sin) reduction(+:X)
					for (j = 0; j < N / 2; j++)
						if (((int)(M2[j] / min_sin)) % 2 == 0)
							X += sin(M2[j]);
					
					#ifdef _OPENMP
						T2 = omp_get_wtime();
						time_ms = 1000 * (T2 - T1);
					#else
						gettimeofday(&T2, NULL);  
						time_ms = 1000 * (T2.tv_sec - T1.tv_sec) + (T2.tv_usec - T1.tv_usec) / 1000;
					#endif

					if (time_ms < minimal_time_ms)	  
						minimal_time_ms = time_ms;
				
				}
			}	
			done = 1;
		}
	}

	omp_set_nested(0);
	
	printf("X=%f, N=%d. Best time (ms): %ld\n", X, N, minimal_time_ms);

	free(M1);
	free(M2);
	return 0;
}
