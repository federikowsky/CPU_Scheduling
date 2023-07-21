#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "fake_os.h"

#define ANSI_ORANGE "\x1b[38;5;208m"
#define ANSI_RED "\x1b[31m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_YELLOW "\x1b[33m"
#define ANSI_BLUE "\x1b[34m"
#define ANSI_MAGENTA "\x1b[35m"
#define ANSI_CYAN "\x1b[36m"
#define ANSI_RESET "\x1b[0m"

/**
 * @brief Initialize the fake OS structure
 *
 * @param os
 */
void FakeOS_init(FakeOS *os, int cores)
{
	os->running = calloc(cores, sizeof(FakePCB *));
	List_init(&os->ready);
	List_init(&os->waiting);
	List_init(&os->processes);
	os->timer = 0;
	os->schedule_fn = 0;
	os->cores = cores;
}

void increaseDuration(FakeOS *os)
{
	FakePCB *pcb;
	int i = -1;

	pcb = *os->running;
	while (++i < os->cores && pcb)
	{
		if (pcb->pid)
			pcb->duration++;
		pcb++;
	}
}


/**
 * @brief Create a Process object
 *
 * @param p
 * @param filename
 */
void FakeOS_createProcess(FakeOS *os, FakeProcess *p)
{
	// sanity check
	assert(p->arrival_time == os->timer && "time mismatch in creation");
	// we check that in the list of PCBs there is no
	// pcb having the same pid
	int i = -1;
	while (++i < os->cores)
	{
		FakePCB *running = os->running[i];
		assert((!running || running->pid != p->pid) && "pid taken");
	}

	ListItem *aux = os->ready.first;
	while (aux)
	{
		FakePCB *pcb = (FakePCB *)aux;
		assert(pcb->pid != p->pid && "pid taken");
		aux = aux->next;
	}

	aux = os->waiting.first;
	while (aux)
	{
		FakePCB *pcb = (FakePCB *)aux;
		assert(pcb->pid != p->pid && "pid taken");
		aux = aux->next;
	}

	// all fine, no such pcb exists
	FakePCB *new_pcb = (FakePCB *)malloc(sizeof(FakePCB));
	new_pcb->list.next = new_pcb->list.prev = 0;
	new_pcb->pid = p->pid;
	new_pcb->events = p->events;
	new_pcb->previousPrediction = 0;
	new_pcb->duration = 0;

	assert(new_pcb->events.first && "process without events");

	// depending on the type of the first event
	// we put the process either in ready or in waiting
	ProcessEvent *e = (ProcessEvent *)new_pcb->events.first;
	switch (e->type)
	{
	case CPU:
		List_pushBack(&os->ready, (ListItem *)new_pcb);
		break;
	case IO:
		List_pushBack(&os->waiting, (ListItem *)new_pcb);
		break;
	default:
		assert(0 && "illegal resource");
		;
	}
}

/**
 * @brief Check if all cores are taken by a process
 *
 * @param running
 * @return int
 */
int cpu_full(FakePCB **running, int cores)
{
	int i = -1;
	while (++i < cores)
	{
		if (!running[i] || !(*running[i]).pid)
			return 0;
	}
	return 1;
}

/**
 * @brief Simulate a step of the fake OS
 *
 * @param os
 */
