#include "job.h"

#include <unistd.h>

#include "log.h"
#include "asserts.h"
#include "memory/memutils.h"

LOCAL Job_System *job_system = NULL;

LOCAL void *job_system_worker_function(void *arg)
{
    UNUSED(arg);
    LOG_INFO("spawning worker thread TID=%d\n", gettid());

    while (true) {
        pthread_mutex_lock(&job_system->job_queue_lock);

        while (ring_queue_is_empty(&job_system->job_queue) && job_system->running) {
            pthread_cond_wait(&job_system->job_queue_notify, &job_system->job_queue_lock);
        }

        if (!job_system->running) {
            LOG_INFO("shutting down worker thread TID=%d\n", gettid());
            pthread_mutex_unlock(&job_system->job_queue_lock);
            pthread_exit(NULL);
        }

        Job job;
        if (!ring_queue_dequeue(&job_system->job_queue, &job)) {
            LOG_ERROR("failed to dequeue a job\n");
            pthread_mutex_unlock(&job_system->job_queue_lock);
            continue;
        }

        pthread_mutex_unlock(&job_system->job_queue_lock);

        bool result = job.entry_point(job.param_data, job.result_data);
        mem_free(job.param_data, job.param_data_size, MEMORY_TAG_JOB);

        pthread_mutex_lock(&job_system->results_lock);
        for (u32 i = 0; i < JOB_SYSTEM_MAX_NUM_RESULTS; i++) {
            if (job_system->results[i].empty) {
                job_system->results[i].empty = false;
                job_system->results[i].status = result ? JOB_STATUS_COMPLETED : JOB_STATUS_FAILED;
                job_system->results[i].callback = job.callback;
                job_system->results[i].result_data = job.result_data;
                job_system->results[i].result_data_size = job.result_data_size;
                break;
            }
        }
        pthread_mutex_unlock(&job_system->results_lock);
    }
}

bool job_system_init(Job_System *js, u8 num_workers)
{
    job_system = js;
    if (job_system->running) {
        return true;
    }

    if (num_workers > JOB_SYSTEM_MAX_NUM_WORKERS) {
        LOG_ERROR("cannot initialize job system with more than %d workers\n", JOB_SYSTEM_MAX_NUM_WORKERS);
        return false;
    }

    job_system->workers_count = num_workers;
    ring_queue_reserve_tagged(&job_system->job_queue, JOB_SYSTEM_MAX_NUM_RESULTS, sizeof(Job), MEMORY_TAG_JOB);
    pthread_mutex_init(&job_system->job_queue_lock, NULL);
    pthread_cond_init(&job_system->job_queue_notify, NULL);
    pthread_mutex_init(&job_system->results_lock, NULL);
    job_system->running = true;

    LOG_INFO("job system initialized with %hhu workers\n", job_system->workers_count);

    for (u32 i = 0; i < JOB_SYSTEM_MAX_NUM_RESULTS; i++) {
        job_system->results[i].empty = true;
    }

    for (u32 i = 0; i < num_workers; i++) {
        pthread_create(&job_system->workers[i].thread, NULL, job_system_worker_function, job_system);
    }

    return true;
}

void job_system_shutdown(void)
{
    ASSERT(job_system != NULL);

    job_system->running = false;

    pthread_cond_broadcast(&job_system->job_queue_notify);

    for (u32 i = 0; i < job_system->workers_count; i++) {
        pthread_join(job_system->workers[i].thread, NULL);
    }

    pthread_mutex_destroy(&job_system->job_queue_lock);
    pthread_cond_destroy(&job_system->job_queue_notify);
    pthread_mutex_destroy(&job_system->results_lock);
    ring_queue_destroy(&job_system->job_queue);

    LOG_INFO("job system shutdown complete\n");
}

void job_system_update(void)
{
    ASSERT(job_system != NULL);

    pthread_mutex_lock(&job_system->results_lock);
    for (u32 i = 0; i < JOB_SYSTEM_MAX_NUM_RESULTS; i++) {
        if (!job_system->results[i].empty) {
            Job_Result *result = &job_system->results[i];
            result->callback(result->status, result->result_data);
            mem_free(result->result_data, result->result_data_size, MEMORY_TAG_JOB);
            result->empty = true;
        }
    }
    pthread_mutex_unlock(&job_system->results_lock);
}

void job_system_submit(pfn_job_entry_point entry_point, void *param_data, u64 param_data_size, pfn_job_callback callback, u64 result_data_size)
{
    ASSERT(job_system != NULL);

    void *param_data_copy = mem_alloc(param_data_size, MEMORY_TAG_JOB);
    mem_copy(param_data_copy, param_data, param_data_size);

    Job job = {
        .entry_point = entry_point,
        .param_data = param_data_copy,
        .param_data_size = param_data_size,
        .callback = callback,
        .result_data = mem_alloc(result_data_size, MEMORY_TAG_JOB),
        .result_data_size = result_data_size
    };

    pthread_mutex_lock(&job_system->job_queue_lock);

    if (!ring_queue_enqueue(&job_system->job_queue, &job)) {
        LOG_ERROR("failed to enqueue a job\n");
        pthread_mutex_unlock(&job_system->job_queue_lock);
        return;

    }

    pthread_mutex_unlock(&job_system->job_queue_lock);
    pthread_cond_signal(&job_system->job_queue_notify);
}
