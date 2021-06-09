/*
 * Group Weighted Round Robin Scheduling Class
 */

#include "sched.h"
#include <linux/slab.h>

#define for_each_sched_wrr_entity(wrr_se) \
    for (; wrr_se; wrr_se = wrr_se->parent)

#define wrr_entity_is_task(wrr_se) (!(wrr_se)->my_q)

// Extern functions
extern void init_wrr_rq(struct wrr_rq *wrr_rq, struct rq *rq);
extern void init_tg_wrr_entry(struct task_group *tg, struct wrr_rq *wrr_rq,
                struct sched_wrr_entity *wrr_se, int cpu,
                struct sched_wrr_entity *parent);

// To determine whether the process is in foreground or background
static char group_path[10000];
static char *task_group_path(struct task_group *tg)
{
        if (autogroup_path(tg, group_path, 10000))
                return group_path;

        if (!tg->css.cgroup) {
                group_path[0] = '\0';
                return group_path;
        }
        cgroup_path(tg->css.cgroup, group_path, 10000);
        return group_path;
}

static inline int on_wrr_rq(struct sched_wrr_entity *wrr_se)
{
    return !list_empty(&wrr_se->run_list);
}

static inline struct task_struct *wrr_task_of(struct sched_wrr_entity *wrr_se)
{
#ifdef CONFIG_SCHED_DEBUG
    WARN_ON_ONCE(!wrr_entity_is_task(wrr_se));
#endif
    return container_of(wrr_se, struct task_struct, wrr);
}

static inline struct rq *rq_of_wrr_rq(struct wrr_rq *wrr_rq)
{
    return wrr_rq->rq;
}

static inline struct wrr_rq *wrr_rq_of_se(struct sched_wrr_entity *wrr_se)
{
    struct task_struct *p = wrr_task_of(wrr_se);
    struct rq *rq = task_rq(rq);

    return &rq->wrr;
}

static inline struct wrr_rq *group_wrr_rq(struct sched_wrr_entity *wrr_se)
{
    return wrr_se->my_q;
}

static inline int wrr_se_prio(struct sched_wrr_entity *wrr_se)
{
    return wrr_task_of(wrr_se)->rt_priority;
}

static inline void list_add_leaf_wrr_rq(struct wrr_rq *wrr_rq)
{
    list_add_rcu(&wrr_rq->leaf_wrr_rq_list,
                &rq_of_wrr_rq(wrr_rq)->leaf_wrr_rq_list);
}

static inline void list_del_leaf_wrr_rq(struct wrr_rq *wrr_rq){
    list_del_rcu(&wrr_rq->leaf_wrr_rq_list);
}

static void inc_wrr_prio(struct wrr_rq *wrr_rq, int prio)
{
    int prev_prio = wrr_rq->highest_prio.curr;

    if (prio < prev_prio)
        wrr_rq->highest_prio.curr = prio;
}

static void dec_wrr_prio(struct wrr_rq *wrr_rq, int prio)
{
    int prev_prio = wrr_rq->highest_prio.curr;

    if (wrr_rq->wrr_nr_running)
    {
        WARN_ON(prio < prev_prio);

        if (prio == prev_prio)
        {
            struct wrr_prio_array *array = &wrr_rq->active;

            wrr_rq->highest_prio.curr =
                sched_find_first_bit(array->bitmap);
        }
    }
    else
        wrr_rq->highest_prio.curr = MAX_WRR_PRIO;
}

static inline void inc_wrr_tasks(struct sched_wrr_entity *wrr_se, struct wrr_rq *wrr_rq)
{
    int prio = wrr_se_prio(wrr_se);

    wrr_rq->wrr_nr_running++;

    inc_wrr_prio(wrr_rq, prio);
}

static inline void dec_wrr_tasks(struct sched_wrr_entity *wrr_se, struct wrr_rq *wrr_rq)
{
    WARN_ON(!wrr_rq->wrr_nr_running);
    wrr_rq->wrr_nr_running--;

    dec_wrr_prio(wrr_rq, wrr_se_prio(wrr_se));
}

/*
 * Update the current task's runtime statistics. Skip current tasks that
 * are not in our scheduling class.
 */
