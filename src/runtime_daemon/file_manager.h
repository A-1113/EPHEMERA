#ifndef _FILE_MANAGER_H_
#define _FILE_MANAGER_H_

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dlfcn.h>

#include "structs.h"
#include "../lib/tools.h"

extern int IS_ONLY_STORE_IN_DISK; // 默认文件可以存在内存中
extern int* memory_limit_ptr; // 内存上限
void set_memory_limit(int* _memory_limit_ptr); // 设置此次运行时的内存上限
void set_file_only_in_disk(); // 设置为将文件仅存在磁盘中，但是会更新内存文件系统的数据结构
void set_file_in_memory(); // 设置文件可以存在内存中

void init_root_inode();
void remove_inodes();
void remove_added_inodes();

inode_t* find_file_by_path(const char* path);
inode_t* find_file_by_fd(int fd);
inode_t* create_directory(const char* path, __mode_t mode);
inode_t* create_file(const char* path, int flags, __mode_t mode, bool create_in_disk);
void modify_first_flags(int *flags);
bool is_in_memory(file_info_t* file);
bool is_in_disk(file_info_t* file);
void set_open_file_info(file_info_t* file, int flags);
inode_t* find_evict_file();
inode_t* find_bring_file();

int get_total_file_size();// 获得当前所有文件的大小之和

extern int (*original_mkdir)(const char *path, mode_t mode);// 原始的mkdir函数

void scheduler_file_out(file_info_t* file); // 将文件调出内存
#endif