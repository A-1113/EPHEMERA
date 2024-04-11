import subprocess
import threading
import queue
import sys
import time
import docker
import os
import argparse

# -------------------配置参数-----------------------------
CONFIG_DISABLE_MEMORY_SHARE = False

# -------------------队列中的消息类型-----------------------------
# 特殊的消息用于通知读线程退出
class TerminateMessage:
    pass

# 用于初始化容器池的消息
class InitMessage:
    def __init__(self, container_name="", container_process="", content=""):
        self.container_name = container_name
        self.container_process = container_process
        self.content = content

# 用于标志某个函数结束的消息
class ResultMessage:
    def __init__(self, container_name):
        self.container_name = container_name

# 用于启动函数的消息
class InvokeMessage:
    def __init__(self, container_name, container_process, func_args):
        self.container_name = container_name
        self.container_process = container_process
        self.func_args = func_args

# ---------------------容器池--------------------------------
class PoolItem:
    def __init__(self,container_name, container_process, memory_limit, max_memory_usage, max_file_size):
        self.is_running = False # 函数实例是否在运行
        self.container_name = container_name # 容器名
        self.container_process = container_process # 运行的容器进程
        self.init_memory_limit = int(memory_limit) # 初始分配的内存限制
        self.now_memory_limit = int(memory_limit) # 现在的内存限制

        self.max_memory_usage = int(max_memory_usage*1.1) # 该函数使用的最大内存，预留10%
        self.max_file_size = int(max_file_size*1.1) # 该容器对文件访问的需求,预留10%

        self.target_container = {} # (AllocationItem)存储自己占用内存的容器 或 (HarvestItem)存储占用自己内存的容器
        self.target_container_memory = {} # (AllocationItem)存储自己占用容器的内存量 或 (HarvestItem)存储占用自己内存的容器所占用的内存量
    
    def get_spare_memory(self):
        return self.now_memory_limit - self.max_memory_usage - self.max_file_size

    def get_need_memroy(self):
        return self.max_memory_usage+self.max_file_size-self.now_memory_limit
    
    def display(self):
        print(f"container_name:{self.container_name}\n\
memory limit:{self.init_memory_limit}\n\
now memory limit:{self.now_memory_limit}")
        
# 用于标志某个函数内存告罄的消息
class SafeguardMessage:
    def __init__(self, container_name):
        self.container_name = container_name

# -----------------------存放image的映射字典---------------------
image2process = {}
image2name = {}
image2thread = {}

name2queue = {} # 队列头存储正在处理的消息，结果返回才会删除，后续为等待处理的消息

# ----------------------线程函数----------------------------------
# 线程函数，用于监控容器输出
def monitor_container_output_thread(container_name, container_process, queue_to_tenant_manager):
    cnt = 0
    # 如果容器还没有关闭就持续监听
    while container_process.poll() is None:
        response = container_process.stdout.readline().strip()
        cnt = cnt+1
        # 将消息发往Global Manager
        if cnt == 1:
            message = InitMessage(container_name, container_process, response)
            queue_to_tenant_manager.put(message)
        else:
            if(response != "stop success" and response != ""):
                message = ResultMessage(container_name)
                queue_to_tenant_manager.put(message)

        # 将返回结果写入文件
        with open("./log/"+container_name+'.log', 'a') as file:
            # 使用文件对象的 write 方法写入内容
            file.write(f"Received response: {response}\n")

# 线程函数，处理内存再分配的Global Manager
def tenant_manager_thread(queue):
    harvest_pool = {}
    allocation_pool = {}

    while True:
        # 读取队列中的消息，如果没有则阻塞
        message = queue.get()
        # 检查是否为终止消息
        if isinstance(message, TerminateMessage):
            break
        if isinstance(message, InvokeMessage): # InvokeMessage输入
            container_queue = name2queue[message.container_name]
            if container_queue.empty(): # 如果当前没有在请求在运行，直接执行
                container_queue.put(message)
                invoke_from_pool(message.container_name, message.container_process, message.func_args, harvest_pool, allocation_pool)
            else: # 如果当前有请求在运行，先入队，等待结果返回再执行
                container_queue.put(message)
        
        elif isinstance(message, ResultMessage): # ResultMessage返回
            container_queue = name2queue[message.container_name]
            container_queue.get() # 弹出占位的InvokeMessage
            if container_queue.empty(): # 如果后续没有请求，需要将内存归还或收回
                retrieve_in_pool(message.container_name, harvest_pool, allocation_pool)
            else: # 如果后续有请求，继续执行，不重新分配内存
                message = container_queue.queue[0] # 获取队首的下一条请求
                invoke_from_pool(message.container_name, message.container_process, message.func_args, harvest_pool, allocation_pool)
        
        elif isinstance(message, InitMessage): # InitMessage返回
            container_queue = name2queue[message.container_name]
            container_queue.get() # 取出队列中占位的InitMessage
            add_to_pool(message, harvest_pool, allocation_pool)
            if not container_queue.empty(): # 如果后续有请求，继续执行
                message = container_queue.queue[0] # 获取队首的下一条请求
                invoke_from_pool(message.container_name, message.container_process, message.func_args, harvest_pool, allocation_pool)

        elif isinstance(message, SafeguardMessage): # 触发Safeguard，需要立刻将内存重新分配
            retrieve_in_pool(message.container_name, harvest_pool, allocation_pool)

