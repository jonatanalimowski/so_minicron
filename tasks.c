#include "tasks.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

Task* create_task(int h, int m, const char *cmd, int info) {
	Task *new_task = malloc(sizeof(Task));
	new_task->hour = h;
	new_task->minutes = m;
	new_task->command = strdup(cmd);
	new_task->info = info;
	new_task->next = NULL;
	return new_task;
}

void free_task(Task *task) {
	free(task->command);
	free(task);
}

void print_task(Task *task) {
	printf("Godzina: %d\nMinuta: %d\nKomenda: %s\nInfo: %d\n", task->hour, task->minutes, task->command, task->info);
}

int main() {
	Task* test_task = create_task(4, 30, "ls -l", 2);
	print_task(test_task);
	free_task(test_task);
	return 0;
}
