#include "controller.h"

extern int IS_ONLY_STORE_IN_DISK;
extern int* memory_limit_ptr;

// 输入路径path和权限mode，返回是否成功
int exec_mkdir(const char* path, __mode_t mode) {
    if (IS_ONLY_STORE_IN_DISK) {
        return exec_mkdir_disk(path, mode);
    }
    return exec_mkdir_memory(path, mode);
}

// 输入路径path，操作模式flags和创建文件时的权限mode，返回是否fd
int exec_open(const char* path, int flags, __mode_t mode) {
    inode_t* node = find_file_by_path(path);
    bool is_first_open = (node==NULL);
    if (is_first_open) { // 实际只有在初次profiler时会判定为初次打开，除非在launcher中使用remove_added_inodes
        // 需要将写变为O_TRUNC，变成覆盖写，忽略上次文件写入的内容
        modify_first_flags(&flags);
        // 本次调用首次出现，需要创建，存储在内存中
        node = create_file(path, flags, mode, IS_ONLY_STORE_IN_DISK);
    }

    if (node == NULL || node->file == NULL) {
        jprintf(LVL_ERROR, "[ERROR] open file %s error, path must start with /tmp \n", path);
        exit(1);
    }

    // 已经打开，直接返回fd
    if (node->file->disk_fd > 0) {
        return node->file->disk_fd;
    }
    
    int ret = -1;

    pthread_mutex_lock(&node->file->mutex);
    
    // 如果文件大小大于内存上限则不存入内存
    if(node->file->file_size/1024/1024 > (*memory_limit_ptr) * 1.1){
        scheduler_file_out(node->file);
    }else if(!IS_ONLY_STORE_IN_DISK){
        node->file->place = STORE_IN_MEMORY;
    }

    // 该文件在本次操作中已经出现，需要确定存储在何处
    if (is_in_memory(node->file)) {// 存储在内存中
        ret = exec_open_memory(path, flags, mode, node, is_first_open);
    } else {// 存储在磁盘中
        ret = exec_open_disk(path, flags, mode, node);
    }
    pthread_mutex_unlock(&node->file->mutex);

    return ret;
}

// 输入fd，返回是否成功
int exec_close(int fd) {
    // 查询fd对应的文件信息
    inode_t* node = find_file_by_fd(fd);
    if (node == NULL || node->file == NULL) {
        jprintf(LVL_ERROR, "[ERROR] find fd:%d error\n", fd);
        exit(1);
    }
    int ret = -1;

    pthread_mutex_lock(&node->file->mutex);
    if (is_in_memory(node->file)) {
        ret = exec_close_memory(fd, node);
    } else {
        ret = exec_close_disk(fd, node);
    }
    pthread_mutex_unlock(&node->file->mutex);

    return ret;
}

// 输入fd，存放的数据区域的指针buf，最大存储长度count，返回读取的长度
__ssize_t exec_read(int fd, void* buf, size_t count) {
    // 查询fd对应的文件信息
    inode_t* node = find_file_by_fd(fd);
    if (node == NULL || node->file == NULL) {
        jprintf(LVL_ERROR, "[ERROR] find fd:%d error\n", fd);
        exit(1);
    }

    // 判断是否有读权限
    if(node->file->flags&O_WRONLY)return -1;

    __ssize_t ret = -1;

    pthread_mutex_lock(&node->file->mutex);
    if (is_in_memory(node->file)) {
        ret = exec_read_memory(fd, buf, count, node);
    } else {
        ret = exec_read_disk(fd, buf, count, node);
    }
    pthread_mutex_unlock(&node->file->mutex);

    return ret;
}

// 输入fd，待写入的文件区域指针buf，最大写入长度count，返回实际写入的长度
__ssize_t exec_write(int fd, const void* buf, size_t count) {
    // 查询fd对应的文件信息
    inode_t* node = find_file_by_fd(fd);
    if (node == NULL || node->file == NULL) {
        jprintf(LVL_ERROR, "[ERROR] find fd:%d error\n", fd);
        exit(1);
    }
    
    // 判断是否有写权限
    if(!(node->file->flags&O_WRONLY||node->file->flags&O_RDWR))return -1;

    __ssize_t ret = -1;

    pthread_mutex_lock(&node->file->mutex);
    if (is_in_memory(node->file)) {
        ret = exec_write_memory(fd, buf, count, node);
    } else {
        ret = exec_write_disk(fd, buf, count, node);
    }
    pthread_mutex_unlock(&node->file->mutex);

    return ret;
}

off_t exec_lseek(int fd, off_t offset, int whence) {
    // 查询fd对应的文件信息
    inode_t* node = find_file_by_fd(fd);
    if (node == NULL || node->file == NULL) {
        jprintf(LVL_ERROR, "[ERROR] find fd:%d error\n", fd);
        exit(1);
    }

    if (is_in_memory(node->file)) {
        return exec_lseek_memory(fd, offset, whence, node);
    } else {
        return exec_lseek_disk(fd, offset, whence, node);
    }
}