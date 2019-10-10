#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include "common.h"
#include "pa2345.h"
#include "banking.h"
#include "ipc.h"

#define USAGE "usage: ./lab [--mutexl] -p X, where X is a number of accounts"
#define DELAY 10000
#define VERBOSE 1

int numb;
int X = 0;
int N = 1;
int fd_wr[MAX_PROCESS_ID + 1][MAX_PROCESS_ID + 1];
int fd_rd[MAX_PROCESS_ID + 1][MAX_PROCESS_ID + 1];
timestamp_t time = 0;

int mutexl = 0;
timestamp_t DR[MAX_PROCESS_ID + 1] = {0};
int n_proc_start = 0;
int n_proc_done = 0;

FILE* pipes; 
FILE* events;

enum {
	THINKING, HUNGRY, EATING
};
int forks[MAX_PROCESS_ID + 1] = {0}; 
int dirty[MAX_PROCESS_ID + 1] = {0}; 
int reqf[MAX_PROCESS_ID + 1] = {0}; 

void child(int numb, int N);

void panic(char * message) {
	perror(message);
	exit(EXIT_FAILURE);
}

void make_message(Message * msg, MessageType type, const void * data, size_t size, timestamp_t time) {
	msg->s_header.s_magic = MESSAGE_MAGIC;
	msg->s_header.s_payload_len = size;
	msg->s_header.s_type = type;
	msg->s_header.s_local_time = time;
	memcpy(msg->s_payload, data, size);
}

void make_pipes(int N, int numb) {
	int i, j, tmp[2]; 
	for (i = 0; i < N; i++) {
		for (j = 0; j < N; j++) {
			if (i != j) {
				if (pipe(tmp) == -1) panic("pipe");
				fd_wr[i][j] = tmp[1];
				fd_rd[j][i] = tmp[0];
				if (fcntl(fd_wr[i][j], F_SETFL, O_NONBLOCK) == -1) panic("fcntl");
				if (fcntl(fd_rd[j][i], F_SETFL, O_NONBLOCK) == -1) panic("fcntl");
				fprintf(pipes, "PIPE CREATED\tprocess %d -> process %d : from fd[%d] to fd[%d]\tBY PROCESS %d\n", 
									i, j, fd_wr[i][j], fd_rd[j][i], numb);
				fflush(pipes);
			}
		}
	}		
}

void close_pipes(int N, int numb) {
	int i, j;
	for (i = 0; i < N; i++) {
		for (j = 0; j < N; j++) {
			if (i != j && i != numb) {
				close(fd_wr[i][j]);
				close(fd_rd[i][j]);
				fprintf(pipes, "PIPE CLOSED\tprocess %d -> process %d : from fd[%d] to fd[%d]\t BY PROCESS %d\n", 
									i, j, fd_wr[i][j], fd_rd[j][i], numb);
				fflush(pipes);
			}
		}
	}
} 

int set_lamport_time(timestamp_t rec_time) {
	int ret;
	if (rec_time > time) {
		time = rec_time;
		ret = 1;
	}
	else if (rec_time == time) ret = 0;
	else ret = -1;

	time++;
	return ret;
}

timestamp_t get_lamport_time() {
	return time;
}

int main(int argc, char * argv[]) {
	pid_t parent;
	time = 0;

	if (argc < 3) panic(USAGE);

	if (argc == 4 && strcmp(argv[1], "--mutexl") == 0) mutexl = 1;

	if (mutexl && argv[2][0] == '-' && argv[2][1] == 'p' && argv[2][2] == 0) {
		X = atoi(argv[3]);
	}
	else if (!mutexl && argv[1][0] == '-' && argv[1][1] == 'p' && argv[1][2] == 0) {
		X = atoi(argv[2]);
	}
	else panic(USAGE);
	
	if (X > MAX_PROCESS_ID) {
		fprintf(stderr, "X should be less than %d\n", MAX_PROCESS_ID);
		exit(1);
	}
	
	N = X + 1;

	if ((events = fopen(events_log, "a")) == NULL) panic("events_log file");
	if ((pipes = fopen(pipes_log, "a")) == NULL) panic("pipes_log file");
	setbuf(events, 0);
	setbuf(pipes, 0);

	make_pipes(N, PARENT_ID);
	
	for (numb = 1; numb <= X; numb++) {
		if ((parent = fork()) == -1) printf("fork");
		
		if (!parent) {
			child(numb, N);
			if (VERBOSE) printf("process %d exited\n", numb);
			exit(0);
		}
	}

	numb = 0;
	close_pipes(N, numb);
	
	Message msg;
	
	while (n_proc_start < X && n_proc_done < X) {
		usleep(DELAY);
		if (receive_any(NULL, &msg) > 0){
			set_lamport_time(msg.s_header.s_local_time);
			if (msg.s_header.s_type == STARTED) n_proc_start++;
			else if (msg.s_header.s_type == DONE) n_proc_done++;
		}	
	}

	while (X--) {
		int s;
		pid_t t = wait(&s);
		if (VERBOSE) printf("pid %d end with status %d\n", t, WEXITSTATUS(s));
	}
	if (VERBOSE) printf("closing this shit\n");
	fclose(events);
	fclose(pipes);

	return 0;
}