# -------------------容器池相关----------------------------
def add_to_pool(message:InitMessage, harvest_pool, allocation_pool):
    container_name = message.container_name
    container_process = message.container_process
    contents = message.content.split()
    memory_limit = int(contents[0])
    max_memory_usage = int(contents[1])
    max_file_size = int(contents[2])

    pool_item = PoolItem(container_name, container_process, memory_limit, max_memory_usage, max_file_size)
    # 将每个容器看做自给自足容器即可屏蔽内存共享机制
    if CONFIG_DISABLE_MEMORY_SHARE:
        return
    if max_memory_usage + max_file_size < memory_limit*0.9: # 自身内存够用，放入harvest pool
        harvest_pool[container_name] = pool_item
    elif max_memory_usage + max_file_size > memory_limit: # 自身内存不够用，放入allocation pool
        allocation_pool[container_name] = pool_item
    else: # 自身内存刚好够自己用，自给自足容器
        pass

def print_pools(harvest_pool, allocation_pool):
    print("-----------------------------------------")
    print("harvest pool:")
    for pool_item in harvest_pool.values():
        pool_item.display()
    print("allocation pool:")
    for pool_item in allocation_pool.values():
        pool_item.display()
    print("-----------------------------------------")

# 将memory_amount的内存分配从harvest_pool_item转移到allocation_pool_item
def memory_re_allocation(harvest_pool_item:PoolItem, allocation_pool_item:PoolItem, memory_amount:int):
    harvest_pool_item.now_memory_limit -= memory_amount
    if allocation_pool_item.container_name in harvest_pool_item.target_container: # 如果已经在目标容器列表中，仅需更新重分配的内存量
        harvest_pool_item.target_container_memory[allocation_pool_item.container_name] += memory_amount
    else:    
        harvest_pool_item.target_container[allocation_pool_item.container_name] = allocation_pool_item
        harvest_pool_item.target_container_memory[allocation_pool_item.container_name] = memory_amount
    
    allocation_pool_item.now_memory_limit += memory_amount
    if harvest_pool_item.container_name in allocation_pool_item.target_container: # 如果已经在目标容器列表中，仅需更新重分配的内存量
        allocation_pool_item.target_container_memory[harvest_pool_item.container_name] += memory_amount
    else:    
        allocation_pool_item.target_container[harvest_pool_item.container_name] = harvest_pool_item
        allocation_pool_item.target_container_memory[harvest_pool_item.container_name] = memory_amount

    # 更新内存上限
    update_container_in_pool(container_name=harvest_pool_item.container_name, 
                             container_process=harvest_pool_item.container_process, 
                             target_memory=harvest_pool_item.now_memory_limit)
    update_container_in_pool(container_name=allocation_pool_item.container_name, 
                             container_process=allocation_pool_item.container_process, 
                             target_memory=allocation_pool_item.now_memory_limit)

# 将container_name对应容器先充分配内存后按照参数启动
def invoke_from_pool(container_name, container_process, func_args, harvest_pool, allocation_pool):
    if container_name in harvest_pool:
        # 在收割池中，将内存分配给需要的容器
        harvest_pool_item:PoolItem = harvest_pool[container_name]
        harvest_pool_item.is_running = True
        for key in allocation_pool:
            allocation_pool_item:PoolItem = allocation_pool[key]
            if allocation_pool_item.is_running == False:
                continue
            need_memory = allocation_pool_item.get_need_memroy()
            spare_memory = harvest_pool_item.get_spare_memory()

            if need_memory>0 and need_memory<=spare_memory: # 可以分配内存
                memory_re_allocation(harvest_pool_item=harvest_pool_item,
                                     allocation_pool_item=allocation_pool_item,
                                     memory_amount=need_memory)

    elif container_name in allocation_pool:
        # 在分配池中，需要从收割池中获取一定的内存
        allocation_pool_item:PoolItem = allocation_pool[container_name]
        allocation_pool_item.is_running = True
        need_memory = allocation_pool_item.get_need_memroy()
        for key in harvest_pool:
            if need_memory <= 0:
                break
            
            harvest_pool_item:PoolItem = harvest_pool[key]

            if harvest_pool_item.is_running == False:
                continue
            spare_memory = harvest_pool_item.get_spare_memory()
            
            if spare_memory > 0:
                memory_re_allocation(harvest_pool_item=harvest_pool_item,
                                     allocation_pool_item=allocation_pool_item,
                                     memory_amount=min(spare_memory,need_memory))
                need_memory -= min(spare_memory,need_memory)

    else:
        # 自己自足容器直接启动
        pass
    communicate_with_container(container_process, func_args)

