#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/wait.h>

char *taskfile;
char *output_filename;

typedef struct Task
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


void free_task(Task *task) {
    if (!task) return;

    if (task->prog_names) {
        for (size_t i = 0; i < (task->pipe_amount + 1); i++) {
            free(task->prog_names[i]);
        }
        free(task->prog_names);
    }

    free(task);
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

	signal(SIGHUP, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
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

	int fd0 = open("/dev/null", O_RDWR);
	dup2(fd0, STDIN_FILENO);
	dup2(fd0, STDOUT_FILENO);
	dup2(fd0, STDERR_FILENO);
	if (fd0 > 2) close(fd0);

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

char** split_command(char *cmd) {
    int count = 0;
    char *copy = strdup(cmd);
    char *token = strtok(copy, " ");
    while (token) {
        count++;
        token = strtok(NULL, " ");
    }
    free(copy);

    char **args = malloc((count + 1) * sizeof(char *));
    copy = strdup(cmd);
    token = strtok(copy, " ");
    for (int i = 0; i < count; i++) {
        args[i] = strdup(token);
        token = strtok(NULL, " ");
    }
    args[count] = NULL;
    free(copy);
    return args;
}

void insert_sorted(Task **head, Task *new_node) {
    if (*head == NULL || (*head)->time >= new_node->time) {
        new_node->next = *head;
        *head = new_node;
    } else {
        Task *current = *head;
        while (current->next != NULL && current->next->time < new_node->time) {
            current = current->next;
        }
        new_node->next = current->next;
        current->next = new_node;
    }
}

Task* parse_taskfile(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return NULL;

    Task *head = NULL;
    char line[1024];

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;

        char *hours_s   = strtok(line, ":");
        char *minutes_s = strtok(NULL, ":");
        char *full_cmd  = strtok(NULL, ":");
        char *info_s    = strtok(NULL, ":");

        if (hours_s && minutes_s && full_cmd && info_s) {
            size_t seconds = (atoi(hours_s) * 3600) + (atoi(minutes_s) * 60);
            int info = atoi(info_s);

            size_t pipes = 0;
            for (int i = 0; full_cmd[i]; i++) {
                if (full_cmd[i] == '|') pipes++;
            }

            char **progs = malloc((pipes + 1) * sizeof(char *));
            char *command_copy = strdup(full_cmd);

            char *single_prog = strtok(command_copy, "|");
            size_t idx = 0;
            while (single_prog != NULL) {

                while(*single_prog == ' ') single_prog++;
                progs[idx++] = strdup(single_prog);
                single_prog = strtok(NULL, "|");
            }

            Task *new_node = create_task(seconds, pipes, (const char**)progs, info);

            if (new_node) {
                insert_sorted(&head, new_node);
            }

            free(command_copy);
            free(progs);
        }
    }

    fclose(file);
    return head;
}


int main(int argc, char *argv[]) {
	if(argc!=3){
		return 1;
	}

	taskfile = argv[1];
	output_filename = argv[2];
	demonize();
	Task *curr = NULL;

	while(running_flag)
	{
		// Obsługa SIGUSR1
		if (read_flag) {
			if (curr) free_all(curr);
			curr = parse_taskfile(taskfile);
			read_flag = 0;
			syslog(LOG_INFO, "Wczytano zadania ponownie.");
		}

		// Obsługa SIGUSR2
		if (log_flag) {
			Task *help = curr;
			if (!help) {
				syslog(LOG_INFO, "Brak zadan w kolejce");
			}

			while (help) {
				syslog(LOG_INFO, "Zadanie w kolejce, info: %d", help->info);
				help = help->next;
			}
			log_flag = 0;
		}

		// Wykonywanie Zadan
		if (curr) {
			time_t now = time(NULL);
			struct tm *t = localtime(&now);
			size_t seconds_now = t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec;
			int diff = (int)curr->time - (int)seconds_now;
			syslog(LOG_INFO, "Zadanie wykona się za: %d sekund", diff);
			if (diff < 0) {
				diff +=  86400;
			}

			while (diff > 0 && running_flag && !read_flag && !log_flag) {
				sleep(1);
				diff--;
			}

			if (read_flag || log_flag) {
				continue;
			}

			pid_t pid = fork();
			if (pid < 0) {
				syslog(LOG_ERR, "Blad przy forkowaniu");
				return 1;
			}

			else if (pid == 0) {
		    	int num_progs = curr->pipe_amount + 1;
		    	int pipefds[2 * (num_progs - 1)];

				setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);
				for (int i = 0; i < num_progs - 1; i++) {
		        		if (pipe(pipefds + i * 2) < 0) {
		            			syslog(LOG_ERR, "Blad tworzenia potoku");
		            			_exit(EXIT_FAILURE);
		        		}
		    		}

				int fd = open(output_filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
				if (fd == -1) {
					syslog(LOG_ERR, "Nie mozna otworzyc pliku wyjsciowego: %m");
					_exit(EXIT_FAILURE);
				}

				dprintf(fd, "Polecenie: ");
				for (int i = 0; i < num_progs; i++) dprintf(fd, "%s%s", curr-> prog_names[i], (i < num_progs - 1) ? " | " : "");
		    		dprintf(fd, "\n");
				fsync(fd);
				for (int i = 0; i < num_progs; i++) {
		        		pid_t p_pid = fork();
		        		if (p_pid == 0) {
						// łączenie stdin z stdout w potoku
		            			if (i > 0) dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
		            			if (i < num_progs - 1) dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
		            			else {
							// ostatnie polecenie w potoku
					                if (curr->info == 0 || curr->info == 2) dup2(fd, STDOUT_FILENO);
			      			}

						if (curr->info == 1 || curr->info == 2) dup2(fd, STDERR_FILENO);

						// Zamkniecie deskryptorow w dziecku
					        for (int j = 0; j < 2 * (num_progs - 1); j++) {
					        	close(pipefds[j]);
					        }
						close(fd);
						char *command_to_exec = curr->prog_names[i];
						while(*command_to_exec == ' ') command_to_exec++;

						char **args = split_command(command_to_exec);
						execvp(args[0], args);
					        syslog(LOG_ERR, "Blad wykonywania komendy");
					        _exit(EXIT_FAILURE);
					}
				}

				for (int j = 0; j < 2 * (num_progs - 1); j++) close(pipefds[j]);
				close(fd);
				int last_status = 0;
				for (int i = 0; i < num_progs; i++) {
					int temp_status;
					wait(&temp_status);
					if (i == num_progs - 1) last_status = temp_status; // status ostatniego programu w potoku
				}

				if (WIFEXITED(last_status)) {
					syslog(LOG_INFO, "Zadanie zakonczone z kodem: %d", WEXITSTATUS(last_status));
				}
				_exit(EXIT_SUCCESS);

			} else {
				syslog(LOG_INFO, "Zadanie uruchomione (PID: %d)", pid);
				waitpid(pid, NULL, WNOHANG);
				Task* help = curr;
				curr = curr->next;
				free_task(help);
			}
		}
	}

	if (curr) free_all(curr);
	closelog();
	return 0;
}
