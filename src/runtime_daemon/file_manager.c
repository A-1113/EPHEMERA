#define _GNU_SOURCE
#include "file_manager.h"

#define INIT_FD 6

int IS_ONLY_STORE_IN_DISK = STORE_IN_MEMORY;
int *memory_limit_ptr = NULL; // 内存限制（MB）
static int nodes_num = 0;
static inode_t* nodes[100];
static char* root_name = "/tmp";
static int total_fd = INIT_FD;

void set_memory_limit(int* _memory_limit_ptr){
    memory_limit_ptr = _memory_limit_ptr;
}

void set_file_only_in_disk(){
    IS_ONLY_STORE_IN_DISK = STORE_IN_DISK;
}

void set_file_in_memory(){
    IS_ONLY_STORE_IN_DISK = STORE_IN_MEMORY;
    // // 将每个文件的初始位置放在内存中，并清空惩罚
    for (int i = 0;i < nodes_num;i++) {
        if(nodes[i]->file != NULL){
            nodes[i]->file->place = STORE_IN_MEMORY;
            nodes[i]->file->schedule_penalty = 0;
        }
    }
}

int (*original_mkdir)(const char *path, mode_t mode);
void init_root_inode() {
    if (nodes[0] == NULL) {
        nodes[0] = (inode_t*)malloc(sizeof(inode_t));
        nodes[0]->file = NULL;

        // 根目录为root_name
        nodes[0]->name = (char*)malloc(MAX_PATH_LENGTH * sizeof(char));
        strncpy(nodes[0]->name, root_name, strlen(root_name));
        nodes[0]->name[strlen(root_name)] = '\0';

        nodes[0]->mode = 0777;
        nodes_num++;
    }

    original_mkdir = dlsym(RTLD_NEXT, "mkdir");
}

void remove_inode(inode_t* node) {
    // 释放文件内容
    if (node->file != NULL) {
        pthread_mutex_lock(&node->file->mutex);
        if (node->file->in_memroy_content != NULL) {
            munmap(node->file->in_memroy_content, MAX_CONTENT_LENGTH);
            node->file->in_memroy_content = NULL;
        }
        pthread_mutex_unlock(&node->file->mutex);
        // 结束时将文件截断到对应的长度，方便查看
        FILE* file = fopen(node->name, "r+");
        if (ftruncate(fileno(file), node->file->file_size) != 0) {
            perror("Ftruncate Error");
            fclose(file);
            return;
        }
        fclose(file);
        pthread_mutex_destroy(&node->file->mutex);
        free(node->file);
    }

    // 释放文件名
    free(node->name);
    free(node);
}

void remove_inodes() {
    for (int i = 0;i < nodes_num;i++) {
        remove_inode(nodes[i]);
        nodes[i] = NULL;
    }
}

void remove_added_inodes() {
    int init_nodes_num = nodes_num;
    nodes_num = 1; // 提前变成1，减少调度器寻找调入文件时因为程序执行顺序访问到刚好被删除的节点
    total_fd = INIT_FD;
    
    for (int i = 1;i < init_nodes_num;i++) {
        remove_inode(nodes[i]);
        nodes[i] = NULL;
    }  
}

bool is_in_memory(file_info_t* file) {
    return file->place == STORE_IN_MEMORY;
}

bool is_in_disk(file_info_t* file) {
    return file->place == STORE_IN_DISK;
}

// 修改首次打开文件时的flags，保证是覆盖写
void modify_first_flags(int* flags) {
    // 首次写入采用覆盖写，增加O_TRUNC
    (*flags) = (*flags) | O_TRUNC;
    // 如果在首次调用时含有O_APPEND，需要去掉，避免影响O_TRUNC
    if ((*flags) & O_APPEND) {
        (*flags) = (*flags) ^ O_APPEND;
    }
}

inode_t* find_file_by_path(const char* path) {
    for (int i = 0;i < nodes_num;i++) {
        if (strcmp(nodes[i]->name, path) == 0) {
            return nodes[i];
        }
    }
    return NULL;
}

inode_t* find_file_by_fd(int fd) {
    for (int i = 0;i < nodes_num;i++) {
        if (nodes[i]->file && nodes[i]->file->disk_fd == fd) {
            return nodes[i];
        }
    }
    return NULL;
}

static bool is_parent_directory_exist(const char* path) {
    // 提取父目录
    int path_length = strlen(path);
    char* parent_path = (char*)malloc((path_length + 1) * sizeof(char));
    parent_path[0] = '\0';
    int i = path_length - 1;
    if (path[i] == '/') {
        i--;
    }
    while (i >= 0 && path[i] != '/') {
        i--;
    }

    if(i == -1)return false;
    
    strncpy(parent_path, path, i); // 不会自动添加'\0'
    parent_path[i] = '\0';
    return find_file_by_path(parent_path) != NULL;
}

