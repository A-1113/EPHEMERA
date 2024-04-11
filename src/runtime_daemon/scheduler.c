#include "scheduler.h"

static int count;
static int stop_signal;

extern int IS_ONLY_STORE_IN_DISK;

double get_memory_usage(pid_t pid);
// 处理内存剩余过少，需要调出的情况
void handle_memory_less(pid_t monitor_pid, int* memroy_limit_ptr) {
    inode_t* node = find_evict_file();
    if(IS_ONLY_STORE_IN_DISK) return;

    if(node == NULL){
        // 临时补丁，似乎因为并行，找不到可以调出内存的node的同时，之前的内存被释放了
        if(get_memory_usage(monitor_pid)>*memroy_limit_ptr)
            jprintf(LVL_ERROR, "memory exceed\n");
        return;
    }
    
    // 记录调度时间
    struct timeval scheduler_start, scheduler_end;
    gettimeofday(&scheduler_start, NULL);

    // 调度开始
    node->file->is_scheduling = true;
    jprintf(LVL_DEBUG, "[scheduler]evict time:%.2f name: %s size:%ld\n",
    count*USLEEP_TIME/1000.0/1000.0, node->name, strlen(node->file->in_memroy_content)/1024/1024);

    // munmap自动将调出内存的文件写回磁盘
    pthread_mutex_lock(&node->file->mutex);
    scheduler_file_out(node->file);
    pthread_mutex_unlock(&node->file->mutex);
    
    // 记录调度时间
    gettimeofday(&scheduler_end, NULL);
    double scheduler_wall_time = get_wall_time_diff_s(scheduler_start, scheduler_end);
    jprintf(LVL_INFO, "[scheduler] m->d path:%s %.3fs\n",node->name, scheduler_wall_time);

    // 调度完成
    node->file->is_scheduling = false;
}

// 处理内存剩余过多，需要调入的情况
void handle_memory_more() {
    inode_t* node = find_bring_file();
    if(IS_ONLY_STORE_IN_DISK) return;

    if(node == NULL) return;
    
    // 记录调度时间
    struct timeval scheduler_start, scheduler_end;
    gettimeofday(&scheduler_start, NULL);
    jprintf(LVL_DEBUG, "[scheduler]bring time:%.2f name: %s\n", count*USLEEP_TIME/1000.0/1000.0, node->name);

    // 调度开始
    node->file->is_scheduling = true;
    
    // 通过mmap，将磁盘文件映射到内存空间中
    pthread_mutex_lock(&node->file->mutex);
    if(node->file->in_memroy_content == NULL && node->file->disk_fd != -1){
        // 可能出现文件在find_bring_file时打开，但此处关闭的情况，因此加上特判补丁
        node->file->in_memroy_content = mmap(
            NULL,
            MAX_CONTENT_LENGTH,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            node->file->disk_fd,
            0
        );
        // 标记文件在内存中
        node->file->place = STORE_IN_MEMORY;
    }
    
    pthread_mutex_unlock(&node->file->mutex);
    

    // 记录调度时间
    gettimeofday(&scheduler_end, NULL);
    double scheduler_wall_time = get_wall_time_diff_s(scheduler_start, scheduler_end);
    jprintf(LVL_INFO, "[scheduler] d->m path:%s %.3fs\n",node->name, scheduler_wall_time);
    // 调度结束
    node->file->is_scheduling = false;
}

FILE* log_memory;

// 返回pid所对应的进程的内存使用量，返回-1表示该pid不存在
double get_memory_usage(pid_t pid) {
    char statm_path[32];
    FILE* fp;
    long size, resident, share, text, lib, data, dt;

    // 构造proc文件路径
    sprintf(statm_path, "/proc/%d/statm", pid);
    
    fp = fopen(statm_path, "r");
    if (fp == NULL) {
        jprintf(LVL_ERROR, "[scheduler] process is closed \n");
        return -1;
    }

    // 读取内存使用量
    if(NEED_MEMORY_LOG){
        fscanf(fp, "%ld %ld %ld %ld %ld %ld %ld", &size, &resident, &share, &text, &lib, &data, &dt);
    }else{
        fscanf(fp, "%ld %ld", &size, &resident);
    }
    fclose(fp);

    long page_size = sysconf(_SC_PAGESIZE) / 1024; // KB

    if(NEED_MEMORY_LOG){
        // -------------------输出内存使用量至日志-----------------
        fprintf(log_memory, 
        "resident:%.2f,  memory: %.2f %.2f %.2f %.2f %.2f %.2f %.2f\n",resident*page_size/1024.0,
        size*page_size/1024.0, resident*page_size/1024.0, share*page_size/1024.0, 
        text*page_size/1024.0, lib*page_size/1024.0, data*page_size/1024.0, dt*page_size/1024.0);
        // -----------------------------------------------------
    }

    return 1.0*resident * page_size / 1024; // 转换为MB
}

