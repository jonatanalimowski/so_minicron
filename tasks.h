#ifndef TASKS_H
#define TASKS_H

#include <time.h>

typedef struct Task {
	int hour;
	int minutes;
	char *command;
	int info;
	struct Task *next;
} Task;

#endif
