#include "os.h"

/* defined in entry.S */
extern void switch_to(ctx_t *next);
extern void switch_from_isr(ctx_t *next);
extern taskCB_t TCBRdy[];
extern taskCB_t * TCBRunning; 
extern ctx_t kernel_context;

void sched_init()
{
	w_mscratch(0);
}


void do_schedule(void (*sitchFunc)(ctx_t *ctx))
{
	taskCB_t *nextTask;
	ctx_t *next;
	taskCB_t *readyQ=NULL;

	//get highest priority queue
	for(int i=0;i<PRIO_LEVEL;i++) {
		if (!list_isempty((list_t*)&TCBRdy[i])) {
			readyQ = &TCBRdy[i];
			break;
		}
	}
	if (readyQ==NULL) return;

	//get next task
	nextTask= (taskCB_t*)readyQ->node.next;
	next = &nextTask->ctx;
	list_remove((list_t*)nextTask);

	//current task into ready queue
	if (TCBRunning != NULL){//kernel
		taskCB_t *currentTask = TCBRunning;
		if (currentTask->priority < nextTask->priority)
			return;
		currentTask->state = TASK_READY;
		list_insert_before((list_t*)&TCBRdy[currentTask->priority], (list_t*)currentTask);
	} 
	
	TCBRunning = nextTask;
	nextTask->state = TASK_RUNNING;
	sitchFunc(next);
}

void schedule()
{
	do_schedule(switch_from_isr);
}

err_t task_yield(void)
{
    taskCB_t *ptcb;

    /* set to current task */
    ptcb = TCBRunning;

	if (ptcb==NULL) {//first task
		do_schedule(switch_to);
	    return OK;	
	}
    /* if the task stat is READY and on ready queue list */
    //if (ptcb->state == TASK_READY)
    //{
        /* remove task from task list */
    //    list_remove((list_t*)ptcb);
        /* put task to end of ready queue */
    //    list_insert_before((list_t*)&TCBRdy[ptcb->priority], (list_t*)ptcb);
    //    spin_unlock();
    //    do_schedule(switch_to);
    //    return OK;
    //}
	do_schedule(switch_to);
    return OK;
}



