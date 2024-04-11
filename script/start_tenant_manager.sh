# args: [-h] [--disable_share] input_path

# clear previous containers and logs
docker rm -f cpu_task_ephemera_instance cpu_task_overlayfs_instance > /dev/null 2>&1
docker rm -f io_task_ephemera_instance io_task_overlayfs_instance > /dev/null 2>&1
rm -f ./log/*

# start Tenant Manager
python3 ./src/tenant_manager/tenant_manager.py $@