void FakeOS_simStep(FakeOS *os)
{

	printf("************** TIME: %08d **************\n", os->timer);

	// scan process waiting to be started
	// and create all processes starting now
	ListItem *aux = os->processes.first;
	while (aux)
	{
		FakeProcess *proc = (FakeProcess *)aux;
		FakeProcess *new_process = 0;
		if (proc->arrival_time == os->timer)
		{
			new_process = proc;
		}
		aux = aux->next;
		if (new_process)
		{
			printf(ANSI_CYAN "\tcreate pid:%d\n" ANSI_RESET, new_process->pid);
			new_process = (FakeProcess *)List_detach(&os->processes, (ListItem *)new_process);
			FakeOS_createProcess(os, new_process);
			free(new_process);
		}
	}

	// scan waiting list, and put in ready all items whose event terminates
	aux = os->waiting.first;
	while (aux)
	{
		FakePCB *pcb = (FakePCB *)aux;
		aux = aux->next;
		ProcessEvent *e = (ProcessEvent *)pcb->events.first;
		printf(ANSI_MAGENTA "\twaiting pid: %d\n" ANSI_RESET, pcb->pid);
		assert(e->type == IO);
		e->duration--;
		printf(ANSI_MAGENTA "\t\tremaining time:%d\n" ANSI_RESET, e->duration);
		if (e->duration == 0)
		{
			printf(ANSI_MAGENTA "\t\tend burst\n" ANSI_RESET);
			List_popFront(&pcb->events);
			free(e);
			List_detach(&os->waiting, (ListItem *)pcb);
			if (!pcb->events.first)
			{
				// kill process
				printf(ANSI_RED "\t\tend process\n" ANSI_RESET);
				free(pcb);
			}
			else
			{
				// handle next event
				e = (ProcessEvent *)pcb->events.first;
				switch (e->type)
				{
				case CPU:
					printf(ANSI_ORANGE "\t\tmove to ready\n" ANSI_RESET);
					List_pushBack(&os->ready, (ListItem *)pcb);
					break;
				case IO:
					printf(ANSI_YELLOW "\t\tmove to waiting\n" ANSI_RESET);
					List_pushBack(&os->waiting, (ListItem *)pcb);
					break;
				}
			}
		}
	}

	// decrement the duration of running
	// if event over, destroy event
	// and reschedule process
	// if last event, destroy running

	// scan running list, and put in ready all items whose event terminates
	FakePCB **running;
	int i = -1;
	while (++i < os->cores)
	{
		// call schedule, if defined
		running = &os->running[i];

		// if no process is running, skip this core
		if (!(*running))
		{
			printf(ANSI_GREEN "\trunning pid: -1 on core: %d\n" ANSI_RESET, i);
			continue;
		}
		printf(ANSI_GREEN "\trunning pid: %d on core: %d\n" ANSI_RESET, (*running)->pid ? (*running)->pid : -1, i);
		if ((*running)->pid)
		{
			ProcessEvent *e = (ProcessEvent *)(*running)->events.first;
			assert(e->type == CPU);
			e->duration--;
			printf(ANSI_GREEN "\t\tremaining time:%d\n" ANSI_RESET, e->duration);
			if (e->duration == 0)
			{
				printf(ANSI_GREEN "\t\tend burst\n" ANSI_RESET);
				List_popFront(&(*running)->events);
				free(e);
				if (!(*running)->events.first)
				{
					printf(ANSI_RED "\t\tend process\n" ANSI_RESET);
					free(*running); // kill process
					
				}
				else
				{
					e = (ProcessEvent *)(*running)->events.first;
					switch (e->type)
					{
					case CPU:
						printf(ANSI_ORANGE "\t\tmove to ready\n" ANSI_RESET);
						List_pushBack(&os->ready, (ListItem *)*running);
						break;
					case IO:
						printf(ANSI_YELLOW "\t\tmove to waiting\n" ANSI_RESET);
						List_pushBack(&os->waiting, (ListItem *)*running);
						break;
					}
				}
				// set running to 0 to signal that the core is free
				*running = 0;
			}
		}
	}

	i = -1;
	while (++i < os->cores)
	{
		// call schedule, if defined
		if (os->schedule_fn && !cpu_full(os->running, os->cores))
		{
			(*os->schedule_fn)(os, os->schedule_args);
		}

		// if running not defined and ready queue not empty
		// put the first in ready to run
		else if (!cpu_full(os->running, os->cores) && os->ready.first)
		{
			FakePCB **temp = os->running;
			while (*temp)
			{
				if (*temp && !(*temp)->pid)
				{
					*temp = (FakePCB *)List_popFront(&os->ready);
					break;
				}
				(*temp)++;
			}
		}
	}

	increaseDuration(os);
	++os->timer;
}

void FakeOS_destroy(FakeOS *os)
{
	free(os->running);
}
