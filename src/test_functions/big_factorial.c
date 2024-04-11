#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#define MAX_NUM_LENGTH 500000 // 定义最大长度，用于存储大数阶乘的结果

static int result[MAX_NUM_LENGTH];

// 大数乘法函数，用于计算阶乘
void multiply(int n) {
    int carry = 0; // 进位
    for (int i = 0; i < MAX_NUM_LENGTH; i++) {
        int prod = result[i] * n + carry;
        result[i] = prod % 10; // 存储当前位的结果
        carry = prod / 10; // 计算进位
    }
}

// 计算给定数的阶乘
// n=1000->len=2567 t=1943.8580ms
// n=5000->len=16325 t=9637.1520ms
char* func(int argc, char** argv) {
    static char response_str[MAX_NUM_LENGTH];
    if (argc != 1) {
        sprintf(response_str, "Error: Invalid number of arguments");
        return response_str;
    }
    struct timeval start, end;
    double compute_latency;
    gettimeofday(&start, NULL);

    int n=atoi(argv[0]);
    memset(result, 0, sizeof(result)); // 初始化结果数组
    result[0] = 1; // 设置0的阶乘为1

    for (int i = 2; i <= n; i++) {
        multiply(i);
    }

    // 打印结果
    int len = MAX_NUM_LENGTH - 1;
    while (len >= 0 && result[len] == 0) len--; // 跳过前导0
    if (len == -1) {
        sprintf(response_str, "0");
        return response_str; // 如果i为-1，则结果为0
    }
    
    // 返回阶乘的值
    // for (int p=0; len >= 0; len--,p++) {
    //     sprintf(response_str+p, "%d", result[len]);
    // }

    // 返回阶乘的长度
    gettimeofday(&end, NULL);
    compute_latency = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0; // s
    sprintf(response_str, "latency: %.3f s, length of %d! is %d",compute_latency,n, len);
    return response_str;
}

// int main(int argc, char *argv[]){
//     char* str = func(argc-1,argv+1);
//     printf("%s\n",str);
//     return 0;
// }
