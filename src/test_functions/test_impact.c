#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <dlfcn.h>

#include "../lib/tools.h"

// 采样字符集 64个 10+1+26+26+1
static char char_table[] = "0123456789 abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\n";
int my_rand(){
    // return rand()%100 * 1024*1024;
    return rand();// RANDMAX 2147483647
}
void get_rand_string(char* str, int num){
    int table_length = strlen(char_table);
    int i = 0;
    while(i<num){
        str[i++] = char_table[my_rand() % table_length];
    }str[num]='\0';
}

/*
test_impact
打开file1, file1填满, 关闭file1
打开file2, file2填满, 关闭file2
打开file1, file1随机写, 关闭file1
打开file2, file2随机写, 关闭file2
打开file1, file1随机读, 关闭file1
打开file2, file2随机读, 关闭file2
input: 文件大小 file_size, 文件重复操作次数 repeat_cnt （*8192为操作文件量）
output: 总时间 total_wall_time, 填充时间 fill_wall_time, 读时间 read_wall_time, 写时间 write_wall_time
*/
char* func(int argc, char** argv) {
    static char response_str[1024];
    if (argc != 2) {
        sprintf(response_str, "Error: Invalid number of arguments");
        return response_str;
    }
    int file_size = atoi(argv[0])*1024*1024;
    int init_repeat_cnt = atoi(argv[1]);

    char* file1_path = "/tmp/output1.txt";
    char* file2_path  = "/tmp/output2.txt";

    int fd, repeat_cnt;
    struct timeval fill_start, fill_end, read_start, read_end, write_start, write_end;
    int flags = O_RDWR | O_CREAT;
    // int flags = O_RDWR | O_CREAT | O_SYNC; // 写会磁盘，超长阻塞时间

    // 写内容初始化
    int write_size =  BUFSIZ;
    int max_offset = file_size-write_size;
    char* write_content = (char*)malloc((write_size+2) * sizeof(char)); // +1存放\0
    memset(write_content, 0, write_size+2);
    get_rand_string(write_content, write_size);
    // 读内容初始化
    int read_size = BUFSIZ;
    char* read_content = (char*)malloc((read_size+2) * sizeof(char));//+1存放\0
    memset(read_content, 0, read_size+2);

    //-------------------fill-----------------------------
    jprintf(LVL_INFO, "[test]start fill file1\n");
    gettimeofday(&fill_start, NULL);
    fd = open(file1_path, flags, 0777);

    for(int i=1;i<=file_size/write_size;i++){
        write(fd, write_content, write_size);
    }
    close(fd);
    jprintf(LVL_INFO, "[test]start fill file2\n");
    fd = open(file2_path, flags, 0777);
    for(int i=1;i<=file_size/write_size;i++){
        write(fd, write_content, write_size);
    }
    close(fd);
    gettimeofday(&fill_end, NULL);

    //-------------------read-----------------------------
    jprintf(LVL_INFO, "[test]start read file1\n");
    gettimeofday(&read_start, NULL);
    fd = open(file1_path, flags, 0777);
    repeat_cnt = init_repeat_cnt;
    while(repeat_cnt--){
        lseek(fd,my_rand()%max_offset,SEEK_SET);
        read(fd, read_content, read_size);
    }
    close(fd);
    jprintf(LVL_INFO, "[test]start read file2\n");
    fd = open(file2_path, flags, 0777);
    repeat_cnt = init_repeat_cnt;
    while(repeat_cnt--){
        lseek(fd,my_rand()%max_offset,SEEK_SET);
        read(fd, read_content, read_size);
    }
    close(fd);
    gettimeofday(&read_end, NULL);
    //--------------write-------------------------------
    jprintf(LVL_INFO, "[test]start write file1\n");
    gettimeofday(&write_start, NULL);
    fd = open(file1_path, flags, 0777);
    repeat_cnt = init_repeat_cnt;
    while(repeat_cnt--){
        lseek(fd,my_rand()%max_offset,SEEK_SET);
        write(fd, write_content, write_size);
    }
    close(fd);
    jprintf(LVL_INFO, "[test]start write file2\n");
    fd = open(file2_path, flags, 0777);
    repeat_cnt = init_repeat_cnt;
    while(repeat_cnt--){
        lseek(fd,my_rand()%max_offset,SEEK_SET);
        write(fd, write_content, write_size);
    }
    close(fd);
    gettimeofday(&write_end, NULL);

    double fill_wall_time = get_wall_time_diff_s(fill_start, fill_end);
    double read_wall_time = get_wall_time_diff_s(read_start, read_end);
    double write_wall_time = get_wall_time_diff_s(write_start, write_end);
    double total_wall_time = fill_wall_time+read_wall_time+write_wall_time;
    
    free(write_content);
    free(read_content);

    sprintf(response_str, "latency: %.3f s",total_wall_time);
    return response_str;
}