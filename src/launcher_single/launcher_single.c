/*
与/src/launcher.c功能类似，单独提出，方便进行但容器测试
可以根据main函数的参数argc和argv来进行结果输出
*/
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <stdarg.h>

#include "../lib/tools.h"

/*
    开ONLY_IN_DISK 关ONLY_IN_MEMORY 表示用传统的文件系统
    关ONLY_IN_DISK 开ONLY_IN_MEMORY 表示无调度的内存文件系统
    关ONLY_IN_DISK 关ONLY_IN_MEMORY 表示有调度的内存文件系统
*/

// #define ONLY_IN_DISK
#define ONLY_IN_MEMORY

void* myso;
// 运行的实际函数
char* (*func)(int argc, char** argv);
// profiler线程相关
void (*start_profiler)(pthread_t *, pid_t , int* , char* );
void (*stop_profiler)(pthread_t *);

#ifndef ONLY_IN_DISK
// 内存文件系统存储相关
void (*init_root_inode)();
void (*remove_inodes)();
void (*remove_added_inodes)();
void (*set_memory_limit)(int*);
void (*set_file_only_in_disk)();
void (*set_file_in_memory)();

// 拦截指令相关
int (*exec_mkdir)(const char* , __mode_t );
int (*exec_open)(const char* , int , __mode_t );
int (*exec_close)(int fd);
__ssize_t (*exec_read)(int , void* , size_t );
__ssize_t (*exec_write)(int , const void* , size_t );
off_t (*exec_lseek)(int , off_t , int );
// 监控线程相关
void (*start_monitor)(pthread_t*, pid_t , int* );
void (*stop_monitor)(pthread_t*);

void setup_runtime_daemon(){
    // 动态加载runtime daemon中的函数，实现指令拦截、文件调度
    myso = dlopen("./build/runtime_daemon.so", RTLD_NOW);
    if(!myso){
        fprintf(stderr, "Error opening library: %s\n", dlerror());
    }
    *(void **)(&init_root_inode) = dlsym(myso, "init_root_inode");
    *(void **)(&remove_inodes) = dlsym(myso, "remove_inodes");
    *(void **)(&remove_added_inodes) = dlsym(myso, "remove_added_inodes");
    *(void **)(&set_memory_limit) = dlsym(myso, "set_memory_limit");
    *(void **)(&set_file_only_in_disk) = dlsym(myso, "set_file_only_in_disk");
    *(void **)(&set_file_in_memory) = dlsym(myso, "set_file_in_memory");

    *(void **)(&exec_mkdir) = dlsym(myso, "exec_mkdir");
    *(void **)(&exec_open) = dlsym(myso, "exec_open");
    *(void **)(&exec_close) = dlsym(myso, "exec_close");
    *(void **)(&exec_read) = dlsym(myso, "exec_read");
    *(void **)(&exec_write) = dlsym(myso, "exec_write");
    *(void **)(&exec_lseek) = dlsym(myso, "exec_lseek");

    *(void **)(&start_monitor) = dlsym(myso, "start_monitor");
    *(void **)(&stop_monitor) = dlsym(myso, "stop_monitor");

    // 初始化root inode
    init_root_inode();
}
#endif

void setup_func_and_profiler(){
    // 动态加载真实运行的函数
    myso = dlopen("./build/main.so", RTLD_NOW);
    *(void **)(&func) = dlsym(myso, "func");
    myso = dlopen("./build/runtime_daemon.so", RTLD_NOW);
    *(void **)(&start_profiler) = dlsym(myso, "start_profiler");
    *(void **)(&stop_profiler) = dlsym(myso, "stop_profiler");
}

#ifndef ONLY_IN_DISK
int mkdir(const char* path, __mode_t mode){
    return exec_mkdir(path,mode);
}
int open(const char* path, int flags, ...){
    va_list valist;
    va_start(valist, flags); 
    __mode_t mode = va_arg(valist, __mode_t);
    va_end(valist);
    return exec_open(path, flags, mode);
}
int close(int fd){
    return exec_close(fd);
}
__ssize_t read(int fd, void* buf, size_t count){
    return exec_read(fd, buf, count);
}
__ssize_t write(int fd, const void* buf, size_t count){
    return exec_write(fd, buf, count);
}
off_t lseek(int fd, off_t offset, int whence){
    return exec_lseek(fd, offset, whence);
}
#endif

int MEMORY_LIMIT;
void arg_check(int argc, char** argv) {
// argv包含0. 当前函数的可执行文件 1. 内存限制 2.后续为函数所需参数
    if(argc > 1){
        MEMORY_LIMIT = atoi(argv[1]);
    }else{
        puts("args too few");
        exit(0);
    }
}

int main(int argc, char** argv) {
    arg_check(argc, argv);
    setup_func_and_profiler();

#ifndef ONLY_IN_DISK
    setup_runtime_daemon();
    set_file_in_memory();
#endif

#if (!defined ONLY_IN_DISK) &&(!defined ONLY_IN_MEMORY)
    // 创建线程，监控子进程的内存使用量，进行调度
    pthread_t tid;
    start_monitor(&tid, getpid(), &MEMORY_LIMIT);
#endif

    for(int invoke_times=1;invoke_times<=5;invoke_times++){
        // 运行实际的函数
        #ifndef ONLY_IN_DISK
            set_memory_limit(&MEMORY_LIMIT); // 设置内存上限
            set_file_in_memory(); // 可以清空penalty
        #endif
        char* response_str = func(argc-2, argv+2);
        printf("%d invoke:\t%s\n",invoke_times, response_str);
        response_str = func(argc-2, argv+2);
    }

#if (!defined ONLY_IN_DISK) &&(!defined ONLY_IN_MEMORY)
    stop_monitor(&tid);
    pthread_join(tid, NULL);
#endif

    // 释放inode空间
#ifndef ONLY_IN_DISK
    remove_inodes();
#endif
    dlclose(myso);
    return 0;
}