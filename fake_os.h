#include "fake_process.h"
#include "linked_list.h"
#pragma once

#define CORE_NUMBER 4

typedef struct
{
	ListItem list;
	int pid;
	double previousPrediction; // used by SJF to predict the duration of the next event
	int duration;
	ListHead events;
} FakePCB;

struct FakeOS;
typedef void (*ScheduleFn)(struct FakeOS *os, void *args);

typedef struct FakeOS
{
	FakePCB **running;
	ListHead ready;
	ListHead waiting;
	int timer;
	int cores;
	ScheduleFn schedule_fn;
	void *schedule_args;
	ListHead processes;
} FakeOS;

void FakeOS_init(FakeOS *os, int cores);
void FakeOS_simStep(FakeOS *os);
void FakeOS_destroy(FakeOS *os);