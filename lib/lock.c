#include "os.h"

int spin_lock()
{
	w_mstatus(r_mstatus() & ~MSTATUS_MIE); /* disable global interrupts */
	return 0;
}

int spin_unlock()
{
	w_mstatus(r_mstatus() | MSTATUS_MIE); /* re-enable global interrupts */
	return 0;
}
