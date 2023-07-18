#pragma once
#include "linked_list.h"

#define PREDICTION_WEIGHT 0.125 // 1/8 

typedef enum
{
	CPU = 0,
	IO = 1
} ResourceType;

// event of a process, is in a list
typedef struct
{
	ListItem list;
	ResourceType type;
	int duration;
	double previousPrediction; // used by SJF to predict the duration of the next event
} ProcessEvent;

// fake process
typedef struct
{
	ListItem list;
	int pid; // assigned by us
	int arrival_time;
	ListHead events;
} FakeProcess;

int FakeProcess_load(FakeProcess *p, const char *filename);

int FakeProcess_save(const FakeProcess *p, const char *filename);
