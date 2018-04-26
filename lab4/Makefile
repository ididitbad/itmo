
all: lab4 lab4-seq

lab4: lab4.c
	gcc lab4.c -std=gnu99 -g -O3 -Wall -o $@ -lm -fopenmp

lab4-seq: lab4.c
	gcc lab4.c -std=gnu99 -g -O3 -Wall -o $@ -lm 
