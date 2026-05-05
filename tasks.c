#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <syslog.h>

typedef struct  
{
	size_t time;
	size_t pipe_amount;
	char **prog_names;
	int info;
	struct Task *next;
} Task;

volatile sig_atomic_t running_flag=1;
volatile sig_atomic_t read_flag = 1;
volatile sig_atomic_t log_flag = 0;

void signal_handler(int signal)
{
	if (signal == SIGINT)
	{
		running_flag=0;
	}
	if (signal == SIGUSR1)
	{
		read_flag=1;
	}
	if (signal == SIGUSR2)
	{
		log_flag = 1;
	}
}

void free_all(Task* curr) 
{
	while(curr){
	Task *help = curr;
	curr = curr->next;
	free_task(help);
	}
}

static void demonize()
{
	pid_t pid;
	
	pid = fork();
	if (pid < 0)
	{
		exit(EXIT_FAILURE);
	}
	
	if (pid > 0)
	{
		exit(EXIT_SUCCESS);
	}
	
	if(setsid() < 0)
	{
		exit(EXIT_FAILURE);
	}
	
	signal(SIGCHLD, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, signal_handler);
	signal(SIGUSR1, signal_handler);
	signal(SIGUSR2, signal_handler);
	
	pid = fork();
	if (pid < 0)
	{
		exit(EXIT_FAILURE);
	}
	
	if (pid > 0)
	{
		exit(EXIT_SUCCESS);
	}
	
	umask(0);
	
	chdir("/");
	
	int x;
	for(x = sysconf(_SC_OPEN_MAX);x>=0; x--)
	{
		close(x);
	}
	
	openlog("mini-cron", LOG_PID, LOG_DAEMON);
}

Task* create_task(int t, int p, const char **cmd, int info) {
	Task *new_task = malloc(sizeof(Task));
	new_task->time = t;
	new_task->pipe_amount = p;
	new_task->info = info;
	new_task->next = NULL;
    new_task->prog_names = malloc((p+1) * sizeof(char *));
    
    if (new_task->prog_names) {
        for (int i = 0; i < p+1; i++) {
            new_task->prog_names[i] = strdup(cmd[i]);
        }
    }

    return new_task;
}

void free_task(Task *task) {
    if (!task) return;

    if (Task->prog_names) {
        for (size_t i = 0; i < (Task->pipe_amount + 1); i++) {
            free(Task->prog_names[i]);
        }
        free(Task->prog_names);
    }

    free(Task);
}



int main(int argc, char *argv[]) {
	if(argc!=4){
	fprintf(stderr, "Usage: %s <> <>", argv[0]);
	return 1;
	}
	
	Task curr;
	while(curr&&running_flag&&read_flag)
	{
	read_flag = 0;
	/*wczytaj, sparsuj i sortując przez wstawianie stwórz stos FIFO ustawiając Task* curr i Task* next */
	
	while(curr&&running_flag)
	{
		sleep(curr->time);
		/*utwórz n procesów i połącz je pipe()*/
		
		Task* help = curr;
		curr = curr->next;
		free_task(help);
	}
	free_all(curr);
	}
	return 0;
}