void* memory_scheduler(void* args) {
    scheduler_args_t* scheduler_args_ptr = (scheduler_args_t*)args;
    pid_t monitor_pid = scheduler_args_ptr->monitor_pid;
    int* memory_limit_ptr = scheduler_args_ptr->memory_limit_ptr;
    if(NEED_MEMORY_LOG){
        log_memory = fopen("./log/log_memory.txt","w");
    }

    double mem_usage; 
    
    while (stop_signal != 1) {
        usleep(USLEEP_TIME); // 隔一段时间更新一次
        mem_usage = get_memory_usage(monitor_pid); // 获取进程的内存使用量
        if (mem_usage < *memory_limit_ptr) { // 内存剩余多，调入
            handle_memory_more();
        } else { // 内存剩余少，调出
            jprintf(LVL_DEBUG, "[scheduler]time:%.2f  mem: %.3f\n",
            count*USLEEP_TIME/1000.0/1000.0, mem_usage);
            handle_memory_less(monitor_pid, memory_limit_ptr);
        }
        count++;
    }
    if(NEED_MEMORY_LOG){
        fclose(log_memory);
    }

    free(scheduler_args_ptr);
    pthread_exit(NULL);
}

void start_monitor(pthread_t *tid_ptr, pid_t pid, int* memory_limit_ptr){
    stop_signal = 0;
    scheduler_args_t* scheduler_args_ptr = (scheduler_args_t*)malloc(sizeof(scheduler_args_t));
    scheduler_args_ptr->monitor_pid = pid;
    scheduler_args_ptr->memory_limit_ptr = memory_limit_ptr;
    pthread_create(tid_ptr, NULL, memory_scheduler, scheduler_args_ptr);
}

void stop_monitor(pthread_t *tid_ptr){
    stop_signal = 1;
}

void* profiler(void* args){
    stop_signal = 0;
    if(NEED_MEMORY_LOG){
        log_memory = fopen("./log/log_memory.txt","w");
    }

    profiler_args_t* profiler_args_ptr = (profiler_args_t*)args;
    pid_t monitor_pid = profiler_args_ptr->monitor_pid;
    int* memory_limit_ptr = profiler_args_ptr->memory_limit_ptr;
    char* profiler_info = profiler_args_ptr->profiler_info;
    
    double max_mem_usage = 0;
    while (stop_signal != 1) {
        usleep(USLEEP_TIME); // 隔一段时间更新一次
        double mem_usage = get_memory_usage(monitor_pid); // 获取进程的内存使用量
        if(max_mem_usage < mem_usage) max_mem_usage = mem_usage;
    }
    int max_file_size = get_total_file_size();
    // 将信息写入profiler_info
    sprintf(profiler_info, "%d %d %d", *memory_limit_ptr, (int)ceil(max_mem_usage), (int)ceil(max_file_size*1.0/1024/1024));
    if(NEED_MEMORY_LOG){
        fclose(log_memory);
    }

    free(profiler_args_ptr);
    pthread_exit(NULL);
}

void start_profiler(pthread_t *tid_ptr, pid_t pid, int* memory_limit_ptr, char* profiler_info){
    stop_signal = 0;
    profiler_args_t* profiler_args_ptr = (profiler_args_t*)malloc(sizeof(profiler_args_t));
    profiler_args_ptr->monitor_pid = pid;
    profiler_args_ptr->memory_limit_ptr = memory_limit_ptr;
    profiler_args_ptr->profiler_info = profiler_info;
    pthread_create(tid_ptr, NULL, profiler, profiler_args_ptr);
}

void stop_profiler(pthread_t *tid_ptr){
    stop_signal = 1;
}