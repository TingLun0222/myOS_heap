#include "os.h"

/*!< Table use to save TCB pointer.              */
taskCB_t    TCBTbl[MAX_USER_TASKS+SYS_TASK_NUM];

/*!< The stack of IDLE task.                     */
uint32_t   idle_stk[SYS_STACK_SIZE] = {0};

taskCB_t *    TCBRunning         = NULL;        /*!< Pointer to TCB that current running task.  */
taskCB_t *    FreeTCB            = NULL;         /*!< pointer to free TCB                        */
taskCB_t      TCBRdy[PRIO_LEVEL];        /*!< READY list.                 */

taskCB_t * getCurrentTask() {
    return TCBRunning;
}

/**
 *******************************************************************************
 * @brief      Create a free TCB list.	  
 *******************************************************************************
 */
void InitTCBList(void)
{	
    uint16_t  i;
    taskCB_t * ptcb1;
    taskCB_t * ptcb0;
    
    FreeTCB = &TCBTbl[0];	/* Build the free TCB list            */
    FreeTCB->node.prev = NULL;
    FreeTCB->node.next = NULL;

    FreeTCB->state = TASK_INIT;
    FreeTCB->taskID = 0;
    ptcb1 = &TCBTbl[1];
    for(i=1;i< MAX_USER_TASKS+SYS_TASK_NUM;i++ )
    {
        ptcb1->taskID    = i;
        ptcb1->state     = TASK_INIT;
        ptcb1->node.next = NULL;
        ptcb1->node.prev = NULL;        
        ptcb0 = ptcb1-1;
        ptcb0->node.next = (list_t *)ptcb1;
        ptcb1++;
    }	
}

void readyQ_init() {
    for (int i=0;i<PRIO_LEVEL;i++)
        list_init((list_t *)&TCBRdy[i]);
}

static taskCB_t * _getFreeTCB(void)
{
    taskCB_t * ptcb;

    spin_lock();
    if(FreeTCB == NULL)                 /* Is there no free TCB               */
    {
        spin_unlock();                  /* Yes,unlock schedule                */
        return NULL;                    /* Error return                       */
    }	
    ptcb    = FreeTCB;          /* Yes,assgin free TCB for this task  */    
    /* Set next item as the head of free TCB list                     */
    FreeTCB = (taskCB_t *)ptcb->node.next; 
    ptcb->node.next = NULL;
    ptcb->node.prev = NULL;
    spin_unlock();
    return ptcb;        
}

taskCB_t * getNewTCB(uint8_t index) {
    return &TCBTbl[index];
}

err_t task_init(taskCB_t *ptcb, const char *name,
                  void (*taskFunc)(void *parameter),
                  void       *parameter,
                  uint32_t    stack_size,
                  uint16_t    priority,
                  uint32_t    ticks)
{

    void *stack_start;
    stack_start = (void *)malloc(stack_size);
    if (stack_start == NULL)
    {
        /* allocate stack failure */
        return ERROR;
    }

    memcpy(ptcb->name, name, sizeof(ptcb->name));
    ptcb->entry = (void *)taskFunc;
    ptcb->parameter = parameter;

    /* init thread stack */
    memset(ptcb->stack_addr, 0, ptcb->stack_size);
    
    ptcb->ctx.ra = (reg_t)taskFunc;
    ptcb->ctx.sp = (reg_t)(stack_start + stack_size);
    ptcb->ctx.pc = (reg_t)taskFunc;

    ptcb->priority    = priority;
    ptcb->init_ticks = ticks;
    ptcb->remain_ticks = ticks;

    list_init((list_t*)ptcb);
    return OK;
}	

taskCB_t * task_create(const char *name,
                  void (*taskFunc)(void *parameter),
                  void       *parameter,
                  uint32_t    stack_size,
                  uint16_t    priority,
                  uint32_t    ticks)
{
    taskCB_t *ptcb = _getFreeTCB();
    if (ptcb == NULL) { return NULL; }
    err_t ret = task_init(ptcb, name, taskFunc, parameter, stack_size, priority, ticks);
    if (ret == OK)
        return ptcb;
    return NULL;
}	

void task_startup(taskCB_t * ptcb)
{

    /* change thread stat */
    ptcb->state = TASK_SUSPEND;
    /* then resume it */
    task_resume(ptcb);
}


err_t task_resume(taskCB_t *ptcb)
{
    if (ptcb->state != TASK_SUSPEND)
    {
        return ERROR;
    }

    //timer_stop(&ptcb->timer);
    spin_lock(); 
    /* remove from suspend list */
    list_remove((list_t*)ptcb);
    list_insert_before((list_t*)&TCBRdy[ptcb->priority], (list_t*)ptcb); 
    spin_unlock();

    return OK;

}

err_t task_suspend(taskCB_t * ptcb)
{
    if (ptcb->state != TASK_READY)
    {
        return ERROR;
    }

    spin_lock();
    /* change thread stat */
    list_remove((list_t*)ptcb);
    ptcb->state = TASK_SUSPEND;

    spin_unlock();
    return OK;
}



