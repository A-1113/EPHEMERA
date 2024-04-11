#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#define MY_COMPRESS_N 256
#define MY_COMPRESS_MAXSIZE 80
#define MY_COMPRESS_SOME 1
#define MY_COMPRESS_Empty 0
#define MY_COMPRESS_FULL -1

typedef unsigned long int WeightType;
typedef unsigned char MyType;
typedef struct // 哈夫曼树
{
    MyType ch;                  // 存字符
    WeightType weight;          /* 用来存放各个结点的权值 */
    int parent, LChild, RChild; /*指向双亲、孩子结点的指针 */
} HTNode;

typedef struct // 队列
{
    int tag;
    int front;
    int rear;
    MyType length;
    char elem[MY_COMPRESS_MAXSIZE];
} SeqQueue;

void writeFile();
void printHFM(HTNode *ht, int n);
void code(char **hc, int n, unsigned char *ch);

int InitQueue(SeqQueue *Q)
{
    if (!Q)
        return 1;
    Q->tag = MY_COMPRESS_Empty;
    Q->front = Q->rear = 0;
    Q->length = 0;

    return 0;
}

int In_seqQueue(SeqQueue *Q, char x)
{
    if (Q->front == Q->rear && Q->tag == MY_COMPRESS_SOME)
        return MY_COMPRESS_FULL; // full

    Q->elem[Q->rear] = x; // printf("in = %c",x);
    Q->rear = (Q->rear + 1) % MY_COMPRESS_MAXSIZE;
    Q->length++;
    Q->tag = MY_COMPRESS_SOME;
    return MY_COMPRESS_SOME;
}

int Out_Queue(SeqQueue *Q, char *x)
{
    if (Q->tag == MY_COMPRESS_Empty)
        return MY_COMPRESS_Empty;

    *x = Q->elem[Q->front];
    Q->length--;
    Q->front = (Q->front + 1) % MY_COMPRESS_MAXSIZE;

    if (Q->front == Q->rear)
        Q->tag = MY_COMPRESS_Empty;

    return MY_COMPRESS_SOME;
}

/* ------------------以上是队列的操作-------------------------  */

void SelectMinTree(HTNode *ht, int n, int *k)
{
    int i, temp;
    WeightType min;

    //      printf(" Selecting……n= %d",n);
    for (i = 0; i <= n; i++)
    {
        if (0 == ht[i].parent)
        {
            min = ht[i].weight; // init min
            temp = i;
            break;
        }
    }
    for (i++; i <= n; i++)
    {
        if (0 == ht[i].parent && ht[i].weight < min)
        {
            min = ht[i].weight;
            temp = i;
        }
    }
    *k = temp;
}

// 对哈夫曼树排序，并统计叶子数量
int SortTree(HTNode *ht)
{
    short i, j;
    HTNode tmp;

    for (i = MY_COMPRESS_N - 1; i >= 0; i--)
    {
        for (j = 0; j < i; j++)
            if (ht[j].weight < ht[j + 1].weight)
            {
                tmp = ht[j];
                ht[j] = ht[j + 1];
                ht[j + 1] = tmp;
            }
    }
    for (i = 0; i < MY_COMPRESS_N; i++)
        if (0 == ht[i].weight)
            return i;
    return i; // 返回叶子个数
}

// 求哈夫曼0-1字符编码表
char **CrtHuffmanCode(HTNode *ht, short LeafNum)
/*从叶子结点到根，逆向求每个叶子结点对应的哈夫曼编码*/
{
    char *cd, **hc; // 容器
    int i, start, p, last;

    hc = (char **)malloc((LeafNum) * sizeof(char *)); /*分配n个编码的头指针 */

    if (1 == LeafNum) // 只有一个叶子节点时
    {
        hc[0] = (char *)malloc((LeafNum + 1) * sizeof(char));
        strcpy(hc[0], "0");
        return hc;
    }

    cd = (char *)malloc((LeafNum + 1) * sizeof(char)); /*分配求当前编码的工作空间 */
    cd[LeafNum] = '\0';                                /*从右向左逐位存放编码，首先存放编码结束符 */
    for (i = 0; i < LeafNum; i++)
    {                    /*求n个叶子结点对应的哈夫曼编码 */
        start = LeafNum; /*初始化编码起始指针 */
        last = i;
        for (p = ht[i].parent; p != 0; p = ht[p].parent)
        { /*从叶子到根结点求编码 */
            if (ht[p].LChild == last)
                cd[--start] = '0'; /*左分支标0 */
            else
                cd[--start] = '1'; /*右分支标1 */
            last = p;
        }
        hc[i] = (char *)malloc((LeafNum - start) * sizeof(char)); /*为第i个编码分配空间 */
        strcpy(hc[i], &cd[start]);
    } 
    free(cd);
    return hc;
}

