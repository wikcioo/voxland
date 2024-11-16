#pragma once

#include <pthread.h>

#include "defines.h"
#include "collections/ring_queue.h"

#define JOB_SYSTEM_MAX_NUM_WORKERS 8
#define JOB_SYSTEM_MAX_NUM_RESULTS 256

typedef enum {
    JOB_STATUS_FAILED,
    JOB_STATUS_COMPLETED
} Job_Status;

typedef bool (*pfn_job_entry_point)(void *param_data, void *result_data);
typedef void (*pfn_job_callback)(Job_Status status, void *result_data);

typedef struct {
    pfn_job_entry_point entry_point;
    void *param_data;
    u64 param_data_size;
    pfn_job_callback callback;
    void *result_data;
    u64 result_data_size;
} Job;

typedef struct {
    pthread_t thread;
} Worker;

typedef struct {
    bool empty;
    Job_Status status;
    pfn_job_callback callback;
    void *result_data;
    u64 result_data_size;
} Job_Result;

typedef struct {
    u8 workers_count;
    Worker workers[JOB_SYSTEM_MAX_NUM_WORKERS];
    pthread_mutex_t job_queue_lock;
    pthread_cond_t job_queue_notify;
    Ring_Queue job_queue;
    pthread_mutex_t results_lock;
    Job_Result results[JOB_SYSTEM_MAX_NUM_RESULTS];
    bool running;
} Job_System;

bool job_system_init(Job_System *js, u8 num_workers);
void job_system_shutdown(void);
void job_system_update(void);
void job_system_submit(pfn_job_entry_point entry_point, void *param_data, u64 param_data_size, pfn_job_callback callback, u64 result_data_size);
