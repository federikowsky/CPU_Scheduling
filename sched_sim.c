#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include "fake_os.h"

FakeOS os;

typedef struct {
	int quantum;
} SchedRRArgs;

void print_ready(ListItem *items, int quantum)
{
	FakePCB *proc;
	ProcessEvent *e;
	while ((proc = (FakePCB*) items) != NULL)
	{
		e = (ProcessEvent*) proc->events.first;
		printf("Process %d with event number %d\n", proc->pid, ((e->duration < quantum) ? e->duration : quantum));
		items = items->next;
	}
	printf("\n");
}

FakePCB *prediction(ListItem *items, int quantum)
{
	FakePCB *proc;
	ProcessEvent *e;
	double shortPrediction = __DBL_MAX__;
	FakePCB *shortProcess = NULL;
	// print_ready(items, quantum);
	while ((proc = (FakePCB*) items) != NULL)
	{
		e = (ProcessEvent*) proc->events.first;
		double currPrediction = PREDICTION_WEIGHT * ((e->duration < quantum) ? e->duration : quantum) + \
			(1 - PREDICTION_WEIGHT) * e->previousPrediction;

		if (currPrediction < shortPrediction)
		{
			shortPrediction = currPrediction;
			shortProcess = proc;
			e->previousPrediction = (e->duration < quantum) ? e->duration : quantum;
		}
		items = items->next;
	}

	return shortProcess;
}

void schedRR(FakeOS* os, void* args_){
	SchedRRArgs* args=(SchedRRArgs*)args_;

	// look for the first process in ready
	// if none, return
	if (! os->ready.first)
		return;

	FakePCB *pcb = prediction(os->ready.first, args->quantum);
	
	// FakePCB* pcb=(FakePCB*) List_popFront(&os->ready);
	pcb = (FakePCB*) List_detach(&os->ready, (ListItem *) pcb);
	os->running=pcb;
	
	assert(pcb->events.first);
	ProcessEvent* e = (ProcessEvent*)pcb->events.first;
	assert(e->type==CPU);

	// look at the first event
	// if duration>quantum
	// push front in the list of event a CPU event of duration quantum
	// alter the duration of the old event subtracting quantum
	if (e->duration>args->quantum) {
		ProcessEvent* qe=(ProcessEvent*)malloc(sizeof(ProcessEvent));
		qe->list.prev=qe->list.next=0;
		qe->type=CPU;
		qe->duration=args->quantum;
		e->duration-=args->quantum;
		List_pushFront(&pcb->events, (ListItem*)qe);
	}
};

int main(int argc, char** argv) {
	FakeOS_init(&os);
	SchedRRArgs srr_args;
	srr_args.quantum=5;
	os.schedule_args=&srr_args;
	os.schedule_fn=schedRR;
	
	for (int i=1; i<argc; ++i){
		FakeProcess new_process;
		int num_events=FakeProcess_load(&new_process, argv[i]);
		printf("loading [%s], pid: %d, events:%d\n",
					 argv[i], new_process.pid, num_events);
		if (num_events) {
			FakeProcess* new_process_ptr=(FakeProcess*)malloc(sizeof(FakeProcess));
			*new_process_ptr=new_process;
			List_pushBack(&os.processes, (ListItem*)new_process_ptr);
		}
	}
	printf("num processes in queue %d\n", os.processes.size);
	while(os.running
				|| os.ready.first
				|| os.waiting.first
				|| os.processes.first){
		FakeOS_simStep(&os);
	}
}
