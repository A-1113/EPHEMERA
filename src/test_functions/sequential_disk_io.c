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
sequential_disk_io
顺序磁盘访问，测试读写延迟(s)和带宽(MB/s)
input: 文件大小 file_size, 文件重复操作次数 repeat_cnt （*8192为操作文件量）
output: read_latency, read_bandwidth, write_latency, write_bandwidth
*/
char* func(int argc, char** argv) {
    static char response_str[1024];
    if (argc != 2) {
        sprintf(response_str, "Error: Invalid number of arguments");
        return response_str;
    }
    int file_size = atoi(argv[0])*1024*1024; // MB
    int init_repeat_cnt = atoi(argv[1]);

    char* file_path = "/tmp/output1.txt";

    int fd, repeat_cnt;
    struct timeval read_start, read_end, write_start, write_end;
    int flags = O_RDWR | O_CREAT;
    // int flags = O_RDWR | O_CREAT | O_SYNC; // 写会磁盘，超长阻塞时间

    // 写内容初始化
    int write_size =  BUFSIZ; // B
    char* write_content = (char*)malloc((write_size+2) * sizeof(char)); // +1存放\0
    memset(write_content, 0, write_size+2);
    get_rand_string(write_content, write_size);
    // 读内容初始化
    int read_size = BUFSIZ; // B
    char* read_content = (char*)malloc((read_size+2) * sizeof(char));//+1存放\0
    memset(read_content, 0, read_size+2);

    //-------------------fill-----------------------------
    jprintf(LVL_INFO, "[test]start fill file1\n");
    fd = open(file_path, flags, 0777);
    for(int i=1;i<=file_size/write_size;i++){
        write(fd, write_content, write_size);
    }
    close(fd);
    //-------------------read-----------------------------
    jprintf(LVL_INFO, "[test]start read file1\n");
    gettimeofday(&read_start, NULL);
    fd = open(file_path, flags, 0777);
    repeat_cnt = init_repeat_cnt;
    while(repeat_cnt>0){
        lseek(fd,0,SEEK_SET);
        for(int i=1;i<=file_size/read_size;i++){
            read(fd, read_content, read_size);
            repeat_cnt--;
            if(repeat_cnt<=0)break;
        }
    }
    close(fd);
    gettimeofday(&read_end, NULL);
    //--------------write-------------------------------
    jprintf(LVL_INFO, "[test]start write file1\n");
    gettimeofday(&write_start, NULL);
    fd = open(file_path, flags, 0777);
    repeat_cnt = init_repeat_cnt;
    while(repeat_cnt>0){
        lseek(fd,0,SEEK_SET);
        for(int i=1;i<=file_size/write_size;i++){
            write(fd, write_content, write_size);
            repeat_cnt--;
            if(repeat_cnt<=0)break;
        }
    }
    close(fd);
    gettimeofday(&write_end, NULL);

    double read_latency = get_wall_time_diff_ms(read_start, read_end); // ms
    double write_latency = get_wall_time_diff_ms(write_start, write_end); // ms
    
    double read_bandwidth = (init_repeat_cnt/1024.0)*(read_size/1024.0)/(read_latency/1000.0); // MB/s
    double write_bandwidth = (init_repeat_cnt/1024.0)*(write_size/1024.0)/(write_latency/1000.0); // MB/s

    free(write_content);
    free(read_content);

    sprintf(response_str, "read_latency: %.3f ms, read_bandwidth: %.3f MB/s, write_latency: %.3f ms, write_bandwidth: %.3f MB/s", read_latency, read_bandwidth, write_latency, write_bandwidth);
    return response_str;
}
// int main(int argc, char *argv[]){
//     char* str = func(argc-1,argv+1);
//     printf("%s\n",str);
//     return 0;
// }