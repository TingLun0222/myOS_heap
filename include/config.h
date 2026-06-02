#define MAX_USER_TASKS      256
#define SYS_TASK_NUM        1
#define SYS_STACK_SIZE      256
#define USER_STACK_SIZE     1024
#define PRIO_LEVEL          256
#define TICK_PER_SECOND     100
/* interval ~= 1s */
#define SYSTEM_TICK CLINT_TIMEBASE_FREQ/TICK_PER_SECOND
