#include "disk_manager.h"

// 输入路径path和权限mode，返回是否成功
int exec_mkdir_disk(const char* path, __mode_t mode) {
    inode_t* node = find_file_by_path(path);

    if (node != NULL) {// 路径已经存在
        return -1;
    }

    if(create_directory(path, mode) == NULL){ // 父目录不存在，无法创建
        return -1;
    }
    
    return original_mkdir(path, mode);
}

// 输入路径path，操作模式flags和创建文件时的权限mode，返回是否fd
int exec_open_disk(const char* path, int flags, __mode_t mode, inode_t* node) {
    jprintf(LVL_DEBUG, "[disk_manager]file \"%s\" in disk\n", path);
    set_open_file_info(node->file, flags); // 设置访问方式和offset

    node->file->disk_fd = __open(path, flags, mode);
    if(flags & O_TRUNC) {
        node->file->file_size=0;
    }
    node->file->is_open = true;
    return node->file->disk_fd; // 返回内存中记录的fd
}

// 输入fd，返回是否成功
int exec_close_disk(int fd, inode_t* node) {
    node->file->is_open = false;
    int result = __close(node->file->disk_fd);
    node->file->disk_fd=-1;
    return result;
}

// 输入fd，存放的数据区域的指针buf，最大存储长度count，返回读取的长度
__ssize_t exec_read_disk(int fd, void* buf, size_t count, inode_t* node) {
    file_info_t* nowFile = node->file;
    __ssize_t read_length = __read(nowFile->disk_fd, buf, count);
    nowFile->offset += read_length;
    return read_length;
}

// 输入fd，待写入的文件区域指针buf，最大写入长度count，返回实际写入的长度
__ssize_t exec_write_disk(int fd, const void* buf, size_t count, inode_t* node) {
    file_info_t* nowFile = node->file;
    if(nowFile->flags & O_APPEND){
        nowFile->offset = nowFile->file_size;
    }

    __ssize_t write_length = __write(nowFile->disk_fd, buf, count);
    nowFile->file_size = max(nowFile->file_size, nowFile->offset+write_length);
    nowFile->offset += write_length;
    return write_length;
}

// 输入fd，相对whence的偏移量，基准whence，返回相对于文件开始的偏移量
off_t exec_lseek_disk(int fd, off_t offset, int whence, inode_t* node) {
    file_info_t* nowFile = node->file;
    off_t new_offset = __lseek(fd, offset, whence);
    nowFile->offset = new_offset;
    return new_offset;
}