#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <sys/time.h>

#include <CL/opencl.h>

#define A (7 * 6 * 9) //  Каширин Кирилл Сергеевич

void print_array(double *arr, int n) {
	for (int i = 0; i < n; i++)
		printf("%f ", arr[i]);
	printf("\n");
}

double get_double_rand_r(double min, double max, unsigned int *seedp) {
	int rand = rand_r(seedp);
	return ((double)rand / (double)RAND_MAX * (max - min) + min);
} 

void heapify(double *arr, int n, int i) {
	double tmp;
	int largest = i;
	int l = 2 * i + 1;
	int r = 2 * i + 2;
 
	if (l < n && arr[l] > arr[largest])
		  largest = l;
 
	if (r < n && arr[r] > arr[largest])
		  largest = r;
 
	if (largest != i) {
	 	tmp = arr[largest];
		arr[largest] = arr[i];
		arr[i] = tmp;
 
		heapify(arr, n, largest);
	}
}
 
void heapSort(double *arr, int n) {
	int i;
	double tmp;

	for (i = n / 2 - 1; i >= 0; i--)
		heapify(arr, n, i);
 
	for (i = n - 1; i >= 0; i--) {
		tmp = arr[0];
		arr[0] = arr[i];
		arr[i] = tmp;
 
		heapify(arr, i, 0);
	}
}

void panic(char* msg) {
	printf("%s\n", msg);
	exit(1);
}

const char *source2 = "\n" \
"__kernel void ul(                                     \n" \
"   __global double* M1_buf,                           \n" \
"   const unsigned int N)                              \n" \
"{                                                     \n" \
"   int i = get_global_id(0);                          \n" \
"//	printf(\" %f x %f = %f \\n \", M1_buf[i], M2_buf[i], M1_buf[i]*M2_buf[i]); \n"\
"   if (i < N)                                         \n" \
"		M1_buf[i] = tanh(M1_buf[i]) - 1;               \n" \
"}                                                     \n" \
"\n";

const char *source3 = "\n" \
"__kernel void mul(                                    \n" \
"   __global double* M1_buf,                           \n" \
"   __global double* M2_buf,                           \n" \
"   const unsigned int K)                              \n" \
"{                                                     \n" \
"   int i = get_global_id(0);                          \n" \
"//	printf(\" %f x %f = %f \\n \", M1_buf[i], M2_buf[i], M1_buf[i]*M2_buf[i]); \n"\
"   if (i < K) {                                       \n" \
"		M2_buf[i] = fabs(tan(M2_buf[i]));              \n" \
"       M2_buf[i] = M1_buf[i] * M2_buf[i];             \n" \
"	}                                                  \n" \
"}                                                     \n" \
"\n";

