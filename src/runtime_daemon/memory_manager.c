#include "memory_manager.h"

// 输入路径path和权限mode，返回是否成功
int exec_mkdir_memory(const char* path, __mode_t mode) {
    inode_t* node = find_file_by_path(path);

    if(node!=NULL){// 路径已经存在
        return -1;
    }
    
    if(create_directory(path, mode) == NULL){ // 父目录不存在，无法创建
        return -1;
    }
    
    return original_mkdir(path, mode);
}

// 输入路径path，操作模式flags和创建文件时的权限mode，返回是否fd
int exec_open_memory(const char* path, int flags, __mode_t mode, inode_t* node, bool is_first_open) {
    jprintf(LVL_DEBUG, "[memory_manager]file \"%s\" in memory\n", path);
    set_open_file_info(node->file, flags); // 提前设置访问方式和offset，可以保证只写
    
    if(flags&O_WRONLY){ // mmap要求有读权限，需要读到内存中
        flags = (flags^O_WRONLY)|O_RDWR;
    }

    node->file->disk_fd = __open(path, flags, mode);

    if(flags&O_TRUNC) { // 如果open带有O_TRUNC参数，需要重置内存中的数据
        node->file->file_size=0;
        if(node->file->in_memroy_content){
            munmap(node->file->in_memroy_content, MAX_CONTENT_LENGTH);
            node->file->in_memroy_content = NULL;
        }
    }

    // mmap映射的空间大小预设置为MAX_CONTENT_LENGTH
    if(is_first_open || flags&O_TRUNC){
        ftruncate(node->file->disk_fd, MAX_CONTENT_LENGTH); // mmap 不能直接映射空文件，不然会报错
    }

    if(node->file->in_memroy_content == NULL){
        node->file->place = STORE_IN_MEMORY; // 避免打开时同时将文件调出，需要重新标记在内存中
        
        int mmap_flags=0;
        if(flags&O_RDONLY) mmap_flags=PROT_READ;
        if(flags&O_RDWR) mmap_flags=PROT_READ|PROT_WRITE;

        node->file->in_memroy_content = mmap(
            NULL,
            MAX_CONTENT_LENGTH,
            mmap_flags,
            MAP_SHARED,
            node->file->disk_fd,
            0
        );
        if (node->file->in_memroy_content == MAP_FAILED) {
            perror("mmap");
            return -1;
        }
    }

    node->file->is_open = true;

    return node->file->disk_fd;
}

// DELETE_IN_MEMORY_FILE确定关闭文件时是否清空内存，即munmap；1为清空，0为保留
#define DELETE_IN_MEMORY_FILE 0

// 输入fd，返回是否成功
int exec_close_memory(int fd, inode_t* node) {
    node->file->is_open = false;
    node->file->disk_fd=-1;
    if(DELETE_IN_MEMORY_FILE){
        munmap(node->file->in_memroy_content, MAX_CONTENT_LENGTH);
        node->file->in_memroy_content = NULL;
        
        node->file->place = STORE_IN_DISK;
    }
    return 0;
}

// 输入fd，存放的数据区域的指针buf，最大存储长度count，返回读取的长度
__ssize_t exec_read_memory(int fd, void* buf, size_t count, inode_t* node) {
    file_info_t* nowFile = node->file;
    int read_length = min(count, nowFile->file_size-nowFile->offset);
    memcpy((char*)buf, nowFile->in_memroy_content+nowFile->offset, read_length); // 可能会写入\0因此不能用strncpy
    nowFile->offset += read_length;
    return read_length;
}

// 输入fd，待写入的文件区域指针buf，最大写入长度count，返回实际写入的长度
__ssize_t exec_write_memory(int fd, const void* buf, size_t count, inode_t* node) {
    file_info_t* nowFile = node->file;
    if(nowFile->flags & O_APPEND){
        nowFile->offset = nowFile->file_size;
    }
    if(nowFile->in_memroy_content==NULL){
        jprintf(LVL_ERROR, "memory content of %s is empty", node->name);
        exit(1);   
    }
    int write_length = count; // 写入长度完全由count决定，buf中可能会有0，因此不能用strlen获取长度取小
    memcpy(nowFile->in_memroy_content+nowFile->offset, buf, write_length); // 可能会写入\0因此不能用strncpy
    nowFile->file_size = max(nowFile->file_size, nowFile->offset+write_length);
    nowFile->offset += write_length;
    return write_length;
}

// 输入fd，相对whence的偏移量，基准whence，返回相对于文件开始的偏移量
off_t exec_lseek_memory(int fd, off_t offset, int whence, inode_t* node){
    file_info_t* nowFile = node->file;

    if(whence == SEEK_SET){
        nowFile->offset = offset;
    }else if(whence==SEEK_CUR){
        nowFile->offset = nowFile->offset+offset;
    }else if(whence == SEEK_END){
        nowFile->offset = nowFile->file_size+offset;
    }

    // 超过边界要拉回来
    if(nowFile->offset<0) nowFile->offset = 0;
    if(nowFile->offset > nowFile->file_size){
        nowFile->file_size = nowFile->offset;
    }
    // if(nowFile->offset > nowFile->file_size) nowFile->offset = nowFile->file_size;

    return nowFile->offset;
}