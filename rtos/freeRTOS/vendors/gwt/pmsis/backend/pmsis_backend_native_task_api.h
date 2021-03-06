#ifndef __PMSIS_BACKEND_NATIVE_TASK_API_H__
#define __PMSIS_BACKEND_NATIVE_TASK_API_H__

#include "pmsis/pmsis_types.h"
#include "FreeRTOS_util.h"
#include "gap_common.h"
#include "pmsis_gcc.h"
#include "pmsis_backend_native_types.h"

struct pi_task_delayed_s
{
    struct pi_task *fifo_head;
    struct pi_task *fifo_tail;
};



int __os_native_kickoff(void *arg);

static inline void __os_native_exit(int code)
{
    exit(code);
}

static inline int __os_native_api_disable_irq(void)
{
    return __disable_irq();
}

static inline void __os_native_api_restore_irq(int irq_enable)
{
    return __restore_irq(irq_enable);
}

static inline void __os_native_api_sem_take(void *sem_object)
{
    int irq = __disable_irq();
    if (pi_cluster_id() == (uint32_t) ARCHI_FC_CID)
    {
        if (__get_MCAUSE() & MCAUSE_IRQ_Msk)
        {
            /* This case should never happen ! */
            BaseType_t ret;
            xSemaphoreTakeFromISR(sem_object, &ret);
        }
        else
        {
            xSemaphoreTake(sem_object, portMAX_DELAY);
        }
    }
    __restore_irq(irq);
}

static inline void __os_native_api_sem_give(void *sem_object)
{
    int irq = __disable_irq();
    BaseType_t ret;
    xSemaphoreGiveFromISR(sem_object, &ret);
    portYIELD_FROM_ISR(ret);
    __restore_irq(irq);
}

static inline int __os_native_api_sem_init(pi_sem_t *sem)
{
    sem->sem_object = &sem->sem_object_static;
    xSemaphoreCreateCountingStatic(0xFFu, 0, sem->sem_object);
    if(sem->sem_object==NULL)
    {
        printf("OUT of SEMA\n");
        return -1;
    }
    //__os_native_api_sem_give(sem->sem_object);
    sem->take = __os_native_api_sem_take;
    sem->give = __os_native_api_sem_give;
    return 0;
}

static inline int __os_native_api_sem_deinit(pi_sem_t *sem)
{
    if(sem->sem_object == NULL)
    {
        return -1;
    }
    //vSemaphoreDelete(sem->sem_object);
    sem->take = NULL;
    sem->give = NULL;
    sem->sem_object = (void*)NULL;
    return 0;
}


static inline void __os_native_api_mutex_lock(void *mutex_object)
{
    xSemaphoreTake(mutex_object, portMAX_DELAY);
}

static inline void __os_native_api_mutex_release(void *mutex_object)
{
    int irq = __disable_irq();
    BaseType_t ret;
    xSemaphoreGiveFromISR(mutex_object, &ret);
    __restore_irq(irq);
}

static inline int __os_native_api_mutex_init(pmsis_mutex_t *mutex)
{
    mutex->mutex_object = xSemaphoreCreateBinary();
    if(mutex->mutex_object==NULL)
    {
        printf("OUT of MUTEXES\n");
        return -1;
    }
    __os_native_api_mutex_release(mutex->mutex_object);
    mutex->take = __os_native_api_mutex_lock;
    mutex->release = __os_native_api_mutex_release;
    return 0;
}

static inline int __os_native_api_mutex_deinit(pmsis_mutex_t *mutex)
{
    if(mutex->mutex_object == NULL)
    {
        return -1;
    }
    vSemaphoreDelete(mutex->mutex_object);
    mutex->take = NULL;
    mutex->release = NULL;
    mutex->mutex_object = (void*)NULL;
    return 0;
}

static inline void __os_native_yield(void)
{
    taskYIELD();
}

static inline void *__os_native_api_create_task(pi_task_entry_t entry,
        void *arg,
        const char *name,
        uint32_t stack_size,
        int priority)
{
    TaskHandle_t task_handle = NULL;
    BaseType_t task_ret;
    task_ret = xTaskCreate(entry,name,stack_size,arg,
            priority,&task_handle);
    //gap9_secure_ctxt_priv_level_set(task_handle->xMPUSettings,PRIV_LEVEL_MACHINE_MODE);
    if(task_ret != pdPASS)
    {
        return NULL;
    }
    else
    {
        return task_handle;
    }
}

static inline void *__os_native_api_create_user_task(pi_task_entry_t entry,
        void *arg,
        const char *name,
        uint32_t stack_size,
        int priority)
{
    TaskHandle_t task_handle = NULL;
    BaseType_t task_ret;
    pi_user_task_arg_t *user_arg = pi_fc_l1_malloc(sizeof(pi_user_task_arg_t));
    user_arg->entry = entry;
    user_arg->arg = arg;
    task_ret = xTaskCreate(system_usermode_entry,name,stack_size,user_arg,
            priority,&task_handle);
    //gap9_secure_ctxt_priv_level(&task_handle->xMPUSettings,PRIV_LEVEL_USER_MODE);
    if(task_ret != pdPASS)
    {
        return NULL;
    }
    else
    {
        return task_handle;
    }
}

static inline void __os_native_task_suspend(__os_native_task_t *task)
{
    vTaskSuspend( (TaskHandle_t) task );
    vTaskDelete( (TaskHandle_t) task );
}

#endif
