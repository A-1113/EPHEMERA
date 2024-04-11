#ifndef LAUNCHER_H
#define LAUNCHER_H
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <stdarg.h>

#include "../lib/tools.h"

typedef struct {
    int* memory_limit_ptr;
    int read_fd;
    int write_fd;
} LauncherArgs;

void* launcher(void *arg);

#endif