void child(int numb, int N) {
	balance_t balance = 0;
	time = 0;

	Message msg;
	char buf[50];
	int rec;
	
	close_pipes(N, numb);
	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, log_started_fmt, get_lamport_time(), numb, getpid(), getppid(), balance);
	fputs(buf, events);
	if (VERBOSE) fputs(buf, stdout);

	set_lamport_time(-1);
	make_message(&msg, STARTED, buf, sizeof(buf), get_lamport_time());
	send_multicast(NULL, &msg);		
	
	while (n_proc_start < N - 2) {
		usleep(DELAY);
		if (receive_any(NULL, &msg) > 0) {
			set_lamport_time(msg.s_header.s_local_time);
			if (msg.s_header.s_type == STARTED) {
				n_proc_start++;
			}
			else if (msg.s_header.s_type == DONE) {
				n_proc_done++;
			}
		}
	}
	fprintf(events, log_received_all_started_fmt, get_lamport_time(), numb);
	if (VERBOSE) fprintf(stdout, log_received_all_started_fmt, get_lamport_time(), numb);
	
	for (int i = numb; i < N; i++) {
		forks[i] = 1;
		dirty[i] = 1;
		reqf[i] = 0;
	}
	for (int i = 1; i < numb; i++) {
		reqf[i] = 1;
	}

	for (int i = 1; i <= 5 * numb; i++) {
		if (mutexl) request_cs(NULL);

		if (VERBOSE) printf("process %d in cs\n", numb);

		memset(buf, 0, sizeof(buf));
		sprintf(buf, log_loop_operation_fmt, numb, i, 5 * numb);

		if (VERBOSE == 0) print(buf);

		if (mutexl) release_cs(NULL);
		if (VERBOSE) printf("process %d exited release_cs\n", numb);
	}

	memset(buf, 0, sizeof(buf));
	sprintf(buf, log_done_fmt, get_lamport_time(), numb, balance);
	fputs(buf, events);
	if (VERBOSE) fputs(buf, stdout);

	set_lamport_time(-1);
	make_message(&msg, DONE, buf, sizeof(buf), get_lamport_time());
	send_multicast(NULL, &msg);

	return;

	while (n_proc_done < N - 2) {
		usleep(DELAY);
		if (VERBOSE) printf("process %d done cycle\n", numb);
		if ((rec = receive_any(NULL, &msg)) > 0) {
			set_lamport_time(msg.s_header.s_local_time);
			if (msg.s_header.s_type == DONE) {
				n_proc_done++;
			}
			else if (msg.s_header.s_type == CS_REQUEST) {
				set_lamport_time(-1);
				make_message(&msg, CS_REPLY, NULL, 0, get_lamport_time());
				send(NULL, rec, &msg);
				// reqf[rec] = 0;
			}

		}
	}
	fprintf(events, log_received_all_done_fmt, get_lamport_time(), numb);
	if (VERBOSE) fprintf(stdout, log_received_all_done_fmt, get_lamport_time(), numb);
	fflush(events);
	fflush(stdout);
}

