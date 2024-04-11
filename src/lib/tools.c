#include "tools.h"

void print_args(int argc, char** argv) {
    for (int i = 0;i < argc;i++) {
        printf("argc[%d]: %s\n", i, argv[i]);
    }
}

void print_string(char* str, int per_char_mode) {
    jprintf(LVL_DEBUG, "-----------string-------------\n");
    jprintf(LVL_DEBUG, "%s\n", str);
    if(per_char_mode) {
        int n = strlen(str);
        for (int i = 0;i < n;++i) {
            jprintf(LVL_DEBUG, "i=%d: %c %d\n", i, str[i], str[i]);
        }
    }
    jprintf(LVL_DEBUG, "----------------------------\n");
}

int min(int a,int b){
    return a<b?a:b;
}

int max(int a,int b){
    return a>b?a:b;
}

int get_timestamp(){
    return clock() / 1000; // 除以1000，来让int没那么容易超限
}

double get_wall_time_diff_s(struct timeval start, struct timeval end){
    return (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
}

double get_wall_time_diff_ms(struct timeval start, struct timeval end){
    return (end.tv_sec - start.tv_sec)*1000 + (end.tv_usec - start.tv_usec) / 1000.0;
}

// 去除字符串末尾的换行符
void remove_trailing_newline(char *str) {
    if (str == NULL || *str == '\0') {
        return;  // 处理空字符串或空指针
    }

    int length = strlen(str);

    // 检查字符串末尾的字符是否是换行符
    while (length > 0 && str[length - 1] == '\n') {
        // 将换行符替换为 null 字符
        str[length - 1] = '\0';
    }
}

// 将input_string按照空格分隔，将单词个数存入argc,每个单词存入argv，会修改input_string
int parse_arguments(char *input_string, char*** argv_ptr) {
    // Count the number of arguments
    int argc = 0;
    char *temp = input_string;
    while(*temp == ' ') temp++;
    while (*temp != '\0') {
        if (*temp == ' ') {
            argc++;
            while (*temp == ' ') {
                temp++;  // Skip consecutive spaces
            }
        } else {
            temp++;
        }
    }
    if (*input_string != '\0' && input_string[strlen(input_string)-1] != ' ') {
        argc++;  // Count the last argument
    }

    // Allocate memory for argv
    *argv_ptr = (char **)malloc((argc + 1) * sizeof(char *));
    char** argv = *argv_ptr;
    if (argv == NULL) {
        perror("Memory allocation failed");
        exit(1);
    }

    // Copy arguments to argv
    temp = input_string;
    for (int i = 0; i < argc; i++) {
        while (*temp == ' ') {
            temp++;  // Skip leading spaces
        }
        argv[i] = temp;
        while (*temp != ' ' && *temp != '\0') {
            temp++;
        }
        if (*temp != '\0') {
            *temp = '\0';  // Null-terminate the argument
            temp++;
        }
    }
    argv[argc] = NULL;  // Null-terminate the argv array

    return argc;
}

// 将argv中从start_p个开始，拼成一个字符串，需在外界free返回值
char* get_message(int argc, char** argv, int start_p){
    int total_length = 1; // 1表示开头的空格
    for(int i=start_p;i<argc;i++)
        total_length += strlen(argv[i])+1;
    
    char* message = (char*)malloc(total_length*sizeof(char));

    int t=0;
    message[t++] = ' '; //增加一个空格，保证有内容写入pipe
    for(int i=start_p;i<argc;i++){
        int len = strlen(argv[i]);
        for(int j=0;j<len;j++)
            message[t++]=argv[i][j];
        message[t++]=' ';
    }
    message[t]='\0';
    return message;
}

// 用于管道通信的写操作
void write_file(char* write_content, int fd){
    __write(fd, write_content, strlen(write_content));
    char c = END_FILE_TOKEN;
    __write(fd, &c, 1);
}
// 用于管道通信的读操作
int read_all_file(char* str, int n, int fd){
    int t=0;
    char buffer[1];
    while(1){
        if(__read(fd, buffer, sizeof(buffer)) == 0){
            return -1;
        }
        if(buffer[0]==END_FILE_TOKEN)break;
        str[t++] = buffer[0];
        if(t>=n){
            perror("read too long");
            exit(1);
        }
    }
    str[t++] = '\0';
    return t;
}