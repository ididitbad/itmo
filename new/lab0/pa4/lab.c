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

#define USAGE "usage: ./lab -p X [--mutexl], where X is a number of accounts"
#define DELAY 100000
#define MAX_TIME 10000

int numb;
int X = 0;
int N = 1;
int fd_wr[MAX_PROCESS_ID + 1][MAX_PROCESS_ID + 1];
int fd_rd[MAX_PROCESS_ID + 1][MAX_PROCESS_ID + 1];
timestamp_t time = 0;

int mutexl = 0;
timestamp_t query[MAX_PROCESS_ID + 1] = {MAX_TIME};
int n_proc_start = 0;
int n_proc_done = 0;

FILE* pipes; 
FILE* events; 

void child(int numb, int N);

void panic(char * message)
{
	perror(message);
	exit(EXIT_FAILURE);
}

void make_message(Message * msg, MessageType type, const void * data, size_t size, timestamp_t time)
{
	msg->s_header.s_magic = MESSAGE_MAGIC;
	msg->s_header.s_payload_len = size;
	msg->s_header.s_type = type;
	msg->s_header.s_local_time = time;
	memcpy(msg->s_payload, data, size);
}

void make_pipes(int N, int numb)
{
	int i, j, tmp[2]; 
	for (i = 0; i < N; i++) {
		for (j = 0; j < N; j++)
		{
			if (i != j)
			{
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

void close_pipes(int N, int numb)
{
	int i, j;
	for (i = 0; i < N; i++) {
		for (j = 0; j < N; j++) {
			if (i != j && i != numb)
			{
				close(fd_wr[i][j]);
				close(fd_rd[i][j]);
				fprintf(pipes, "PIPE CLOSED\tprocess %d -> process %d : from fd[%d] to fd[%d]\t BY PROCESS %d\n", 
									i, j, fd_wr[i][j], fd_rd[j][i], numb);
				fflush(pipes);
			}
		}
	}
} 

int set_lamport_time(timestamp_t rec_time)
{
	if (rec_time > time)
	{
		time = rec_time;
		time++;
		return 1;
	}
	else 
	{
		time++;
		return 0;
	}
}

timestamp_t get_lamport_time()
{
	return time;
}

int main (int argc, char * argv[])
{	
	pid_t parent;
	time = 0;

	if (argc < 3) panic(USAGE);

	if (argc == 4 && strcmp(argv[1], "--mutexl") == 0) mutexl = 1;
	//if ((argc == 3 || argc == 4) && argv[1][0] == '-' && argv[1][1] == 'p' && argv[1][2] == 0) 
	if (mutexl && argv[2][0] == '-' && argv[2][1] == 'p' && argv[2][2] == 0) {
		X = atoi(argv[3]);
	}
	else if (!mutexl && argv[1][0] == '-' && argv[1][1] == 'p' && argv[1][2] == 0) {
		X = atoi(argv[2]);
	}
	else panic(USAGE);
	
	if (X > MAX_PROCESS_ID) 
	{
		fprintf(stderr, "X should be less than %d\n", MAX_PROCESS_ID);
		exit(1);
	}
	
	N = X + 1;

	if ((events = fopen(events_log, "a")) == NULL) panic("events_log file");
	if ((pipes = fopen(pipes_log, "a")) == NULL) panic("pipes_log file");
	setbuf(events, 0);
	setbuf(pipes, 0);

	make_pipes(N, PARENT_ID);
	
	for (numb = 1; numb <= X; numb++)
	{
		if ((parent = fork()) == -1) printf("fork");
		
		if (!parent)
		{
			child(numb, N);
			exit(0);
		}
	}

	numb = 0;
	close_pipes(N, numb);
	
	Message msg;
	
	while (n_proc_start < X && n_proc_done < X)
	{
		usleep(DELAY);
		if (receive_any(NULL, &msg) > 0){
			set_lamport_time(msg.s_header.s_local_time);
			if (msg.s_header.s_type == STARTED) n_proc_start++;
			else if (msg.s_header.s_type == DONE) n_proc_done++;
		}	
	}

	while (X--) wait(0);
	fclose(events);
	fclose(pipes);

	return 0;
}

void child(int numb, int N)
{
	balance_t balance = 0;
	time = 0;

	Message msg;
	char buf[50];
	int i;
	
	close_pipes(N, numb);
	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, log_started_fmt, get_lamport_time(), numb, getpid(), getppid(), balance);
	fputs(buf, events);

	set_lamport_time(-1);
	make_message(&msg, STARTED, buf, sizeof(buf), get_lamport_time());
	send_multicast(NULL, &msg);		
	
	while (n_proc_start < N - 2) {
		usleep(DELAY);
		if (receive_any(NULL, &msg) > 0)
		{
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
	
	for (i = 1; i <= 5 * numb; i++)
	{ 
		if (mutexl) request_cs(NULL);

		memset(buf, 0, sizeof(buf));
		sprintf(buf, log_loop_operation_fmt, numb, i, 5 * numb);

		print(buf);

		if (mutexl) release_cs(NULL);
	}

	memset(buf, 0, sizeof(buf));
	sprintf(buf, log_done_fmt, get_lamport_time(), numb, balance);
	fputs(buf, events);

	set_lamport_time(-1);
	make_message(&msg, DONE, buf, sizeof(buf), get_lamport_time());
	send_multicast(NULL, &msg);

	while (n_proc_done < N - 2) {
		usleep(DELAY);
		if (receive_any(NULL, &msg) > 0)
		{
			set_lamport_time(msg.s_header.s_local_time);
			if (msg.s_header.s_type == DONE) {
				n_proc_done++;
			}
		}
	}
	fprintf(events, log_received_all_done_fmt, get_lamport_time(), numb);

}

int send(void * io, local_id dst, const Message * msg)
{
	if (write(fd_wr[numb][dst], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len) < 0)
	{
		fprintf(stderr, "from process %d to process %d : write failed\n", numb, dst);
		return -1;
	}
	return 1;
}

int send_multicast(void * io, const Message * msg)
{
	int ret = 0;
	local_id i;
	for (i = 0; i < N; i++)
	{
		if (i != numb) {
			if (send(NULL, i, msg) != 0) {
				ret = 1;
			}
		}
	}
	return ret;
}

int receive(void * io, local_id from, Message * msg)
{
	int r = 0;
	if ((r = read(fd_rd[numb][from], msg, sizeof(MessageHeader))) <= 0)
	{
		//fprintf(stderr, "process %d from process %d : read header failed\n", numb, from);
		return -1;
	}
	if ((r = read(fd_rd[numb][from], msg->s_payload, msg->s_header.s_payload_len)) < 0)
	{
		fprintf(stderr, "process %d from process %d : read body failed\n", numb, from);
		return -1;
	}
	return 1;
}

int receive_any(void * io, Message * msg)
{
	local_id i;
	for (i = 0; i < N; i++)
	{
		if (i != numb) {
			if (receive(NULL, i, msg) > 0) {
				return i;
			}
		}
	}
	return -1;
}

int request_cs(const void * self)
{
	Message msg;
	int i, rec, answers = 0, access = 0;
	timestamp_t rec_time;

	set_lamport_time(-1);
	query[numb] = get_lamport_time();
	make_message(&msg, CS_REQUEST, NULL, 0, get_lamport_time());
	send_multicast(NULL, &msg);

	while (answers < X - 1 - n_proc_done || !access)
	{
		if (!answers && access) break;
		usleep(DELAY);
		if ((rec = receive_any(NULL, &msg)) > 0)
		{
			rec_time = msg.s_header.s_local_time;
			set_lamport_time(rec_time);

			if (msg.s_header.s_type == CS_REPLY)
			{
				answers++;
			}
			else if (msg.s_header.s_type == CS_REQUEST)
			{
				query[rec] = rec_time;

				set_lamport_time(-1);
				make_message(&msg, CS_REPLY, NULL, 0, get_lamport_time());
				send(NULL, rec, &msg);
			}
			else if (msg.s_header.s_type == CS_RELEASE)
			{
				query[rec] = MAX_TIME;
			}
			else if (msg.s_header.s_type == DONE)
			{
				n_proc_done++;
				query[rec] = MAX_TIME;
			}
		}

		if (answers >= X - 1 - n_proc_done || n_proc_done == X - 1)
		{
			access = 1;
			for (i = 1; i <= X; i++)
			{
				if (i == numb) continue;
				if (query[numb] > query[i] || (query[numb] == query[i] && numb > i)) 
				{
					access = 0;
					break;
				}
			}
		}
	}
	return 1;
}

int release_cs(const void * self)
{
	Message msg;
	query[numb] = MAX_TIME;
	set_lamport_time(-1);
	make_message(&msg, CS_RELEASE, NULL, 0, get_lamport_time());
	send_multicast(NULL, &msg);
	return 1;
}