HTNode *CreatHFM(int fileDescriptor, short *n, WeightType *FileLength)
{
    HTNode *ht = NULL;
    int i, m, s1, s2;
    MyType ch;

    ht = (HTNode *)malloc(2 * MY_COMPRESS_N * sizeof(HTNode));
    if (!ht)
        exit(1);

    for (i = 0; i < MY_COMPRESS_N; i++)
    {
        ht[i].weight = 0;
        ht[i].ch = (MyType)i;
    }

    for (*FileLength = 0;; ++(*FileLength))
    {
        ssize_t bytesRead = read(fileDescriptor, &ch, sizeof(MyType));
        if (bytesRead <= 0)
            break;

        ht[ch].weight++;
    }

    *n = SortTree(ht);
    m = *n * 2 - 1;

    if (1 == *n)
    {
        ht[0].parent = 1;
        return ht;
    }
    else if (0 > *n)
        return NULL;

    for (i = m - 1; i >= 0; i--)
    {
        ht[i].LChild = 0;
        ht[i].parent = 0;
        ht[i].RChild = 0;
    }

    for (i = *n; i < m; i++)
    {
        SelectMinTree(ht, i - 1, &s1);
        ht[s1].parent = i;
        ht[i].LChild = s1;

        SelectMinTree(ht, i - 1, &s2);
        ht[s2].parent = i;
        ht[i].RChild = s2;

        ht[i].weight = ht[s1].weight + ht[s2].weight;
    }

    return ht;
}
// 从队列里取8个字符（0、1），转换成一个字节
MyType GetBits(SeqQueue *Q)
{
    MyType i, bits = 0;
    char x;

    for (i = 0; i < 8; i++)
    {
        if (Out_Queue(Q, &x) != MY_COMPRESS_Empty)
        {
            if ('0' == x)
                bits = bits << 1;
            else
                bits = (bits << 1) | 1;
        }
        else
            break;
    } 

    return bits;
}

// 求最长（最短）编码长度
void MaxMinLength(int fileDescriptor, HTNode *ht, char **hc, short NLeaf, MyType *Max,
                  MyType *Min)
{
    int i;
    MyType length;

    *Max = *Min = strlen(hc[0]);
    for (i = 0; i < NLeaf; i++)
    {
        length = strlen(hc[i]);

        // 使用 write 替代 fwrite
        write(fileDescriptor, &ht[i].ch, sizeof(MyType)); // 字符写进文件
        write(fileDescriptor, &length, sizeof(MyType));   // 编码长度写进文件

        if (length > *Max)
            *Max = length;
        if (length < *Min)
            *Min = length;
    }
}

// 把出现过的字符编码表经过压缩写进文件
short CodeToFile(int fileDescriptor, char **hc, short n, SeqQueue *Q, MyType *length)
{
    int i;
    char *p;
    MyType j, bits;
    short count = 0;

    for (i = 0; i < n; i++) // 将n个叶子压缩并写入文件
    {
        for (p = hc[i]; '\0' != *p; p++)
            In_seqQueue(Q, *p);

        while (Q->length > 8)
        {
            bits = GetBits(Q); // 出队8个元素
            // 使用 write 替代 fputc
            write(fileDescriptor, &bits, sizeof(char));
            count++;
        }
    }

    *length = Q->length;
    i = 8 - *length;
    bits = GetBits(Q);
    for (j = 0; j < i; j++)
        bits = bits << 1;

    // 使用 write 替代 fputc
    write(fileDescriptor, &bits, sizeof(char));
    count++;

    InitQueue(Q);
    return count;
}