inode_t* create_directory(const char* path, __mode_t mode) {
    if (!is_parent_directory_exist(path)) {// 父目录不存在，无法创建
        return NULL;
    }
    nodes[nodes_num] = (inode_t*)malloc(sizeof(inode_t));

    nodes[nodes_num]->name = (char*)malloc(MAX_PATH_LENGTH * sizeof(char));
    // memcpy(nodes[nodes_num]->name, path, strlen(path));
    strncpy(nodes[nodes_num]->name, path, strlen(path));
    nodes[nodes_num]->name[strlen(path)] = '\0';
    nodes[nodes_num]->mode = mode;
    nodes[nodes_num]->file = NULL;

    nodes_num++;
    return nodes[nodes_num - 1];
}

inode_t* create_file(const char* path, int flags, __mode_t mode, bool create_in_disk) {
    inode_t* node = create_directory(path, mode);
    if(node==NULL){ // 不存在父目录
        return NULL;
    }
    node->file = (file_info_t*)malloc(sizeof(file_info_t));

    file_info_t* nowFile = node->file;
    nowFile->disk_fd = -1;
    
    if(create_in_disk == STORE_IN_DISK){
        nowFile->place = STORE_IN_DISK;
    }else{
        nowFile->place = STORE_IN_MEMORY;
    }

    nowFile->flags = flags;
    nowFile->offset = 0;
    nowFile->file_size = 0;
    nowFile->timestamp_in = -1;
    nowFile->timestamp_out = -1;
    nowFile->schedule_penalty = 0;

    pthread_mutex_init(&nowFile->mutex, NULL);
    return node;
}

void set_open_file_info(file_info_t* file, int flags) {
    file->flags = flags;
    if(file->flags & O_APPEND){
        file->offset = file->file_size;
    }else{
        file->offset = 0;
    }
    return;
}

#define ADD_SCHEDULE_ALGORITHM

inode_t* find_evict_file(){
    // 调出关闭的文件
    for (int i = 1;i < nodes_num;i++) {
        file_info_t* now_file = nodes[i]->file;
        if(now_file && now_file->is_open==false && now_file->place == STORE_IN_MEMORY){
            #ifdef ADD_SCHEDULE_ALGORITHM
            now_file->timestamp_out = get_timestamp();
            now_file->schedule_penalty = 0; // 正常调出，不再限制调入
            #endif
            return nodes[i];
        }
    }

    // 调出已经打开的文件
    for (int i = 1;i < nodes_num;i++) {
        file_info_t* now_file = nodes[i]->file;
        if(now_file && now_file->is_open==true && now_file->place == STORE_IN_MEMORY){
            #ifdef ADD_SCHEDULE_ALGORITHM
            now_file->timestamp_out = get_timestamp();
            if(now_file->timestamp_out - now_file->timestamp_in > get_file_threshold()){
                now_file->schedule_penalty = 0; // 正常调出，不再限制调入
            }else{
                // 在阈值内调出，增加惩罚
                now_file->schedule_penalty = now_file->schedule_penalty?now_file->schedule_penalty*2:2;
            }
            #endif
            return nodes[i];
        }
    }
    
    return NULL;
}

inode_t* find_bring_file(){
    // 仅处理文件已打开且在磁盘中的情况，即disk_fd有值
    int init_nodes_num = nodes_num;
    for (int i = 1;i < init_nodes_num;i++) {
        // 文件已打开，但是在磁盘中，需要调入
        file_info_t* now_file = nodes[i]->file;
        if(now_file && now_file->disk_fd!=-1 && now_file->place == STORE_IN_DISK){
            #ifdef ADD_SCHEDULE_ALGORITHM
            // 如果现在还在惩罚时间就不调入
            int now_timestamp = get_timestamp();
            if(now_timestamp <= now_file->timestamp_out+1LL*now_file->schedule_penalty*get_file_threshold()){
                continue;
            }
            now_file->timestamp_in = now_timestamp;
            #endif
            return nodes[i];
        }
    }
    return NULL;
}

int get_total_file_size(){
    int total_file_size = 0;
    for (int i = 1;i < nodes_num;i++) {
        file_info_t* now_file = nodes[i]->file;
        if(now_file){
            total_file_size += now_file->file_size;
        }
    }
    return total_file_size;
}

void scheduler_file_out(file_info_t *file){
    if(file->in_memroy_content != NULL){
        // 标记文件存在磁盘中
        file->place = STORE_IN_DISK;
        munmap(file->in_memroy_content, MAX_CONTENT_LENGTH);
        file->in_memroy_content = NULL;
    }
}
