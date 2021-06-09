/*
 * Random MLFQ Based Weighted Round Robin Scheduling Class
 */

#include "sched.h"
#include <linux/slab.h>
#include <linux/random.h>

#define UNSIGNED_MAX 4294967295

static unsigned int getRand() {
    unsigned int randNum;
    get_random_bytes(&randNum,sizeof(unsigned int));
    return randNum;
}

// To determine whether the process is in foreground or background
static char group_path[10000];
static char *task_group_path(struct task_group *tg)
{
    if (autogroup_path(tg, group_path, 10000))
        return group_path;

    if (!tg->css.cgroup)
    {
        group_path[0] = '\0';
        return group_path;
    }
    cgroup_path(tg->css.cgroup, group_path, 10000);
    return group_path;
}

static inline struct task_struct *wrr_task_of(struct sched_wrr_entity *wrr_se)
{
    return container_of(wrr_se, struct task_struct, wrr);
}

static inline struct rq *rq_of_wrr_rq(struct wrr_rq *wrr_rq)
{
    return wrr_rq->rq;
}

static inline struct wrr_rq *wrr_rq_of_se(struct sched_wrr_entity *wrr_se)
{
    struct task_struct *p = wrr_task_of(wrr_se);
    struct rq *rq = task_rq(p);

    return &rq->wrr;
}

static inline int wrr_se_prio(struct sched_wrr_entity *wrr_se)
{
    return 99 - wrr_task_of(wrr_se)->rt_priority;
}

static inline int wrr_task_prio(struct task_struct *p)
{
    return 99 - p->rt_priority;
}

/*
 * Update the current task's runtime statistics. Skip current tasks that
 * are not in our scheduling class.
 */
static void update_curr_wrr(struct rq *rq)
{
    // printk("Update current task statistics!\n");
    struct task_struct *curr = rq->curr;
    u64 delta_exec;

    if (curr->sched_class != &wrr_sched_class)
        return;

    delta_exec = rq->clock_task - curr->se.exec_start;
    if (unlikely((s64)delta_exec < 0))
        delta_exec = 0;

    schedstat_set(curr->se.statistics.exec_max,
                max(curr->se.statistics.exec_max, delta_exec));

    curr->se.sum_exec_runtime += delta_exec;
    account_group_exec_runtime(curr, delta_exec);

    curr->se.exec_start = rq->clock_task;
    cpuacct_charge(curr, delta_exec);
}

static inline void list_del_leaf_wrr_rq(struct wrr_rq *wrr_rq)
{
    list_del_rcu(&wrr_rq->leaf_wrr_rq_list);
}

static void dequeue_wrr_entity(struct sched_wrr_entity *wrr_se)
{
    struct wrr_rq *wrr_rq = wrr_rq_of_se(wrr_se);
    struct wrr_prio_array *array = &wrr_rq->active;

    list_del_init(&wrr_se->run_list);
    if (list_empty(array->queue + wrr_se_prio(wrr_se)))
        __clear_bit(wrr_se_prio(wrr_se), array->bitmap);

    WARN_ON(!wrr_rq->wrr_nr_running);
    wrr_rq->wrr_nr_running--;

    if (!wrr_rq->wrr_nr_running)
        list_del_leaf_wrr_rq(wrr_rq);
}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
    printk("Dequeue a wrr task!\n");

    struct sched_wrr_entity *wrr_se = &p->wrr;

    update_curr_wrr(rq);
    dequeue_wrr_entity(wrr_se);

    dec_nr_running(rq);
}

static inline void list_add_leaf_wrr_rq(struct wrr_rq *wrr_rq)
{
    list_add_rcu(&wrr_rq->leaf_wrr_rq_list,
                &rq_of_wrr_rq(wrr_rq)->leaf_wrr_rq_list);
}

static void enqueue_wrr_entity(struct sched_wrr_entity *wrr_se, bool head)
{
    struct wrr_rq *wrr_rq = wrr_rq_of_se(wrr_se);
    struct wrr_prio_array *array = &wrr_rq->active;
    struct list_head *queue = array->queue + wrr_se_prio(wrr_se);

    if (!wrr_rq->wrr_nr_running)
        list_add_leaf_wrr_rq(wrr_rq);

    if (head)
        list_add(&wrr_se->run_list, queue);
    else
        list_add_tail(&wrr_se->run_list, queue);

    __set_bit(wrr_se_prio(wrr_se), array->bitmap);

    wrr_rq->wrr_nr_running++;
}

