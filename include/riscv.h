#ifndef __RISCV_H__
#define __RISCV_H__

#include "types.h"


/* Machine Status Register, mstatus */
#define MSTATUS_MIE (1 << 3)
#define MSTATUS_SIE (1 << 1)
#define MSTATUS_UIE (1 << 0)

static inline reg_t r_mstatus()
{
	reg_t x;
	asm volatile("csrr %0, mstatus" : "=r" (x) );
	return x;
}

static inline void w_mstatus(reg_t x)
{
	asm volatile("csrw mstatus, %0" : : "r" (x));
}

static inline void w_mscratch(reg_t x)
{
	asm volatile("csrw mscratch, %0" : : "r" (x));
}

static inline reg_t r_mscratch()
{
	reg_t x;
	asm volatile("csrr %0, mscratch" : "=r" (x) );
	return x;
}

/* Machine-mode interrupt vector */
static inline void w_mtvec(reg_t x)
{
	asm volatile("csrw mtvec, %0" : : "r" (x));
}



/* Machine-mode Interrupt Enable */
#define MIE_MEIE (1 << 11) // external
#define MIE_MTIE (1 << 7)  // timer
#define MIE_MSIE (1 << 3)  // software

static inline reg_t r_mhartid()
{
	reg_t x;
	asm volatile("csrr %0, mhartid" : "=r" (x) );
	return x;
}

static inline reg_t r_mie()
{
	reg_t x;
	asm volatile("csrr %0, mie" : "=r" (x) );
	return x;
}

static inline void w_mie(reg_t x)
{
	asm volatile("csrw mie, %0" : : "r" (x));
}
#endif /* __RISCV_H__ */
