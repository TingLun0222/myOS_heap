#ifndef __TYPES_H__
#define __TYPES_H__

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int  uint32_t;
typedef unsigned long long uint64_t;

/*
 * RISCV32: register is 32bits width
 */ 
typedef uint32_t reg_t;
typedef uint8_t state_t;
typedef char err_t;

#define NULL (void *)0
#define TRUE 1
#define FALSE 0
#define OK 0
#define ERROR -1

/*---------------------------- Error Codes   ---------------------------------*/
#define E_CREATE_FAIL         -1
#define E_OK                  0

typedef struct list {
    struct list *prev;
    struct list *next;
} list_t;


/* lock struct*/
typedef struct spinlock {
	volatile uint32_t locked; 		/* 1 = locked, 0 = unlocked */
} spinlock_t;
#endif /* __TYPES_H__ */