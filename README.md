# EPHEMERA

EPHEMERA is a framework based on serverless computing to optimize the efficiency of ephemeral storage access and memory utilization.

EPHEMERA consists of three components top-down: a _cluster-level_ Controller, a _tenant-level_ Manager, and a _function-level_ Daemon. 
The Runtime Daemon using an in-memory file system transforms file operations to memory-based for I/O optimization. The Tenant Manager enables dynamic memory sharing with security. The Cluster Controller dynamically adjusts workload and resources for optimal performance. 

Through prototype testing, EPHEMERA demonstrates an average 50\% improvement in file access performance and even 95.73\% optimization in specific conditions.

## Prerequisites

This project uses Docker and AWS CLI.

```shell
$ sudo yum update -y
$ sudo amazon-linux-extras install docker
$ sudo service docker start
```

```shell
$ curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip"
$ unzip awscliv2.zip
$ sudo ./aws/install
```

## Configurations
In `src/func/serverless_function.c`, you can write your serverless function. This function must take `func` as the function name, `int argc` and `char* argv[]` as parameters, and `char *` as the return value.

In `src/launcher/launcher.c`, you can choose `ONLY_IN_DISK` and `ONLY_IN_MEMORY` to change test mode. Besides, you can use `set_file_only_in_disk` or `set_file_in_memory` to change the location of files stored in the in-memory file system.

In `src/tenant_manger/tenant_manager.py`, you can change the value of `CONFIG_DISABLE_MEMORY_SHARE` to configure whether to use the memory-sharing mechanism.

In `src/cluster_controller/scheduler.py`, you can change the value of `CONFIG_TRADITIONAL_SCHEDULER` to configure whether to use traditional scheduler.

In `src/lib/tools.h`, you can change `LVL_NEED` to adjust the level of information printed.

## Characteristics
To validate the under-utilization of memory resources during the execution of functions on serverless computing platforms, select three representative functions from [FunctionBench](https://github.com/ddps-lab/serverless-faas-workbench) and evaluate them on AWS Lambda under different resource configurations.
The selected functions include _gzip compression_, _pyaes_ and _linpack_. These three functions require a minimum memory of 77 MB, 40 MB, and 93 MB for execution, respectively.

Deploy them on AWS Lambda and then use `test.sh` scripts in `characteristics/gzip/`, `characteristics/pyaes` and `characteristics/linpack` to obtain the results under different memory configuration.

## Runtime Daemon
The Runtime Daemon achieves a user-transparent memory file system for serverless functions.

**First**, evaluate the system by subjecting it to diverse conditions, altering memory constraints, file usage sizes, and the frequency of file operations. 

Following below commands, get results under different conditions by using EPHEMERA. 
The results will be stored in `output/micro_benchmarks/ephemera/`.

```sh
$ ./script/micro_benchmarks_ephemera.sh
```

Following below commands, get results under different conditions by using original OverlayFS.
The results will be stored in `output/micro_benchmarks/overlayfs/`.

```sh
$ ./script/micro_benchmarks_overlayfs.sh
```

**Second**, evaluate our system by three disk performance-related functions from [FunctionBench](https://github.com/ddps-lab/serverless-faas-workbench). Three funcions include the compression performance evaluation, the sequential read or write performance evaluation, and the random read or write performance evaluation.

Following below commands, get results of three functions by using EPHEMERA. 
The results will be stored in `output/funcionbench/ephemera/`.

```sh
$ ./script/functionbench_ephemera.sh
```

Following below commands, get results of three functions by using original OverlayFS. 
The results will be stored in `output/functionbench/overlayfs/`.

```sh
$ ./script/functionbench_overlayfs.sh
```

## Tenant Manager
The Tenant Manager optimize memory-sharing across function instances with heterogeneous resource requirements.

**First**, create CPU-intensive task Docker images and I/O-intensive task Docker images.
The CPU-intensive tasks are large integer factorial calculations and the I/O-intensive tasks are the micro-benchmarks' functions.

```sh
$ ./script/build_cpu_io_docker.sh
``` 

**Second**,  evaluate multi-containers running performance. The results and logs of each container instance will be stored in `./log`.

Following below commands, evaluate multi-containers running performance by EPHEMERA with memory-sharing mechanism.

```sh
$ ./script/start_tenant_manager.sh ./input/input_manager_mix_ephemera.txt
```

Following below commands, evaluate multi-containers running performance by EPHEMERA without memory-sharing mechanism.

```sh
$ ./script/start_tenant_manager.sh --disable_share ./input/input_manager_mix_ephemera.txt
```

Following below commands, evaluate multi-containers running performance by original system without memory-sharing mechanism.

```sh
$ ./script/start_tenant_manager.sh --disable_share ./input/input_manager_mix_overlayfs.txt
```

## Cluster Controller
The Cluster Controller dynamically adjusts the selection of nodes and resource allocation during function runtime.

For comparison, a workload-balancing algorithm that prioritizes nodes with the most available memory as the optimal choice serves as the baseline.
The trace employs six tasks arriving every second to simulate a real-world serverless computing request environment, observing the workload distribution on nodes and the response latency of each task.
The task set consists of 4 I/O-intensive tasks and 2 CPU-intensive tasks from the same tenant, distributed across two nodes, each with a total memory of 1024 MB.

Following below commands, evaluate scheduler in EPHEMERA. The results will be stored in `output/schedule/schedule_results.txt`.
```sh
$ ./script/start_scheduler.sh ./input/input_tenant_scheduler.txt
```

Following below commands, evaluate traditional scheduler. The results will be stored in `output/schedule/schedule_results.txt`.
```sh
$ ./script/start_scheduler.sh --tradition_schedule ./input/input_tenant_scheduler.txt
```

## Plot

Add data to `plot/background_plot.py`, and then run `python plot/background_plot.py` to get the figures about characteristics of AWS Lambda.

Add data to `plot/evaluation_plot.py`, and then run `python plot/evaluation_plot.py` to get the figures about evaluation results.