static void update_curr_wrr(struct rq *rq)
{
    struct task_struct *curr = rq->curr;
    u64 delta_exec;

    if(curr->sched_class != &wrr_sched_class)
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

static void __dequeue_wrr_entity(struct sched_wrr_entity *wrr_se)
{
    struct wrr_rq *wrr_rq = wrr_rq_of_se(wrr_se);
    struct wrr_prio_array *array = &wrr_rq->active;

    list_del_init(&wrr_se->run_list);
    if (list_empty(array->queue + wrr_se_prio(wrr_se)))
        __clear_bit(wrr_se_prio(wrr_se), array->bitmap);

    dec_wrr_tasks(wrr_se, wrr_rq);
    if (!wrr_rq->wrr_nr_running)
        list_del_leaf_wrr_rq(wrr_rq);
}

static void __enqueue_wrr_entity(struct sched_wrr_entity *wrr_se, bool head)
{
    printk("Enqueue a new wrr entity!\n");
    struct wrr_rq *wrr_rq = wrr_rq_of_se(wrr_se);
    struct wrr_prio_array *array = &wrr_rq->active;
    struct wrr_rq *group_rq = group_wrr_rq(wrr_se);
    struct list_head *queue = array->queue + wrr_se_prio(wrr_se);

    if (group_rq && !group_rq->wrr_nr_running)
        return;

    if (!wrr_rq->wrr_nr_running)
        list_add_leaf_wrr_rq(wrr_rq);

    if (head)
        list_add(&wrr_se->run_list, queue);
    else
        list_add_tail(&wrr_se->run_list, queue);
    
    __set_bit(wrr_se_prio(wrr_se), array->bitmap);

    inc_wrr_tasks(wrr_se, wrr_rq);
}

/*
 * Because the prio of an upper entry depends on the lower
 * entries, we must remove entries top-down
 */
static void dequeue_wrr_stack(struct sched_wrr_entity *wrr_se)
{
    struct sched_wrr_entity *back = NULL;

    for_each_sched_wrr_entity(wrr_se)
    {
        wrr_se->back = back;
        back = wrr_se;
    }

    for (wrr_se = back; wrr_se; wrr_se = wrr_se->back)
    {
        if (on_wrr_rq(wrr_se))
            __dequeue_wrr_entity(wrr_se);
    }
}

static void dequeue_wrr_entity(struct sched_wrr_entity *wrr_se)
{
    dequeue_wrr_stack(wrr_se);

    for_each_sched_wrr_entity(wrr_se)
    {
        struct wrr_rq *wrr_rq = group_wrr_rq(wrr_se);

        if (wrr_rq && wrr_rq->wrr_nr_running)
            __enqueue_wrr_entity(wrr_se, false);
    }
}

static void dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
    printk("Dequeue a wrr task!\n");
    struct sched_wrr_entity *wrr_se = &p->wrr;

    update_curr_wrr(rq);
    dequeue_wrr_entity(wrr_se);

    dec_nr_running(rq);
}

static void enqueue_wrr_entity(struct sched_wrr_entity *wrr_se, bool head)
{
    dequeue_wrr_stack(wrr_se);
    for_each_sched_wrr_entity(wrr_se)
        __enqueue_wrr_entity(wrr_se, head);
}

/*
 * Adding/removing a task to/from a priority array;
 */
static void enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
    printk("Enqueue a new wrr task!\n");
    struct sched_wrr_entity *wrr_se = &p->wrr;
    // printk("%d", p->rt_priority);
    enqueue_wrr_entity(wrr_se, flags & ENQUEUE_HEAD);
    inc_nr_running(rq);
}

static void requeue_wrr_entity(struct wrr_rq *wrr_rq, struct sched_wrr_entity *wrr_se, int head)
{
    if (on_wrr_rq(wrr_se))
    {
        struct wrr_prio_array *array = &wrr_rq->active;
        struct list_head *queue = array->queue + wrr_se_prio(wrr_se);

        if (head)
            list_move(&wrr_se->run_list, queue);
        else
            list_move_tail(&wrr_se->run_list, queue);
    }
}

/*
 * Put task to the head or the end of the run list without the overhead of
 * dequeue followed by enqueue.
 */
static void requeue_task_wrr(struct rq *rq, struct task_struct *p, int head)
{
    printk("Requeue the tasks!\n");
    struct sched_wrr_entity *wrr_se = &p->wrr;
    struct wrr_rq *wrr_rq;

    for_each_sched_wrr_entity(wrr_se)
    {
        wrr_rq = wrr_rq_of_se(wrr_se);
        requeue_wrr_entity(wrr_rq, wrr_se, head);
    }
}

static void yield_task_wrr(struct rq *rq)
{
    requeue_task_wrr(rq, rq->curr, 0);
}

/*
 * Preempt the current task with a newly woken task if needed:
 */
static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags)
{
    printk("Check wrr task preemption!\n");
    if (p->rt_priority > rq->curr->rt_priority)
    {
        resched_task(rq->curr);
        return;
    }
}

static struct sched_wrr_entity *pick_next_wrr_entity(struct rq *rq, struct wrr_rq *wrr_rq)
{
    struct wrr_prio_array *array = &wrr_rq->active;
    struct sched_wrr_entity *next = NULL;
    struct list_head *queue;
    int idx;

    idx = sched_find_first_bit(array->bitmap);
    BUG_ON(idx >= MAX_WRR_PRIO);