/*
 * Adding/removing a task to/from a priority array;
 */
static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
    printk("Enqueue a new wrr task!\n");
    struct sched_wrr_entity *wrr_se = &p->wrr;

    enqueue_wrr_entity(wrr_se, flags & ENQUEUE_HEAD);

    inc_nr_running(rq);
}

/*
 * Put task to the head or the end of the run list without the overhead of
 * dequeue followed by enqueue.
 */
static void requeue_task_wrr(struct rq *rq, struct task_struct *p, int head)
{
    printk("Requeue the tasks!\n");

    // get random number
    unsigned int r = getRand();
    // printk("Get a random number! [%u]\n",r);

    // get current priority
    int prio = wrr_task_prio(p);

    struct sched_wrr_entity *wrr_se = &p->wrr;
    struct wrr_rq *wrr_rq = &rq->wrr;

    struct wrr_prio_array *array = &wrr_rq->active;
    struct list_head *queue = array->queue + prio;

    // set high and low stage
    struct list_head *high_queue = (prio < 90) ? array->queue+prio+10 : queue;
    struct list_head *low_queue = (prio > 9) ? array->queue+prio-10 :queue;

    // p->times refers to the times it stays in the current stage
    if(r < ((p->times+prio/20) * UNSIGNED_MAX /10)){ // go down to the low stage
        p->times = 1; // reset the times
        list_del_init(&wrr_se->run_list);
        if (head)
            list_add(&wrr_se->run_list, low_queue);
        else 
            list_add_tail(&wrr_se->run_list, low_queue);
        
        if(prio > 9){
            // update the bitmap
            if(list_empty(queue))
                __clear_bit(prio, array->bitmap);
            __set_bit(prio-10, array->bitmap);

            // reset priority
            p->rt_priority = p->rt_priority + 10;
            printk("Priority Changed: [%d]\n",wrr_task_prio(p));
        }
    }
    else if(r > ((5-p->times+prio/20) * UNSIGNED_MAX / 10)){ // go up to the high stage
        p->times = 1; // reset the times
        list_del_init(&wrr_se->run_list);
        if (head)
            list_add(&wrr_se->run_list, high_queue);
        else 
            list_add_tail(&wrr_se->run_list, high_queue);

        if(prio < 90){
            // update the bitmap
            if(list_empty(queue))
                __clear_bit(prio, array->bitmap);
            __set_bit(prio+10, array->bitmap);

            // reset priority
            p->rt_priority = p->rt_priority - 10;
            printk("Priority Changed: [%d]\n",wrr_task_prio(p));
        }
    } else { // stay in the current stage
        p->times += 1; // accumulate the times
        if (head)
            list_move(&wrr_se->run_list, queue);
        else
            list_move_tail(&wrr_se->run_list, queue);
    }
}

static void yield_task_wrr(struct rq *rq)
{
    printk("Yield a wrr task!\n");
    requeue_task_wrr(rq, rq->curr, 0);
}

/*
 * Preempt the current task with a newly woken task if needed:
 */
static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags)
{
    printk("Check wrr task preemption!\n");
    if (wrr_task_prio(p) < wrr_task_prio(rq->curr))
    {
        resched_task(rq->curr);
        return;
    }
}

static struct task_struct *pick_next_task_wrr(struct rq *rq)
{
    // printk("Select next wrr task!\n");

    if (unlikely(!rq->wrr.wrr_nr_running))
        return NULL;

    struct wrr_rq *wrr_rq = &rq->wrr;
    struct wrr_prio_array *array = &wrr_rq->active;
    struct sched_wrr_entity *next = NULL;
    struct list_head *queue;
    int idx;

    idx = sched_find_first_bit(array->bitmap);
    printk("Find first bit %d\n",idx);
    BUG_ON(idx >= MAX_WRR_PRIO);

    queue = array->queue + idx;
    next = list_entry(queue->next, struct sched_wrr_entity, run_list);

    struct task_struct *p;
    p = wrr_task_of(next);
    if(p) printk("Get task!\n");

    if (!p)
        return NULL;
    p->se.exec_start = rq->clock_task;

    return p;
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *p)
{
    printk("Put previous wrr task!\n");
    update_curr_wrr(rq);
    // p->se.exec_start = 0;
}