# 将pool_item恢复初始状态，并重置容器的内存限制
def reset_pool_item(pool_item: PoolItem):
    pool_item.now_memory_limit = pool_item.init_memory_limit
    update_container_in_pool(container_name=pool_item.container_name, 
                             container_process=pool_item.container_process, 
                             target_memory=pool_item.now_memory_limit)
    pool_item.is_running = False
    pool_item.target_container.clear()
    pool_item.target_container_memory.clear()

# 将pool_item重分配的内存还原，flag==1表示自身在allocation_pool，flag==-1表示自身在harvest_pool
def retrieve_pool_item(pool_item:PoolItem, flag:int):
    for target_container_name in pool_item.target_container:
        target_pool_item:PoolItem = pool_item.target_container[target_container_name]
        memory_amount = pool_item.target_container_memory[target_container_name]

        target_pool_item.now_memory_limit += flag*memory_amount
        # 更新目标容器的内存上限
        update_container_in_pool(container_name=target_pool_item.container_name, 
                                 container_process=target_pool_item.container_process, 
                                 target_memory=target_pool_item.now_memory_limit)
        # 清除被分配的内存的容器中收割的信息
        target_pool_item.target_container.pop(pool_item.container_name)
        target_pool_item.target_container_memory.pop(pool_item.container_name)
    
    # 标记为原始状态并清空
    reset_pool_item(pool_item)

# 将container_name对应的容器重分配的内存还原
def retrieve_in_pool(container_name, harvest_pool, allocation_pool):
    if container_name in harvest_pool:
        # 在收割池中，需要取回内存
        harvest_pool_item:PoolItem = harvest_pool[container_name]
        retrieve_pool_item(harvest_pool_item,-1)
    elif container_name in allocation_pool:
        # 在分配池中，需要将内存归还
        allocation_pool_item:PoolItem = allocation_pool[container_name]
        retrieve_pool_item(allocation_pool_item,1)
    else:
        # 自己自足容器不参与内存重分配
        pass

# 将内存上限的更新信息发往容器，被动
def update_container_in_pool(container_name, container_process, target_memory):
    # 必须加上--memory-swap，不然出bug
    command = f'docker update --memory {target_memory}m --memory-swap {target_memory}m {container_name} > /dev/null 2>&1'
    subprocess.run(command, shell=True)

    message_to_send = "update " + str(target_memory)
    communicate_with_container(container_process, message_to_send)

#--------------------容器操作相关---------------------------
# 根据image_name启动容器，并命名为container_name
docker_count = 0 # 按照当前是第几个容器来按照核心分配
core_count = os.cpu_count() # 获取cpu核心数

