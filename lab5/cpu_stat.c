#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int cpu_stat(unsigned int proc_num, unsigned int ms) {

	time_t start = time(NULL);

	long** cpu_stats = malloc(proc_num * sizeof(long*));
	for (int i = 0; i < proc_num; i++) 
		cpu_stats[i] = calloc(10, sizeof(long));

	struct timespec ts = { ms / 1000, (ms % 1000) * 1000000 };

	long delta[10] = {0};
	
	FILE* fw = fopen("cpu_load.txt", "a");
	FILE* fr = fopen("/proc/stat", "r");
	if (fr == NULL || fw == NULL)
		return 1;
	
	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	while (1) {

		freopen("/proc/stat", "r", fr);

		int proc_count = 0;
		fprintf(fw, "time_s: %3ld", (long)(time(NULL) - start));
		while ((read = getline(&line, &len, fr)) != -1) {
			if (line[0] == 'c' && line[1] == 'p' && line[2] == 'u' && line[3] != ' ') {
				//printf("%s", line);
				int old_space = 0, count = 0;
				for (int i = 0; i < read; i++) {
					if (line[i] == ' ' || i == (read - 1)) {
						if (old_space == 0) {
							old_space = i;
							continue;
						}
						long new = strtol(&line[old_space + 1], NULL, 10);
						//printf("old=%ld,new=%ld,delta=%ld\n", cpu_stats[proc_count][count], new, new - cpu_stats[proc_count][count]);
						delta[count] = new - cpu_stats[proc_count][count]; 
						cpu_stats[proc_count][count] = new;
						//printf("%ld ", numb);
						old_space = i;
						count++;
					}
				}
				long delta_sum = 0;
				for (int i = 0; i < 10; i++)
					if (i != 3)
						delta_sum += delta[i];
				// TODO: delta_sum == 0
				double cpu_load = (double)delta[3] / (double)delta_sum;
				//printf("delta_sum=%ld, delta[3]=%ld, cpu_load=%f\n", delta_sum, delta[3], cpu_load);
				fprintf(fw, ", cpu[%d]: %9.6f", proc_count, cpu_load);
				proc_count++;
			}
		}

		fprintf(fw, "\n");
		fflush(fw);
		printf(".");
		fflush(stdout);
		nanosleep(&ts, NULL);
	
	}

	for (int i = 0; i < proc_num; i++) 
		free(cpu_stats[i]);
	free(cpu_stats);
	free(line);
	fclose(fr); 
	fclose(fw); 
	return 0;
}


int main() {
	
	cpu_stat(4, 1000);

	return 0;
}

