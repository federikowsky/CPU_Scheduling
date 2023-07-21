#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include "fake_os.h"

FakeOS os;

typedef struct
{
	int quantum;
} schedSJFArgs;

void print_ready(ListItem *items, int quantum)
{
	FakePCB *proc;
	ProcessEvent *e;
	while ((proc = (FakePCB *)items) != NULL)
	{
		e = (ProcessEvent *)proc->events.first;
		printf("Process %d with event number %d\n", proc->pid, ((e->duration < quantum) ? e->duration : quantum));
		items = items->next;
	}
	printf("\n");
}

// calculate the prediction of the process with the shortest prediction time and return it
// prediction time is calculated as a weighted average of the previous prediction and the current burst time 
// (or quantum if the burst time is greater than the quantum)
FakePCB *prediction(ListItem *items, int quantum)
{
	FakePCB *proc;
	double shortPrediction = __DBL_MAX__;
	FakePCB *shortProcess = NULL;
	while ((proc = (FakePCB *)items) != NULL)
	{
		double currPrediction = PREDICTION_WEIGHT * ((proc->duration < quantum) ? proc->duration : quantum) +
								(1 - PREDICTION_WEIGHT) * proc->previousPrediction;

		if (currPrediction < shortPrediction)
		{
			shortPrediction = currPrediction;
			shortProcess = proc;
			proc->previousPrediction = currPrediction;
		}
		items = items->next;
	}

	return shortProcess;
}

/**
 * @brief Simulate a step of the fake OS process scheduler SJF 
 * 
 * @param os 
 * @param args_ 
 */
void schedSJF(FakeOS *os, void *args_)
{
	schedSJFArgs *args = (schedSJFArgs *)args_;

	// look for the first process in ready
	// if none, return
	if (!os->ready.first)
		return;

	// look for the process with the shortest prediction time 
	FakePCB *pcb = prediction(os->ready.first, args->quantum);
	if (pcb)
		pcb->duration = 0;

	// remove it from the ready list
	pcb = (FakePCB *)List_detach(&os->ready, (ListItem *)pcb);

	// put it in running list (first empty slot)
	int i = 0;
	FakePCB **running = os->running;
	while (running[i])
		++i;
	running[i] = pcb;

	assert(pcb->events.first);
	ProcessEvent *e = (ProcessEvent *)pcb->events.first;
	assert(e->type == CPU);

	// look at the first event
	// if duration>quantum
	// push front in the list of event a CPU event of duration quantum
	// alter the duration of the old event subtracting quantum
	if (e->duration > args->quantum)
	{
		ProcessEvent *qe = (ProcessEvent *)malloc(sizeof(ProcessEvent));
		qe->list.prev = qe->list.next = 0;
		qe->type = CPU;
		qe->duration = args->quantum;
		e->duration -= args->quantum;
		List_pushFront(&pcb->events, (ListItem *)qe);
	}
};

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("Usage: %s <num_cores> <process1> <process2> ...\n", argv[0]);
		return 1;
	}
	FakeOS_init(&os, atoi(argv[1]));
	schedSJFArgs srr_args;
	srr_args.quantum = 5;
	os.schedule_args = &srr_args;
	os.schedule_fn = schedSJF;

	for (int i = 2; i < argc; ++i)
	{
		FakeProcess new_process;
		int num_events = FakeProcess_load(&new_process, argv[i]);
		printf("loading [%s], pid: %d, events:%d\n",
			   argv[i], new_process.pid, num_events);
		if (num_events)
		{
			FakeProcess *new_process_ptr = (FakeProcess *)malloc(sizeof(FakeProcess));
			*new_process_ptr = new_process;
			List_pushBack(&os.processes, (ListItem *)new_process_ptr);
		}
	}
	printf("num processes in queue %d\n", os.processes.size);
	// run the simulation until all processes are terminated and all queues are empty
	// memcmp returns 0 if the two arrays are equal (in this case, all the pointers are NULL)
	FakePCB **temp = malloc(sizeof(FakePCB *) * os.cores);
	memset(temp, 0, sizeof(FakePCB *) * os.cores);
	while (memcmp(os.running, temp, sizeof(FakePCB *) * os.cores) ||
		   os.ready.first ||
		   os.waiting.first ||
		   os.processes.first)
	{
		FakeOS_simStep(&os);
	}
	free(temp);
	FakeOS_destroy(&os);

}
