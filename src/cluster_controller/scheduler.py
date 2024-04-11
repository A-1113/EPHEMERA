import argparse

# 只将任务分配到剩余内存最多的节点
CONFIG_TRADITIONAL_SCHEDULER = False 

class Node:
    def __init__(self, node_memory_limit):
        self.node_memory_limit = node_memory_limit
        self.tasks = []
        self.used_memory = 0
        self.real_memory_usage = 0

    def can_allocate(self, task):
        """检查节点是否能够分配新任务"""
        return self.used_memory + task['memory_limit'] <= self.node_memory_limit

    def update_allocation(self, task):
        """更新节点的任务分配状态"""
        self.tasks.append(task)
        self.used_memory += task['memory_limit']
        self.real_memory_usage += task['max_memory_usage'] + task['max_file_size']

    def remaining_memory(self):
        """计算节点剩余的内存配置空间"""
        return self.node_memory_limit - self.used_memory

    def surplus_memory(self):
        """计算节点剩余的实际内存使用空间"""
        return self.used_memory - self.real_memory_usage

def print_node_info(nodes):
    """打印当前每个节点的任务信息"""
    for i, node in enumerate(nodes):
        print(f"Node {i + 1} current task information:")
        for task in node.tasks:
            print(f"  Task name: {task['name']}, Memory allocation: {task['memory_limit']}, Actual usage: {task['max_memory_usage']}, File usage: {task['max_file_size']}")
        print(f"Total configured memory of the node: {node.used_memory}, Actual memory usage of the node: {node.real_memory_usage}")

def find_optimal_node(task, nodes):
    """找到最优的节点进行任务分配"""
    optimal_node = None
    best_fit_metric = float('-inf')
    for node in nodes:
        if node.can_allocate(task):
            surplus_memory = node.surplus_memory()
            remain_node_memory = node.node_memory_limit - (node.used_memory + task['memory_limit'])
            if task['max_memory_usage'] + task['max_file_size'] > task['memory_limit']:
                # 任务内存超额，选择内存配置有剩余的节点，或内存配置超额较少的节点
                metric = surplus_memory if surplus_memory > 0 else surplus_memory - 100000  # 为负数时，给予大的惩罚
            elif task['max_memory_usage'] + task['max_file_size'] < task['memory_limit']:
                # 任务内存不足，选择内存配置超额的节点，或内存配置剩余较少的节点
                metric = -surplus_memory if surplus_memory < 0 else -surplus_memory - 100000  # 为正数时，给予大的惩罚
            else:
                # 任务内存刚好，选择节点内存使用量剩余更多的
                metric = 0
            
            # 如果开启传统调度方式，只考虑节点剩余的内存量
            if CONFIG_TRADITIONAL_SCHEDULER is True:
                metric = remain_node_memory
            
            if metric > best_fit_metric or (best_fit_metric == metric and remain_node_memory > metric_remain_node_memory):
                best_fit_metric = metric
                metric_remain_node_memory = remain_node_memory
                optimal_node = node

    return optimal_node

def dynamic_schedule(task, nodes):
    """动态调度新任务到最优节点"""
    optimal_node = find_optimal_node(task, nodes)
    if optimal_node:
        optimal_node.update_allocation(task)
        # print(f"Task {task['name']} has been allocated to a node, current node memory usage {optimal_node.used_memory} / {optimal_node.node_memory_limit}")
    else:
        print("Failed to find a suitable node for the new task.")
    
    # print_node_info(nodes)
    # print()

# 初始化节点
node_cnt = 2
node_memory_limit = 1024
nodes = [Node(node_memory_limit) for _ in range(node_cnt)]

def read_command_line():
    # 从命令行读取任务参数并调度
    while True:
        input_line = input("Enter task parameters (name memory_limit max_memory_usage max_file_size), or type 'exit' to quit:")
        if input_line == 'exit':
            break
        name, memory_limit, max_memory_usage, max_file_size = input_line.split()
        task = {
            'name': name,
            'memory_limit': int(memory_limit),
            'max_memory_usage': int(max_memory_usage),
            'max_file_size': int(max_file_size),
        }
        dynamic_schedule(task, nodes)


def read_input_from_file(file_path):
    """从文件中读取任务参数"""
    with open(file_path, 'r') as file:
        tasks = []
        for line in file:
            name, memory_limit, max_memory_usage, max_file_size = line.split()
            tasks.append({
                'name': name,
                'memory_limit': int(memory_limit),
                'max_memory_usage': int(max_memory_usage),
                'max_file_size': int(max_file_size),
            })
    return tasks

def main(file_path):
    tasks = read_input_from_file(file_path)
    for task in tasks:
        dynamic_schedule(task, nodes)
    print_node_info(nodes)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Modify boolean and string variables")
    parser.add_argument("--tradition_schedule", action="store_true", help="Use Traditional Scheduling")
    parser.add_argument("input_path", type=str, help="Input Commands Path")
    args = parser.parse_args()
    
    # 根据输入参数，确定是否关闭内存共享机制
    CONFIG_TRADITIONAL_SCHEDULER = args.tradition_schedule
    file_path = "./input/input_tenant_scheduler.txt"
    main(args.input_path)
    