int main(int argc, char* argv[]) {

	unsigned int seed = 42, count = 10;
	int i = 0, j = 0, verbose = 0;
	double X;
	struct timeval T1, T2;
	long time_ms, minimal_time_ms = LONG_MAX;
	long times[count];
	int N = 0;

	if (argc > 1)
		N = atoi(argv[1]);
	if (N <= 0) {
		printf("usage: %s N\nwhere N > 0\n", argv[0]);
		return 1;
	}
	if (argc > 2 && argv[2][0] == '-' && argv[2][1] == 'v')
		verbose = 1;

	double *M1 = malloc(sizeof(double) * N);
	double *M2 = malloc(sizeof(double) * N / 2);

	srand(seed); 

	//////////////////////////////////////////////////////////////////////////

	int err;                            // error code returned from api calls
	  
	cl_platform_id platform;            //
	cl_device_id device;                // compute device id 
	cl_context context;                 // compute context
	cl_command_queue commands;          // compute command queue
	cl_program program2, program3;      // compute program3
	cl_kernel kernel2, kernel3;         // compute kernel3
	cl_event event1, event2;         	// 
	cl_ulong cl_T1, cl_T2;
	double cl_times[count];
	
	cl_mem M1_buf;                      // device memory used for the M1_buf array
	cl_mem M2_buf;                      // device memory used for the M2_buf array

	err = clGetPlatformIDs(1, &platform, NULL);
	if (err != CL_SUCCESS) 
		panic("Error: Failed to get platform!");

	err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
	if (err != CL_SUCCESS) 
		panic("Error: Failed to create a device group!");
  
	context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
	if (!context) 
		panic("Error: Failed to create a compute context!");
 
	cl_queue_properties properties[] = { CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0 };
	commands = clCreateCommandQueueWithProperties(context, device, properties, &err);
	if (err == CL_INVALID_VALUE) printf("values\n");
	if (!commands) 
		panic("Error: Failed to create a command queue!");
 
	program2 = clCreateProgramWithSource(context, 1, &source2, NULL, &err);
	program3 = clCreateProgramWithSource(context, 1, &source3, NULL, &err);
	if (!program2 || !program3) 
		panic("Error: Failed to create compute program!");
 
	err = clBuildProgram(program2, 0, NULL, "-cl-std=CL2.0", NULL, NULL);
	if (err == CL_SUCCESS)
		err = clBuildProgram(program3, 0, NULL, "-cl-std=CL2.0", NULL, NULL);
	if (err != CL_SUCCESS) {
	    size_t len;
	    char buffer[2048];
 
	    printf("Error: Failed to build program2 executable!\n");
	    clGetProgramBuildInfo(program2, device, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
	    printf("%s\n", buffer);
	    exit(1);
	}
 
	kernel2 = clCreateKernel(program2, "ul", &err);
	kernel3 = clCreateKernel(program3, "mul", &err);
	if (!kernel2 || !kernel3 || err != CL_SUCCESS) 
		panic("Error: Failed to create compute kernel2!");
 
 
	M1_buf = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(double) * N,     NULL, NULL);
	M2_buf = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(double) * N / 2, NULL, NULL);
	if (!M1_buf || !M2_buf) 
		panic("Error: Failed to allocate device memory!");
	
	err = 0;
	err  = clSetKernelArg(kernel2, 0, sizeof(cl_mem), &M1_buf);
	err |= clSetKernelArg(kernel2, 1, sizeof(unsigned int), &N);
	if (err != CL_SUCCESS)
	    panic("Error: Failed to set kernel3 arguments!");

	unsigned int K = N / 2;
	err = 0;
	err  = clSetKernelArg(kernel3, 0, sizeof(cl_mem), &M1_buf);
	err |= clSetKernelArg(kernel3, 1, sizeof(cl_mem), &M2_buf);
	err |= clSetKernelArg(kernel3, 2, sizeof(unsigned int), &K);
	if (err != CL_SUCCESS)
	    panic("Error: Failed to set kernel3 arguments!");

	//////////////////////////////////////////////////////////////////////////

	for (i = 0; i < count; i++) {

		gettimeofday(&T1, NULL);
		
		// 1. Generate ---> 3
		for (j = 0; j < N; j++)
			M1[j] = get_double_rand_r(1, A, &seed);
		for (j = 0; j < N / 2; j++)
			M2[j] = get_double_rand_r(A, 10 * A, &seed);
		
		// 2. Map ---> 3
		/*for (j = 0; j < N; j++)
			M1[j] = tanh(M1[j]) - 1;
		for (j = 0; j < N / 2; j++) {
			//M2[j] += (j == 0 ? 0 : M2[j - 1]);
			M2[j] = fabs(tan(M2[j]));
		}*/	

		if (verbose) {
			printf("2.Map\n"); 
			printf("M1:\n"); 
			print_array(M1, N);
			printf("M2:\n"); 
			print_array(M2, N / 2);
		}
		// 3. Merge ---> 3
		//for (j = 0; j < N / 2; j++) 
		//	M2[j] *= M1[j];

		err = clEnqueueWriteBuffer(commands, M1_buf, CL_TRUE, 0, sizeof(double) * N, M1, 0, NULL, &event1);
		if (err != CL_SUCCESS) 
		    panic("Error: Failed to write to M1_buf array!");

		err = clEnqueueWriteBuffer(commands, M2_buf, CL_TRUE, 0, sizeof(double) * N / 2, M2, 0, NULL, NULL);
		if (err != CL_SUCCESS) 
		    panic("Error: Failed to write to M2_buf array!");
 
 		size_t global2 = N, global3 = K;
		err = clEnqueueNDRangeKernel(commands, kernel2, 1, NULL, &global2, NULL, 0, NULL, NULL);
		if (!err)	
			err = clEnqueueNDRangeKernel(commands, kernel3, 1, NULL, &global3, NULL, 0, NULL, NULL);
		if (err) 
		    panic("Error: Failed to execute kernel!");
 		
		err = clEnqueueReadBuffer(commands, M2_buf, CL_TRUE, 0, sizeof(double) * N / 2, M2, 0, NULL, &event2);  
		if (err != CL_SUCCESS) 
		    panic("Error: Failed to read M2_buf array!");

		clFinish(commands);
			
		if (verbose) {
			printf("3.Merge\n"); 
			printf("M2:\n"); 
			print_array(M2, N / 2);
		}
					
		// 4. Sort ---> 3
		heapSort(M2, N / 2);

		// 5. Reduce
		double min_sin = DBL_MAX;
		X = 0;
		for (j = 0; j < N / 2; j++)
			if (M2[j] != 0 && M2[j] < min_sin)
				min_sin = M2[j];
		for (j = 0; j < N / 2; j++)
			if (((int)(M2[j] / min_sin)) % 2 == 0)
				X += sin(M2[j]);
		
		gettimeofday(&T2, NULL);
		time_ms = 1000 * (T2.tv_sec - T1.tv_sec) + (T2.tv_usec - T1.tv_usec) / 1000;
		if (time_ms < minimal_time_ms)	  
			minimal_time_ms = time_ms;
		times[i] = time_ms;

		err = clGetEventProfilingInfo(event1, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &cl_T1, NULL);
		err |= clGetEventProfilingInfo(event2, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &cl_T2, NULL);
		if (err == CL_PROFILING_INFO_NOT_AVAILABLE) printf("fu\n"); 
		if (err != CL_SUCCESS) 
			panic("Error: Failed to get profiling info!");
		cl_times[i] = ((double)cl_T2 - (double)cl_T1) / 1000000.0; 

		clReleaseEvent(event1);
		clReleaseEvent(event2);
	}

	printf("X=%f, N=%d. Best time (ms): %ld\n", X, N, minimal_time_ms);
	printf("All Times (ms): ");
	for (i = 0; i < count; i++)
		printf("%ld ", times[i]);
	printf("\n");
	printf("Times in OpenCL (ms): ");
	for (i = 0; i < count; i++)
		printf("%f ", cl_times[i]);
	printf("\n");

	free(M1);
	free(M2);
	clReleaseMemObject(M1_buf);
	clReleaseMemObject(M2_buf);
	clReleaseProgram(program2);
	clReleaseProgram(program3);
	clReleaseKernel(kernel2);
	clReleaseKernel(kernel3);
	clReleaseCommandQueue(commands);
	clReleaseContext(context);
	return 0;
}

