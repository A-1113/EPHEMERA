#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include <pthread.h>
#include <stdio.h>
#include <malloc.h>
#include <math.h>

#include "file_manager.h"
#include "memory_manager.h"
#include "disk_manager.h"
#include "../lib/tools.h"

double get_memory_usage(pid_t pid);

// 监控进程的内存使用量，arg表示需要监控的进程的pid
void *memory_scheduler(void *arg);

void start_monitor(pthread_t *tid_ptr, pid_t pid, int* memory_limit_ptr);
void stop_monitor(pthread_t *tid_ptr);

void start_profiler(pthread_t *tid_ptr, pid_t pid, int* memory_limit_ptr, char* profiler_info);
void stop_profiler(pthread_t *tid_ptr);
#endif