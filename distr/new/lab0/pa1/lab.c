#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include "common.h"
#include "ipc.h"
#include "pa1.h"

#define USAGE "usage: ./lab1 [-p X], where X is a number of child processes"

int fd_wr[MAX_PROCESS_ID][MAX_PROCESS_ID];
int fd_rd[MAX_PROCESS_ID][MAX_PROCESS_ID];
int numb = PARENT_ID;
int X = 0;
int N;
int n_proc_start = 0;
int n_proc_done = 0;
int16_t clicker = 0;

void panic(char *message)
{
	perror(message);
	exit(EXIT_FAILURE);
}
		
int main(int argc, char **argv)
{
	FILE* events;
	FILE* pipes;
	pid_t parent;
	int tmp[2], i, j;
	Message msg;
	
	if (argc == 3 && argv[1][0] == '-' && argv[1][1] == 'p' && argv[1][2] == 0) X = atoi(argv[2]); 
	else panic(USAGE);
	
	if (X > MAX_PROCESS_ID) 
	{
		fprintf(stderr, "X should be less than %d\n", MAX_PROCESS_ID);
		exit(1);
	}
	
	if ((events = fopen(events_log, "a")) == NULL) panic("events_log file");
	if ((pipes = fopen(pipes_log, "a")) == NULL) panic("pipes_log file");
	
	N = X + 1;
	
	for (i = 0; i < N; i++)
		for (j = 0; j < N; j++)
			if (i != j)
			{
				if (pipe(tmp) == -1) panic("pipe");
				fd_wr[i][j] = tmp[1];
				fd_rd[j][i] = tmp[0];
				if (fcntl(fd_wr[i][j], F_SETFL, O_NONBLOCK) == -1) panic("fcntl");
				if (fcntl(fd_rd[j][i], F_SETFL, O_NONBLOCK) == -1) panic("fcntl");
				fprintf(pipes, "process %d -> process %d : from fd[%d] to fd[%d]\n", i, j, fd_wr[i][j], fd_rd[j][i]);
			}
	
	fclose(pipes);
	
	for (numb = 1; numb <= X; numb++)
	{
		if ((parent = fork()) == (pid_t)-1) panic("fork");
		
		if (!parent)
		{
			clicker = n_proc_done = n_proc_start = 0;
		
			clicker++;
			fprintf(events, log_started_fmt, numb, getpid(), getppid());
			
			for (i = 0; i < N; i++)
				for (j = 0; j < N; j++)
				{
					//if (j == 0) close(fd_rd[i][j]);
					if (i != j && i != numb)
					{
						close(fd_wr[i][j]);
						close(fd_rd[i][j]);
					}
				}
			
			msg.s_header.s_magic = MESSAGE_MAGIC;
			msg.s_header.s_payload_len = sprintf(msg.s_payload, log_started_fmt, numb, getpid(), getppid());
			msg.s_header.s_type = STARTED;
			msg.s_header.s_local_time = clicker++;
			if (send_multicast(NULL, &msg) == 1) fprintf(stderr, "Did not send all STARTED from process %d\n", numb);
			
			while (n_proc_start < N - 2) 
				if (receive_any(NULL, &msg) == 0)
				{
					clicker = clicker >= msg.s_header.s_local_time ? clicker : msg.s_header.s_local_time;
					clicker++;
					if (msg.s_header.s_type == STARTED) n_proc_start++;
					else if (msg.s_header.s_type == DONE) n_proc_done++;
					else fprintf(stderr, "Unknown message type\n");
				}
			
			fprintf(events, log_received_all_started_fmt, numb);
			
			clicker++;
			fprintf(events, log_done_fmt, numb);
			
			msg.s_header.s_magic = MESSAGE_MAGIC;
			msg.s_header.s_payload_len = sprintf(msg.s_payload, log_done_fmt, numb);
			msg.s_header.s_type = DONE;
			msg.s_header.s_local_time = clicker++;
			if (send_multicast(NULL, &msg) == 1) fprintf(stderr, "Did not send all DONE from process %d\n", numb);
			
			while (n_proc_done < N - 2) 
				if (receive_any(NULL, &msg) == 0)
				{
					clicker = clicker >= msg.s_header.s_local_time ? clicker : msg.s_header.s_local_time;
					clicker++;
					if (msg.s_header.s_type == DONE) n_proc_done++;
				}
			
			fprintf(events, log_received_all_done_fmt, numb);
			
			exit(0);
		}
	}
	
	numb = PARENT_ID;
	n_proc_done = n_proc_start = 0;
	
	for (i = 0; i < N; i++)
		for (j = 0; j < N; j++)
			if (i != j && i != numb)
			{
				close(fd_wr[i][j]);
				//if (i != numb) close(fd_rd[i][j]);
				close(fd_rd[i][j]);
			}
			
	for (i = 0; i < 2 * X; i++) receive_any(NULL, &msg);
	
	while (X--) wait(0);
	
	fclose(events);
	
	return 0;
}

int send(void * self, local_id dst, const Message * msg)
{
	if (write(fd_wr[numb][dst], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len) == -1)
	{
		fprintf(stderr, "from process %d to process %d : write failed\n", numb, dst);
		return 1;
	}
	return 0;
}

int send_multicast(void * self, const Message * msg)
{
	int ret = 0;
	local_id i;
	for (i = 0; i < N; i++)
		if (i != numb)
			if (send(NULL, i, msg) != 0)
				ret = 1;
	return ret;
}

int receive(void * self, local_id from, Message * msg)
{
	if (read(fd_rd[numb][from], msg, sizeof(MessageHeader)) == -1)
	{
		//fprintf(stderr, "process %d from process %d : read header failed\n", numb, from);
		return 1;
	}
	if (read(fd_rd[numb][from], msg->s_payload, msg->s_header.s_payload_len) == -1)
	{
		fprintf(stderr, "process %d from process %d : read body failed\n", numb, from);
		return 1;
	}
	return 0;
}

int receive_any(void * self, Message * msg)
{
	local_id i;
	while (1)
	{
		usleep(1000);
		for (i = 1; i < N; i++)
			if (i != numb)
				if (receive(NULL, i, msg) == 0)
					return 0;
	}
}
