#ifndef _DISK_MANAGER_H_
#define _DISK_MANAGER_H_

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "structs.h"
#include "file_manager.h"
#include "../lib/tools.h"

// 输入路径path和权限mode，返回是否成功
int exec_mkdir_disk(const char* path, __mode_t mode);

// 输入路径path，操作模式flags和创建文件时的权限mode，返回是否fd
int exec_open_disk(const char* path, int flags, __mode_t mode, inode_t* node);

// 输入fd，返回是否成功
int exec_close_disk(int fd, inode_t* node);

// 输入fd，存放的数据区域的指针buf，最大存储长度count，返回读取的长度
__ssize_t exec_read_disk(int fd, void* buf, size_t count, inode_t* node);

// 输入fd，待写入的文件区域指针buf，最大写入长度count，返回实际写入的长度
__ssize_t exec_write_disk(int fd, const void* buf, size_t count, inode_t* node);

// 输入fd，相对whence的偏移量，基准whence，返回相对于文件开始的偏移量
off_t exec_lseek_disk(int fd, off_t offset, int whence, inode_t* node);
#endif