int request_cs(const void * self) {
	Message msg;
	int rec, clean_forks = 0;
	timestamp_t request_time, received_time;

	if (VERBOSE) printf("process %d in request_cs\n", numb);

	for (int i = 1; i < N; i++) {
		if (i != numb && forks[i] == 0) {
			if (VERBOSE) printf("process %d do not have fork %d\n", numb, i);
			
			if (reqf[i] != -1) reqf[i] = 1;

			set_lamport_time(-1);
			make_message(&msg, CS_REQUEST, NULL, 0, get_lamport_time());
			send(NULL, i, &msg);
		}
	}

	request_time = get_lamport_time();

	while (clean_forks < (X - 1 - n_proc_done)) {
		usleep(DELAY);

		// for (int i = 1; i < N; i++) {
		// 	if (i != numb && forks[i] == 0) {
		// 		printf("process %d do not have fork %d\n", numb, i);
		// 		reqf[i] = 1;
		// 		set_lamport_time(-1);
		// 		make_message(&msg, CS_REQUEST, NULL, 0, get_lamport_time());
		// 		send(NULL, i, &msg);
		// 	}
		// }

		int dirty_forks = 0;
		clean_forks = 0;
		for (int i = 0; i < N; i++) {
			if (i != numb && forks[i] == 1 && reqf[i] != -1) {
			 	if (dirty[i] == 1) {
					dirty_forks++;
				}
				else {
					clean_forks++;
				}
			}
		}
		if (VERBOSE) printf("process %d has %d clean_forks and %d dirty_forks\n", numb, clean_forks, dirty_forks);
		
		if ((rec = receive_any(NULL, &msg)) > 0) {
			received_time = msg.s_header.s_local_time;
			set_lamport_time(received_time);

			if (msg.s_header.s_type == CS_REPLY) {
				if (VERBOSE) printf("process %d received fork from %d\n", numb, rec);

				clean_forks++;
				forks[rec] = 1;
				dirty[rec] = 0;
				// if (numb < rec) reqf[rec] = 0;
				// else reqf[rec] = 1;
			}
			else if (msg.s_header.s_type == CS_REQUEST) {
				// if (request_time > received_time || (request_time == received_time && numb > rec)) {
				if (forks[rec] == 1 && dirty[rec] == 1) {
					set_lamport_time(-1);
					make_message(&msg, CS_REPLY, NULL, 0, get_lamport_time());
					send(NULL, rec, &msg);
					forks[rec] = 0;
					dirty[rec] = 0;
					reqf[rec] = 0;
				}
				else {
					reqf[rec] = 1;
				}
			}
			else if (msg.s_header.s_type == DONE) {
				n_proc_done++;
				reqf[rec] = -1;
			}
		}

	}
	if (VERBOSE) printf("process %d entering cs\n", numb);

	return 1;
}

int release_cs(const void * self) {
	Message msg;

	if (VERBOSE) printf("process %d in release_cs\n", numb);

	for (int i = 1; i <= X; i++) {
		if (i != numb) { 
			if (reqf[i] == 1) {
				set_lamport_time(-1);
				make_message(&msg, CS_REPLY, NULL, 0, get_lamport_time());
				send(NULL, i, &msg);
				reqf[i] = 0;
				forks[i] = 0;
				dirty[i] = 0;
			}
			else if (forks[i] == 1) {
				dirty[i] = 1;
			}
		}
	}

	return 1;
}

int send(void * io, local_id dst, const Message * msg) {

	if (VERBOSE) {
		if (msg->s_header.s_type == CS_REQUEST)
			fprintf(stdout, "request from process %d to process %d\n", numb, dst);
		if (msg->s_header.s_type == CS_REPLY)
			fprintf(stdout, "reply from process %d to process %d\n", numb, dst);
		if (msg->s_header.s_type == DONE)
			fprintf(stdout, "done from process %d to process %d\n", numb, dst);
	
	}

	if (write(fd_wr[numb][dst], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len) < 0) {
		fprintf(stderr, "from process %d to process %d : write failed\n", numb, dst);
		return -1;
	}
	return 1;
}

int send_multicast(void * io, const Message * msg) {
	int ret = 0;
	local_id i;
	for (i = 0; i < N; i++) {
		if (i != numb) {
			if (send(NULL, i, msg) != 0) {
				ret = 1;
			}
		}
	}
	return ret;
}

int receive(void * io, local_id from, Message * msg) {
	int r = 0;
	if ((r = read(fd_rd[numb][from], msg, sizeof(MessageHeader))) <= 0) {
		//fprintf(stderr, "process %d from process %d : read header failed\n", numb, from);
		return -1;
	}
	if ((r = read(fd_rd[numb][from], msg->s_payload, msg->s_header.s_payload_len)) < 0) {
		fprintf(stderr, "process %d from process %d : read body failed\n", numb, from);
		return -1;
	}
	return 1;
}

int receive_any(void * io, Message * msg) {
	local_id i;
	for (i = 0; i < N; i++) {
		if (i != numb) {
			if (receive(NULL, i, msg) > 0) {
				return i;
			}
		}
	}
	return -1;
}
