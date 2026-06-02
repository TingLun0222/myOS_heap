#include "os.h"

static void pass(const char *msg) { kprintf("  PASS: %s\n", msg); }
static void fail(const char *msg) { kprintf("  FAIL: %s\n", msg); }

static volatile int test_phase = 1;

/* ============================================================
 * TEST 1: Timer preemption (phase 1)
 * ============================================================ */
static volatile uint32_t t1_a = 0, t1_b = 0;

static void t1_task_a(void *p)
{
    while (test_phase == 1) t1_a++;
    while (1) task_yield();
}

static void t1_task_b(void *p)
{
    while (test_phase == 1) t1_b++;
    while (1) task_yield();
}

static void t1_monitor(void *p)
{
    while (t1_a < 500 || t1_b < 500) task_yield();

    kprintf("[TEST 1] Timer preemption\n");
    kprintf("  cnt_a=%d  cnt_b=%d\n", t1_a, t1_b);
    if (t1_a > 0 && t1_b > 0)
        pass("both tasks got CPU via timer");
    else
        fail("one task never ran");

    test_phase = 2;
    while (1) task_yield();
}

/* ============================================================
 * TEST 3: task_yield round-robin order (phase 2)
 * ============================================================ */
#define T3_SIZE 9
static volatile int t3_log[T3_SIZE];
static volatile int t3_idx = 0;

static void t3_a(void *p)
{
    while (test_phase < 2) task_yield();
    while (test_phase == 2) { if (t3_idx < T3_SIZE) t3_log[t3_idx++] = 0; task_yield(); }
    while (1) task_yield();
}

static void t3_b(void *p)
{
    while (test_phase < 2) task_yield();
    while (test_phase == 2) { if (t3_idx < T3_SIZE) t3_log[t3_idx++] = 1; task_yield(); }
    while (1) task_yield();
}

static void t3_c(void *p)
{
    while (test_phase < 2) task_yield();
    while (test_phase == 2) { if (t3_idx < T3_SIZE) t3_log[t3_idx++] = 2; task_yield(); }
    while (1) task_yield();
}

static void t3_monitor(void *p)
{
    while (test_phase < 2) task_yield();
    while (t3_idx < T3_SIZE) task_yield();

    kprintf("[TEST 3] task_yield order\n  order:");
    for (int i = 0; i < T3_SIZE; i++) kprintf(" %d", t3_log[i]);
    kprintf("\n");

    int ok = 1;
    for (int i = 0; i < T3_SIZE; i++)
        if (t3_log[i] != i % 3) { ok = 0; break; }
    if (ok) pass("round-robin yield order correct");
    else    fail("yield order wrong");

    test_phase = 3;
    while (1) task_yield();
}

/* ============================================================
 * TEST 4: task_suspend / task_resume (phase 3)
 * ============================================================ */
static volatile uint32_t t4_cnt = 0;
static taskCB_t *t4_victim_tcb = NULL;

static void t4_victim(void *p)
{
    while (test_phase < 3) task_yield();
    while (1) { t4_cnt++; task_yield(); }
}

static void t4_ctrl(void *p)
{
    while (test_phase < 3) task_yield();
    while (t4_cnt < 10) task_yield();

    err_t r = task_suspend(t4_victim_tcb);
    uint32_t before = t4_cnt;

    kprintf("[TEST 4] task_suspend / task_resume\n");
    kprintf("  suspend: %s\n", r == OK ? "OK" : "ERROR");

    for (int i = 0; i < 300; i++) task_yield();

    if (t4_cnt == before)
        pass("task stopped after suspend");
    else
        fail("task still ran after suspend");

    task_resume(t4_victim_tcb);
    for (int i = 0; i < 50; i++) task_yield();

    if (t4_cnt > before)
        pass("task ran again after resume");
    else
        fail("task did not run after resume");

    test_phase = 4;
    while (1) task_yield();
}

/* ============================================================
 * TEST 5: malloc / free (phase 4)
 * ============================================================ */
static void t5_malloc(void *p)
{
    while (test_phase < 4) task_yield();

    kprintf("[TEST 5] malloc / free\n");

    void *a = malloc(256);
    void *b = malloc(256);
    void *c = malloc(256);
    kprintf("  a=%x  b=%x  c=%x\n", a, b, c);

    if (a && b && c && a != b && b != c && a != c)
        pass("three allocations at distinct addresses");
    else
        fail("bad allocation");

    free(b);
    void *d = malloc(256);
    kprintf("  after free(b): d=%x\n", d);
    if (d == b)
        pass("freed page reused");
    else
        fail("freed page not reused");

    free(a); free(c); free(d);

    test_phase = 5;
    while (1) task_yield();
}

/* ============================================================
 * TEST 2: Priority scheduling (phase 5, last)
 *   t2_hi and t2_lo are created dynamically by t2_launcher at phase 5
 *   to prevent hi (priority 5) from monopolizing the CPU during earlier phases.
 *   t2_hi counts and reports the result itself.
 * ============================================================ */
static volatile uint32_t t2_hi_cnt = 0, t2_lo_cnt = 0;

static void t2_lo(void *p)
{
    while (1) { t2_lo_cnt++; task_yield(); }
}

static void t2_hi(void *p)
{
    uint32_t lo_before = t2_lo_cnt;
    while (t2_hi_cnt < 300) { t2_hi_cnt++; task_yield(); }
    uint32_t lo_after = t2_lo_cnt;

    kprintf("[TEST 2] Priority scheduling\n");
    kprintf("  hi=%d  lo ran during hi active: %d\n",
            t2_hi_cnt, lo_after - lo_before);

    if (lo_after == lo_before)
        pass("lo never ran while hi was active");
    else
        fail("lo ran while hi was active");

    kprintf("\n[ALL TESTS DONE]\n");
    while (1) task_yield();
}

static void t2_launcher(void *p)
{
    while (test_phase < 5) task_yield();

    /* Create hi/lo only at phase 5 so they don't interfere with earlier tests */
    task_startup(task_create("t2hi", t2_hi, NULL, 1024,  5, 5000));
    task_startup(task_create("t2lo", t2_lo, NULL, 1024, 20, 5000));

    /* launcher done, spin */
    while (1) task_yield();
}

/* ============================================================
 * loadTasks — all tasks at priority 11; TEST 2 tasks created dynamically
 * ============================================================ */
void loadTasks(void)
{
    /* TEST 1 */
    task_startup(task_create("t1a",  t1_task_a,  NULL, 1024, 11,    5));
    task_startup(task_create("t1b",  t1_task_b,  NULL, 1024, 11,    5));
    task_startup(task_create("t1m",  t1_monitor, NULL, 1024, 11, 5000));

    /* TEST 3 */
    task_startup(task_create("t3a",  t3_a,       NULL, 1024, 11, 5000));
    task_startup(task_create("t3b",  t3_b,       NULL, 1024, 11, 5000));
    task_startup(task_create("t3c",  t3_c,       NULL, 1024, 11, 5000));
    task_startup(task_create("t3m",  t3_monitor, NULL, 1024, 11, 5000));

    /* TEST 4 */
    t4_victim_tcb = task_create("t4v", t4_victim, NULL, 1024, 11, 5000);
    task_startup(t4_victim_tcb);
    task_startup(task_create("t4c",  t4_ctrl,    NULL, 1024, 11, 5000));

    /* TEST 5 */
    task_startup(task_create("t5",   t5_malloc,  NULL, 1024, 11, 5000));

    /* TEST 2 launcher (created last; spawns hi/lo only at phase 5) */
    task_startup(task_create("t2l",  t2_launcher,NULL, 1024, 11, 5000));
}
