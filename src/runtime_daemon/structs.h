#ifndef _MYSTRUCTS_H_
#define _MYSTRUCTS_H_
#include <sys/types.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "config.h"

// ----------存储文件结构体-------------------------------
#define STORE_IN_MEMORY 0
#define STORE_IN_DISK 1

typedef struct{
    int disk_fd; // 通过磁盘打开的fd
    bool place; // 文件当前存储的位置，内存中或磁盘中
    bool is_open; // 文件是否打开
    int file_size; // 文件的总字节数，单位为B
    int flags; // 打开文件的标识，如O_CREATE
    int offset; // 当前文件读到的位置
    // int use_time; // 记录时间戳，表示最近使用的时间，用于调入调出
    char* in_memroy_content; // 存储在内存中的文件内容
    pthread_mutex_t mutex; // 避免调度线程和主线程同时访问file造成问题
    bool is_scheduling; // 是否正处于调度状态

    int timestamp_in; // 最近一次调入的时间戳
    int timestamp_out; // 最近一次调出的时间戳
    int schedule_penalty; // 在阈值时间内调出需要增加惩罚，限制调入
}file_info_t;

int get_file_threshold();

typedef struct inode_t{
    // 目录信息
    char* name; // 目录名或文件名
    int mode; // rwx权限

    // 文件信息
    file_info_t* file; // 若为NULL，表示该inode为目录
} inode_t;

// ----------存储传递给scheduler的参数的结构体-------------------------------
typedef struct {
    pid_t monitor_pid;
    int* memory_limit_ptr;
} scheduler_args_t;

// ----------存储传递给profiler的参数的结构体-------------------------------
typedef struct {
    pid_t monitor_pid;
    int* memory_limit_ptr;
    char* profiler_info;
} profiler_args_t;

#endif