    queue = array->queue + idx;
    next = list_entry(queue->next, struct sched_wrr_entity, run_list);

    return next;
}

static struct task_struct *_pick_next_task_wrr(struct rq *rq)
{
    struct sched_wrr_entity *wrr_se;
    struct task_struct *p;
    struct wrr_rq *wrr_rq;

    wrr_rq = &rq->wrr;

    if (!wrr_rq->wrr_nr_running)
        return NULL;

    do
    {
        wrr_se = pick_next_wrr_entity(rq, wrr_rq);
        BUG_ON(!wrr_se);
        wrr_rq = group_wrr_rq(wrr_se);
    } while (wrr_rq);

    p = wrr_task_of(wrr_se);
    p->se.exec_start = rq->clock_task;

    return p;
}

static struct task_struct *pick_next_task_wrr(struct rq *rq)
{
    // printk("Select next wrr task!\n");
    struct task_struct *p = _pick_next_task_wrr(rq);
    return p;
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *p)
{
    printk("Put previous wrr task!\n");
    update_curr_wrr(rq);
}

static void set_curr_task_wrr(struct rq *rq)
{
    printk("Set current wrr task!\n");
    struct task_struct *p = rq->curr;
    p->se.exec_start = rq->clock_task;
}

static void task_tick_wrr(struct rq *rq, struct task_struct *p, int queued)
{
    printk("Time Slice left: [%d]\n",p->wrr.time_slice);

    struct sched_wrr_entity *wrr_se = &p->wrr;

    update_curr_wrr(rq);

    if (p->policy != SCHED_WRR)
        return;

    if (--p->wrr.time_slice)
        return;

    if (task_group_path(p->sched_task_group)[1] != 'b') // Foreground
    {
        p->wrr.time_slice = WRR_FORE_TIMESLICE;
    }
    else // Background
    {
        p->wrr.time_slice = WRR_BACK_TIMESLICE;
    }

    // Requeue to the end of queue if we are not the only element on the queue
    for_each_sched_wrr_entity(wrr_se)
    {
        if (wrr_se->run_list.prev != wrr_se->run_list.next)
        {
            requeue_task_wrr(rq, p, 0);
            set_tsk_need_resched(p);
            return;
        }
    }
}

static unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *task)
{
    printk("Get wrr interval!");

    if (task == NULL)
        return -EINVAL;

    if (task_group_path(task->sched_task_group)[1] != 'b')
        return WRR_FORE_TIMESLICE;
    else
        return WRR_BACK_TIMESLICE;
}

static void task_fork_wrr(struct task_struct *p)
{
    printk("Fork a new wrr task!\n");
    p->wrr.time_slice = p->wrr.parent->time_slice;
}

static void switched_to_wrr(struct rq *rq, struct task_struct *p)
{
    if (p->on_rq && rq->curr != p)
    {
        if (p->rt_priority > rq->curr->rt_priority)
            resched_task(rq->curr);
    }
}

void free_wrr_sched_group(struct task_group *tg)
{
    printk("Switch to wrr task!\n");
    // printk("%d", p->rt_priority);
    kfree(tg->wrr_rq);
    kfree(tg->wrr_se);
}

int alloc_wrr_sched_group(struct task_group *tg, struct task_group *parent)
{
    struct wrr_rq *wrr_rq;
    struct sched_wrr_entity *wrr_se;

    tg->wrr_rq = kzalloc(sizeof(wrr_rq) * nr_cpu_ids, GFP_KERNEL);
    if (!tg->wrr_rq)
        goto err;
    tg->wrr_se = kzalloc(sizeof(wrr_se) * nr_cpu_ids, GFP_KERNEL);
    if (!tg->wrr_se)
        goto err;
    
    wrr_rq = kzalloc_node(sizeof(struct wrr_rq), GFP_KERNEL, cpu_to_node(0));
    if(!wrr_rq) goto err;
    wrr_se = kzalloc_node(sizeof(struct sched_wrr_entity), GFP_KERNEL, cpu_to_node(0));
    if(!wrr_se) goto err_free_rq;

    init_wrr_rq(wrr_rq, cpu_rq(0));
    init_tg_wrr_entry(tg, wrr_rq, wrr_se, 0, parent-> wrr_se[0]);

    return 1;

err_free_rq:
    kfree(wrr_rq);
err:
    return 0;
}

// Dummy functions
static int select_task_rq_wrr(struct task_struct *p, int sd_flag, int flags) {}

static void set_cpus_allowed_wrr(struct task_struct *p, const struct cpumask *new_mask) {}

static void rq_offline_wrr(struct rq *rq) {}

static void rq_online_wrr(struct rq *rq) {}

static void pre_schedule_wrr(struct rq *rq, struct task_struct *prev) {}

static void post_schedule_wrr(struct rq *rq){}

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
