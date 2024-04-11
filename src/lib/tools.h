#ifndef _TOOLS_H
#define _TOOLS_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#define END_FILE_TOKEN (char)'\n'

#define MAX_NUM_LENGTH 20
#define MAX_CMD_LENGTH 10
#define MAX_PATH_LENGTH 256
#define MAX_WRITE_LENGTH BUFSIZ
#define MAX_CONTENT_LENGTH (256*1024*1024)

// LVL
#define LVL_DEBUG 0
#define LVL_INFO 1
#define LVL_ERROR 2

#define LVL_NEED 2

// color
#define NONE "\033[m"
#define RED "\033[0;32;31m"
#define LIGHT_RED "\033[1;31m"
#define GREEN "\033[0;32;32m"
#define LIGHT_GREEN "\033[1;32m"
#define BLUE "\033[0;32;34m"
#define LIGHT_BLUE "\033[1;34m"
#define DARY_GRAY "\033[1;30m"
#define CYAN "\033[0;36m"
#define LIGHT_CYAN "\033[1;36m"
#define PURPLE "\033[0;35m"
#define LIGHT_PURPLE "\033[1;35m"
#define BROWN "\033[0;33m"
#define YELLOW "\033[1;33m"
#define LIGHT_GRAY "\033[0;37m"
#define WHITE "\033[1;37m"

#define jprintf(LVL, F, ...)                                                   \
  do {                                                                         \
    if(LVL < LVL_NEED){                                                        \
      continue;                                                                \
    }                                                                          \
    if (LVL == LVL_DEBUG) {                                                   \
        fprintf(stderr, BLUE F NONE, ##__VA_ARGS__);                           \
    }else if(LVL == LVL_INFO){                                                  \
        fprintf(stderr, YELLOW F NONE, ##__VA_ARGS__);                         \
    }else if(LVL == LVL_ERROR){                                \
        fprintf(stderr, LIGHT_RED F NONE, ##__VA_ARGS__);                      \
    }                                                                          \
    fflush(stderr);                                                            \
  } while (0)



void print_args(int argc, char** argv);
void print_string(char* str, int per_char_mode);

int min(int a, int b); // 获取a,b中的较大值
int max(int a, int b); // 获取a,b中的较小值

int get_timestamp(); // 获取当前的时间戳
double get_wall_time_diff_s(struct timeval start, struct timeval end); // 获取墙上时间，单位为ms
double get_wall_time_diff_ms(struct timeval start, struct timeval end); // 获取墙上时间，单位为ms

int parse_arguments(char *input_string, char*** argv_ptr); // 将input_string按照空格分隔，将单词个数存入argc,每个单词存入argv(需在外界free)，会修改input_string
void remove_trailing_newline(char *str); // 去除字符串末尾的换行
char* get_message(int argc, char** argv, int start_p); // 将argv中从start_p个开始，拼成一个字符串，需在外界free返回值

void write_file(char* write_content, int fd); // 用于管道通信的写操作
int read_all_file(char* lineptr, int n, int fd); // 用于管道通信的读操作
#endif