static void set_curr_task_wrr(struct rq *rq)
{
    printk("Set current wrr task!\n");
    struct task_struct *p = rq->curr;

    p->se.exec_start = rq->clock_task;
}

static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued)
{
    // printk("Periodically update the wrr task!\n");
    printk("Time Slice left: [%d]\n",p->wrr.time_slice);
    struct sched_wrr_entity *wrr_se = &p->wrr;

    update_curr_wrr(rq);

    if (p->policy != SCHED_WRR)
        return;

    printk("Current Priority [%d]\n",wrr_task_prio(p));

    if (--p->wrr.time_slice)
        return;

    int stage = wrr_task_prio(p) / 10 + 1; // to get timeslice

    if (task_group_path(p->sched_task_group)[1] != 'b') // Foreground
        p->wrr.time_slice = WRR_FORE_TIMESLICE * stage;
    else // Background
        p->wrr.time_slice = WRR_BACK_TIMESLICE;

    // Requeue the task queue
    set_tsk_need_resched(p);
    requeue_task_wrr(rq, p, 0);
    return;
}

static unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *task)
{
    printk("Get wrr interval!\n");
    if (task == NULL)
        return -EINVAL;

    int stage = (wrr_task_prio(task)) / 10 + 1; // to get timeslice

    if (task_group_path(task->sched_task_group)[1] != 'b')
        return WRR_FORE_TIMESLICE * stage;
    else
        return WRR_BACK_TIMESLICE;
}

static void task_fork_wrr(struct task_struct *p)
{
    // printk("Fork a new wrr task!\n");
    // p->wrr.time_slice = p->wrr.parent->time_slice;
}

static void switched_to_wrr(struct rq *rq, struct task_struct *p)
{
    printk("Switch to wrr task!\n");
    if (p->on_rq && rq->curr != p)
    {
        if (wrr_task_prio(p) < wrr_task_prio(rq->curr))
            resched_task(rq->curr);
    }
}

void free_wrr_sched_group(struct task_group *tg){

}

int alloc_wrr_sched_group(struct task_group *tg, struct task_group *parent)
{
    return 1;
}

// Dummy functions
static int select_task_rq_wrr(struct task_struct *p, int sd_flag, int flags) {}

static void set_cpus_allowed_wrr(struct task_struct *p, const struct cpumask *new_mask) {}

static void rq_offline_wrr(struct rq *rq) {}

static void rq_online_wrr(struct rq *rq) {}

static void pre_schedule_wrr(struct rq *rq, struct task_struct *prev) {}

static void post_schedule_wrr(struct rq *rq) {}

static void task_woken_wrr(struct rq *rq, struct task_struct *p) {}

static void switched_from_wrr(struct rq *rq, struct task_struct *p) {}

static void prio_changed_wrr(struct rq *rq, struct task_struct *p, int oldprio) {}

const struct sched_class wrr_sched_class = {
    .next = &fair_sched_class,                    /*Required*/
    .enqueue_task = enqueue_task_wrr,             /*Required*/
    .dequeue_task = dequeue_task_wrr,             /*Required*/
    .yield_task = yield_task_wrr,                 /*Required*/
    .check_preempt_curr = check_preempt_curr_wrr, /*Required*/

    .pick_next_task = pick_next_task_wrr, /*Required*/
    .put_prev_task = put_prev_task_wrr,   /*Required*/

    .task_fork = task_fork_wrr,
#ifdef CONFIG_SMP
    .select_task_rq = select_task_rq_wrr, /*Never need impl*/
    .set_cpus_allowed = set_cpus_allowed_wrr,
    .rq_online = rq_online_wrr,         /*Never need impl*/
    .rq_offline = rq_offline_wrr,       /*Never need impl*/
    .pre_schedule = pre_schedule_wrr,   /*Never need impl*/
    .post_schedule = post_schedule_wrr, /*Never need impl*/
    .task_woken = task_woken_wrr,       /*Never need impl*/
#endif
    .switched_from = switched_from_wrr, /*Never need impl*/

    .set_curr_task = set_curr_task_wrr, /*Required*/
    .task_tick = task_tick_wrr,         /*Required*/

    .get_rr_interval = get_rr_interval_wrr,

    .prio_changed = prio_changed_wrr, /*Never need impl*/
    .switched_to = switched_to_wrr,   /*Never need impl*/
};