void Compress(char des[], char ren[])
{
    char desFile[80], rename[80];
    MyType maxLen, minLen, ch, bits, n, finalLength;
    int i;
    short LeafNum, codeNum;
    WeightType count = 0, Length = 0, FileLength;
    int fp, compressFile; // 修改文件指针类型
    SeqQueue *Q;
    HTNode *ht = NULL;
    char **hc = NULL, **Map = NULL, *p;
    
    strcpy(desFile, des);
    strcpy(rename, ren);
    compressFile = open(rename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    fp = open(desFile, O_RDONLY); // 原文件

    if (fp == -1 || compressFile == -1)
    {
        perror("Cannot open file");
        return;
    }

    ht = CreatHFM(fp, &LeafNum, &FileLength);
    if (!FileLength)
    {
        close(fp);
        close(compressFile);
        free(ht);
        return;
    }
    Q = (SeqQueue *)malloc(sizeof(SeqQueue));
    InitQueue(Q);

    hc = CrtHuffmanCode(ht, LeafNum);
    Map = (char **)malloc(MY_COMPRESS_N * sizeof(char *));
    if (!Map)
    {
        puts("failed to apply new memory");
        return;
    }

    for (i = 0; i < MY_COMPRESS_N; i++)
        Map[i] = NULL;

    for (i = 0; i < LeafNum; i++)
        Map[(int)(ht[i].ch)] = hc[i];

    lseek(compressFile, sizeof(WeightType) + sizeof(short) + 6 * sizeof(MyType), SEEK_SET);

    MaxMinLength(compressFile, ht, hc, LeafNum, &maxLen, &minLen);

    free(ht);

    codeNum = CodeToFile(compressFile, hc, LeafNum, Q, &finalLength);

    lseek(compressFile, 0, SEEK_SET);
    lseek(compressFile, sizeof(WeightType) + sizeof(MyType), SEEK_SET);
    write(compressFile, &LeafNum, sizeof(short));
    write(compressFile, &maxLen, sizeof(MyType));
    write(compressFile, &minLen, sizeof(MyType));
    write(compressFile, &codeNum, sizeof(short));
    write(compressFile, &finalLength, sizeof(MyType));

    lseek(compressFile, 2 * LeafNum * sizeof(MyType) + codeNum, SEEK_CUR);

    lseek(fp, 0, SEEK_SET);

    while (count < FileLength)
    {
        read(fp, &ch, sizeof(MyType));
        ++count;
        for (p = Map[ch]; *p != '\0'; p++)
            In_seqQueue(Q, *p);

        while (Q->length > 8)
        {
            bits = GetBits(Q);
            write(compressFile, &bits, sizeof(MyType));
            Length++;
        }
    }

    finalLength = Q->length;
    n = 8 - finalLength;
    bits = GetBits(Q);
    for (i = 0; i < n; i++)
        bits = bits << 1;

    write(compressFile, &bits, sizeof(MyType));
    Length++;

    lseek(compressFile, 0, SEEK_SET);
    write(compressFile, &Length, sizeof(WeightType));
    write(compressFile, &finalLength, sizeof(char));

    Length = Length + 12 + codeNum;

    close(fp);
    close(compressFile);
    free(Q);
    free(hc);
    free(Map);
}

// 把读出的字符，转换成8个0、1字符串并入队
void ToQueue(SeqQueue *Q, MyType ch)
{
    int i;
    MyType temp;

    for (i = 0; i < 8; i++)
    {
        temp = ch << i;
        temp = temp >> 7;
        if (1 == temp)
            In_seqQueue(Q, '1'); // printf("1");
        else
            In_seqQueue(Q, '0'); // printf("0");
    }                            // puts("");
}

/*
gzip_compression
先测试写文件，再测试压缩文件后写入压缩包
input: 文件大小 file_size
output: disk_latency, compress_latency
*/
char *func(int argc, char *argv[])
{
    static char result[100];

    if (argc != 1)
    {
        sprintf(result, "Error: Invalid number of arguments");
        return result;
    }

    int file_size = atoi(argv[0]);
    
    char* file_write_path = "/tmp/my_test_output1.txt"; // 如果和/tmp中原本存在的文件重名可能会Permission denied
    char* compress_path ="/tmp/my_test_result1.gz";

    struct timeval start, end;
    double disk_latency, compress_latency;

    // 第一段功能
    unsigned long long buffer_size = file_size * 1024 * 1024;
    char *buffer = malloc(buffer_size);
    if (buffer == NULL)
    {
        sprintf(result, "Error: Memory allocation failed");
        return result;
    }
    srand((unsigned int)time(NULL));
    for (unsigned long long i = 0; i < buffer_size; ++i)
    {
        buffer[i] = (char)(rand() % 256); // 生成伪随机数
    }
    gettimeofday(&start, NULL);
    int fd = open(file_write_path, O_RDWR | O_CREAT, 0666);
    if (fd == -1)
    {
        sprintf(result, "Error: Unable to open/create file");
        return result;
    }

    if (write(fd, buffer, buffer_size) == -1)
    {
        sprintf(result, "Error: Unable to write to file");
        return result;
    }
    close(fd);
    gettimeofday(&end, NULL);
    free(buffer);
    disk_latency = (end.tv_sec - start.tv_sec)*1000 + (end.tv_usec - start.tv_usec) / 1000.0;

    // 第二段功能
    gettimeofday(&start, NULL);
    Compress(file_write_path, compress_path);
    gettimeofday(&end, NULL);
    compress_latency = (end.tv_sec - start.tv_sec)*1000 + (end.tv_usec - start.tv_usec) / 1000.0;

    sprintf(result, "disk_latency: %.3f ms, compress_latency: %.3f ms", disk_latency, compress_latency);
    return result;
}

// int main(int argc, char *argv[])
// {
//     char *result = func(argc-1, argv+1);
//     printf("%s\n", result);
//     return 0;
// }