def start_docker_container(image_name, container_name, memory_limit):
    global docker_count
    global core_count
    c_program_path = "./build/proxy"  # proxy在docker中的路径
    # 构建Docker命令
    docker_command = [
        "docker", "run",  "-i",
        "--name", container_name,
        "--memory", memory_limit,
        "--memory-swap", memory_limit,
        "--cpuset-cpus", str(docker_count % core_count),
        image_name,
        c_program_path
    ]
    docker_count += 1
    # 启动Docker容器
    process = subprocess.Popen(docker_command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

    return process

# 将message发送到process对应的容器中
def communicate_with_container(process, message):
    # 如果容器还未停止，向容器发送消息
    if process.poll() is None:
        process.stdin.write(message + "\n")
        process.stdin.flush()

# 根据用户输入的image_name来创造容器
def create_container(user_input, queue_to_tenant_manager):
    argv = user_input.split()
    argc = len(user_input)
    if(argc < 3):
        print(f'ERROR INPUT')
        return
    
    docker_image = argv[1] # 所使用的docker镜像名称
    container_name = argv[1]+"_instance" # 所创建的docker名称
    memory_limit = argv[2]+"m" # 创建时分配的内存,单位为MB
    container_process = start_docker_container(docker_image, container_name, memory_limit)

    image2process[docker_image] =  container_process
    image2name[docker_image] =  container_name

    name2queue[container_name] = queue.Queue() # 创建用于存取消息的队列
    name2queue[container_name].put(InitMessage()) # 表示现在正在处理的消息是初始化的消息

    # 启动线程监听返回值
    thread = threading.Thread(target=monitor_container_output_thread, args=(container_name, container_process, queue_to_tenant_manager))
    thread.start()
    image2thread[docker_image] = thread

    # 第一次启动，获得函数信息
    message_to_send = ' '.join(argv[:1]+argv[2:])
    communicate_with_container(container_process, message_to_send)

# 将request发往容器
def invoke_container(user_input, queue_to_tenant_manager):
    argv = user_input.split()
    if argv[1] not in image2process:
        print(f"ERROR: {argv[1]} is not running")
        return

    container_name = image2name[argv[1]]
    container_process = image2process[argv[1]]
    func_args = ' '.join(argv[:1]+argv[2:])

    message = InvokeMessage(container_name, container_process, func_args)
    queue_to_tenant_manager.put(message)

# 将内存上限的更新信息发往容器，手动
def update_container(user_input):
    argv = user_input.split()
    if argv[1] not in image2process:
        print(f"ERROR: {argv[1]} is not running")
        return
    container_process = image2process[argv[1]]
    message_to_send = ' '.join(argv[:1]+argv[2:])

    communicate_with_container(container_process, message_to_send)

#--------------------------结束相关---------------------
# 将停止信息发往容器，手动
def stop_container(user_input):
    argv = user_input.split()
    if argv[1] not in image2process:
        print(f"ERROR: {argv[1]} is not running")
        return
    container_process = image2process[argv[1]]
    message_to_send = ' '.join(argv[:1])

    communicate_with_container(container_process, message_to_send)

# 停止并移除名字为container_name的容器
def stop_and_remove_docker_container(container_name):
    # 停止Docker容器
    stop_command = f"docker stop {container_name}"
    subprocess.run(stop_command, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) # 将输出重定向到 /dev/null

    # 删除Docker容器
    remove_command = f"docker rm {container_name}"
    subprocess.run(remove_command, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

# 判断所有消息队列都为空
def is_all_queue_empty(queue):
    if not queue.empty():
        return False
    for container_queue in name2queue.values():
        if not container_queue.empty():
            return False
    return True
    
# 终止所有容器
def stop_containers(queue):
    # 等待之前的指令全部结束
    while True:
        if is_all_queue_empty(queue):
            break
        time.sleep(1)
        
    # 向还在运行的Docker发送stop消息，让Docker自主停止
    for container_process in image2process.values():
        if container_process.poll() is None:
            communicate_with_container(container_process, "stop")
    # 等待Docker结束
    for container_process in image2process.values():
        container_process.wait()
    # 清除Docker
    for container_name in image2name.values():
        stop_and_remove_docker_container(container_name)
    
    image2name.clear()
    image2process.clear()
    name2queue.clear()

# 等待线程结束
def join_threads():
    for thread in image2thread.values():
        thread.join()
    
    image2thread.clear()

# 提示信息
def print_input_format():
    print("input format:\n\
        1. create IMAGE MEMORY_LIMIT ARGS\n\
        2. invoke IMAGE ARGS\n\
        3. update IMAGE MEMORY_LIMIT\n\
        4. stop IMAGE\n\
        5. exit")

# 通过docker_image来启动对应的含有函数的容器
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Modify boolean and string variables")
    parser.add_argument("--disable_share", action="store_true", help="Disable Memory-Sharing Mechanism")
    parser.add_argument("input_path", type=str, help="Input Commands Path")
    args = parser.parse_args()
    
    # 根据输入参数，确定是否关闭内存共享机制
    CONFIG_DISABLE_MEMORY_SHARE = args.disable_share

    # 创建队列，默认最大容量无限
    message_queue = queue.Queue()
    tenant_manager_instance = threading.Thread(target=tenant_manager_thread, args=(message_queue,))
    tenant_manager_instance.start()

    sys.stdin = open(args.input_path,"r")
    # sys.stdin = open("./input/input_manager_mix_memory.txt","r")
    # sys.stdin = open("./input/input_manager_mix_disk.txt","r")

    while True:
        # 通过input()获取用户输入
        user_input = input("> ").strip()
        print(user_input)
        if user_input.lower().startswith("create "):
            # create IMAGE MEMORY_LIMIT ARGS
            create_container(user_input, message_queue)
        elif user_input.lower().startswith("invoke "):
            # invoke IMAGE ARGS
            invoke_container(user_input, message_queue)
        elif user_input.lower().startswith("update "):
            # update IMAGE MEMORY_LIMIT
            update_container(user_input)
        elif user_input.lower().startswith("stop "):
            # stop IMAGE
            stop_container(user_input)
        elif user_input.lower().startswith("exit"):
            break
        elif user_input.lower().startswith("sleep"):
            time.sleep(1)
        else:
            print(f"ERROR INPUT")
            print_input_format()
    
    stop_containers(message_queue)
    join_threads()

    # 终止tenant manager
    message_queue.put(TerminateMessage())
    tenant_manager_instance.join()
