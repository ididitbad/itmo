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

#define USAGE "usage: ./lab -p X S1 S2 ... SX  , where X is a number of accounts and S[i] is a balance of account i"

int numb;
int X = 0;
int N = 1;
int fd_wr[MAX_PROCESS_ID][MAX_PROCESS_ID];
int fd_rd[MAX_PROCESS_ID][MAX_PROCESS_ID];
int S[MAX_PROCESS_ID];
timestamp_t time = 0;
unsigned int DELAY = 100000;

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
	AllHistory all;  
	pid_t parent;
	time = 0;
	int i = 0;

	all.s_history_len = 1;
	
	if (argc > 3 && argv[1][0] == '-' && argv[1][1] == 'p' && argv[1][2] == 0) 
	{
		X = atoi(argv[2]);
		for (i = 1; i <= X; i++)
		{
			if (!argv[i + 2]) panic(USAGE);
			else S[i] = atoi(argv[i + 2]);
		}
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
	fseek(events, SEEK_SET, 0);
	fseek(pipes, SEEK_SET, 0);
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
	
	int n_proc_start = 0;
	int n_proc_done = 0;
	int n_histories = 0;
	Message msg;
	
	while (n_proc_start < X)
	{
		usleep(DELAY);
		if (receive_any(NULL, &msg) == 0){
			set_lamport_time(msg.s_header.s_local_time);
			n_proc_start++;
		}	
	}

	bank_robbery(NULL, X);
	
	set_lamport_time(-1);
	make_message(&msg, STOP, NULL, 0, get_lamport_time());
	send_multicast(NULL, &msg);

	all.s_history_len = X;
	BalanceHistory * balance = (BalanceHistory*) msg.s_payload;

	while(1)
	{
		if (receive_any(NULL, &msg) == 0)
		{
			set_lamport_time(msg.s_header.s_local_time);
			if (msg.s_header.s_type == BALANCE_HISTORY)
			{
				memcpy(&all.s_history[balance->s_id - 1], msg.s_payload, sizeof(BalanceHistory));
				n_histories++;
			}
			else if (msg.s_header.s_type == DONE) {
				n_proc_done++;
			}
			if (n_histories == X) break;
		}
		else usleep(DELAY);
	}

	print_history(&all);

	while (X--) wait(0);
	
	fclose(events);
	fclose(pipes);
	return 0;
}

void make_history(BalanceHistory* history, timestamp_t t, balance_t balance, balance_t pending)
{
	history->s_history[t].s_balance = balance;
	history->s_history[t].s_time = t;
	history->s_history[t].s_balance_pending_in = pending;
	history->s_history_len = t + 1;
}

void child(int numb, int N)
{
	balance_t balance = S[numb];
	
	time = 0;

	BalanceHistory history;
	history.s_id = numb;
	history.s_history[0].s_balance = balance;
	history.s_history_len = 0;
	
	TransferOrder * order;
	int n_proc_start = 0;
	int n_proc_done = 0;
	Message msg;
	char buf[50];
	timestamp_t t, i;
	
	close_pipes(N, numb);
	
	memset(buf, 0, sizeof(buf));
	sprintf(buf, log_started_fmt, get_lamport_time(), numb, getpid(), getppid(), balance);
	fputs(buf, events);

	set_lamport_time(-1);
	make_message(&msg, STARTED, buf, sizeof(buf), get_lamport_time());
	send_multicast(NULL, &msg);		
	
	while (1)
	{
		if (receive_any(NULL, &msg) == 0)
		{
			set_lamport_time(msg.s_header.s_local_time);
			if (msg.s_header.s_type == STARTED)
			{
				n_proc_start++;
				if (n_proc_start == N - 2) {
					fprintf(events, log_received_all_started_fmt, get_lamport_time(), numb);
				}
			}
			else if (msg.s_header.s_type == TRANSFER)
			{
				order = (TransferOrder * )msg.s_payload;
				if (order->s_src == numb)
				{
					balance -= order->s_amount;

					set_lamport_time(-1);
					msg.s_header.s_local_time = get_lamport_time();
					send(NULL, order->s_dst, &msg);

					fprintf(events, log_transfer_out_fmt, get_lamport_time(), numb, order->s_amount, order->s_dst);
					make_history(&history, get_lamport_time(), balance, 0);
				}
				else if (order->s_dst == numb)
				{
					fprintf(events, log_transfer_in_fmt, get_lamport_time(), numb, order->s_amount, order->s_src);
					for (t = msg.s_header.s_local_time; t < get_lamport_time(); t++) {
						make_history(&history, t, balance, order->s_amount);
					}
					balance += order->s_amount;
					make_history(&history, get_lamport_time(), balance, 0);

					set_lamport_time(-1);
					make_message(&msg, ACK, NULL, 0, get_lamport_time());
					send(NULL, 0, &msg);
				}
			}
			else if (msg.s_header.s_type == STOP)
			{
				memset(buf, 0, sizeof(buf));
				sprintf(buf, log_done_fmt, get_lamport_time(), numb, balance);
				fputs(buf, events);

				set_lamport_time(-1);
				make_message(&msg, DONE, buf, sizeof(buf), get_lamport_time());
				send_multicast(NULL, &msg);
			}
			else if (msg.s_header.s_type == DONE)
			{
				n_proc_done++;
				if (n_proc_done == N - 2) 
				{
					fprintf(events, log_received_all_done_fmt, get_lamport_time(), numb);
					set_lamport_time(-1);
	
					t = get_lamport_time();
					for (i = 1; i <= t; i++)
					{
						if (history.s_history[i].s_time != i)
						{	
							history.s_history[i].s_time = i;
							history.s_history[i].s_balance_pending_in = history.s_history[i - 1].s_balance_pending_in;
							history.s_history[i].s_balance = history.s_history[i - 1].s_balance;		
						}
					}
					history.s_history_len = t + 1;
					

					make_message(&msg, BALANCE_HISTORY, &history, sizeof(history), get_lamport_time());
					send(NULL, 0, &msg);
					break;
				}
			}
		}
		usleep(DELAY);
	}
}

int send(void * io, local_id dst, const Message * msg)
{
	if (write(fd_wr[numb][dst], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len) == -1)
	{
		fprintf(stderr, "from process %d to process %d : write failed\n", numb, dst);
		return 1;
	}
	return 0;
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
		return 1;
	}
	if ((r = read(fd_rd[numb][from], msg->s_payload, msg->s_header.s_payload_len)) < 0)
	{
		fprintf(stderr, "process %d from process %d : read body failed\n", numb, from);
		return 1;
	}
	return 0;
}

int receive_any(void * io, Message * msg)
{
	local_id i;
	for (i = 0; i < N; i++)
	{
		if (i != numb) {
			if (receive(NULL, i, msg) == 0) {
				return 0;
			}
		}
	}
	return 1;
}

void transfer(void * parent_data, local_id src, local_id dst, balance_t amount)
{
	Message msg;
	TransferOrder order;
	
	order.s_amount = amount;
	order.s_src = src;
	order.s_dst = dst;
	
	set_lamport_time(-1);
	make_message(&msg, TRANSFER, &order, sizeof(TransferOrder), get_lamport_time());
	send(NULL, src, &msg);

	while (receive(NULL, dst, &msg) != 0) {
		usleep(DELAY);
	}
	set_lamport_time(msg.s_header.s_local_time);
}
