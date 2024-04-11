#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "launcher.h"


int signal_stop_monitor_launcher;
// 监听launcher的返回值
void* monitor_launcher_output(void *arg){
    int read_fd = *((int*)arg);

    size_t buffer_size=1024;
    char* buffer = (char*)malloc(buffer_size * sizeof(char));
    while(signal_stop_monitor_launcher != 1){
        int r = read_all_file(buffer, buffer_size, read_fd);
        if(r == -1){
            break;
        }
        printf("%s\n", buffer);
        fflush(stdout);
    }

    free(buffer);
    pthread_exit(NULL);
}

int main() {
    // freopen("./input/input_proxy.txt","r",stdin);
    int MEMORY_LIMIT=-1;
    int pipe_input_fd[2];
    int pipe_result_fd[2];
    char buffer[1024];
    int argc;
    char** argv;

    // 创建管道
    pipe(pipe_input_fd);
    pipe(pipe_result_fd);
    int read_fd = pipe_result_fd[0];
    int write_fd = pipe_input_fd[1];
    
    // 启动launcher
    pthread_t thread_launcher_id;
    LauncherArgs launcher_args= {.memory_limit_ptr = &MEMORY_LIMIT, .read_fd=pipe_input_fd[0], .write_fd=pipe_result_fd[1]};
    pthread_create(&thread_launcher_id, NULL, launcher, (void *)&launcher_args);

    // 启动监听线程，监听launcher返回值
    pthread_t thread_monitor_id;
    signal_stop_monitor_launcher = 0;
    pthread_create(&thread_monitor_id, NULL, monitor_launcher_output, &read_fd);
    
    while (1) {
        // 处理输入
        fgets(buffer, sizeof(buffer), stdin);
        remove_trailing_newline(buffer);
        // 结束判断
        if(strcmp(buffer, "stop") == 0){
            char* message="stop";
            write_file(message, write_fd);
            break;
        }

        // 解析参数
        argc = parse_arguments(buffer, &argv);

        // 模拟程序运行，传入launcher的程序均为实际运行函数的参数
        if(strcmp(argv[0], "update")==0){ // 更新内存上限
            MEMORY_LIMIT = atoi(argv[1]);
        }else if(strcmp(argv[0], "invoke")==0){ // 启动函数相应请求
            char* message = get_message(argc, argv, 1);
            write_file(message, write_fd);
            free(message);
        }else if(strcmp(argv[0], "create")==0){ // 初次运行
            // 首次启动
            MEMORY_LIMIT = atoi(argv[1]);
            char* message = (char*)get_message(argc, argv, 2);
            write_file(message, write_fd);
            free(message);
        }
        // 释放空间
        free(argv);
    }
    pthread_join(thread_launcher_id, NULL);
    signal_stop_monitor_launcher = 1;
    pthread_join(thread_monitor_id, NULL);

    __close(read_fd);
    __close(write_fd);
    return 0;
}
