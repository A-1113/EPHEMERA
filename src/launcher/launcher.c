#include "launcher.h"

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

void* launcher(void *arg){
    setup_func_and_profiler();

#ifndef ONLY_IN_DISK
    setup_runtime_daemon();
#endif

    LauncherArgs *args = (LauncherArgs *)arg;
    int* memory_limit_ptr = args->memory_limit_ptr;
    int read_fd = args->read_fd;
    int write_fd = args->write_fd;

    size_t buffer_size=1024;
    char* buffer = (char*)malloc(buffer_size * sizeof(char)); // 存储request
    char* profiler_info = (char*)malloc(buffer_size * sizeof(char)); //用于profiler返回结果
    int invoke_times = 0;
    while(1){
        char** argv;
        pthread_t tid;
        // 获取request，并解析
        read_all_file(buffer, buffer_size, read_fd);
        remove_trailing_newline(buffer);
        if(strcmp(buffer,"stop")==0){
            write_file("stop success",write_fd);
            break;
        }
        int argc = parse_arguments(buffer, &argv);

        if(invoke_times == 0){ // 启动之前需要分析程序，不将文件存入内存
            #ifndef ONLY_IN_DISK
                set_memory_limit(memory_limit_ptr);
                set_file_only_in_disk();
            #endif
            start_profiler(&tid, getpid(), memory_limit_ptr, profiler_info);
        }else{ // 后续启动使用内存文件系统
            #ifndef ONLY_IN_DISK
                set_memory_limit(memory_limit_ptr);
                set_file_in_memory();
            #endif
            #if (!defined ONLY_IN_DISK) &&(!defined ONLY_IN_MEMORY)
                // 创建线程，监控子进程的内存使用量，进行调度
                start_monitor(&tid, getpid(), memory_limit_ptr);
            #endif
        }

        // 运行实际的函数
        char* response_str = func(argc, argv);
        char result[1024];
        sprintf(result, "response: %s, memory_limit: %d MB",response_str, *memory_limit_ptr);
        response_str = result;
        if(invoke_times == 0){
            stop_profiler(&tid);
            pthread_join(tid, NULL);
            write_file(profiler_info, write_fd); // 将profile信息传递回去
        }else{
            #if (!defined ONLY_IN_DISK) &&(!defined ONLY_IN_MEMORY)
                if(invoke_times){
                    stop_monitor(&tid);
                    pthread_join(tid, NULL);
                }
            #endif
            write_file(response_str, write_fd); // 将函数结果传递回去
        }
        
        invoke_times++;
        free(argv);
        // remove_added_inodes(); // 实现串行隔离
    }

    // 释放inode空间
#ifndef ONLY_IN_DISK
    remove_inodes();
#endif
    dlclose(myso);
    free(buffer);
    free(profiler_info);
    // 关闭pipe
    __close(read_fd);
    __close(write_fd);
    pthread_exit(NULL);
}