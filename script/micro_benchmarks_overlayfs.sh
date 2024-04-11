output_dir="output/micro_bechmarks/baseline"
docker_image_name="ephemera-test:v1"
docker_name="ephemera-test"

# copy specific function
cp ./src/test_functions/test_impact.c ./src/func/serverless_function.c
# compile function with Runtime Daemon
CFLAGS="-D ONLY_IN_DISK" make single

invoke_functions() {
    echo memory_limit file_size operation_times
    for memory_limit in ${arrary_memory_limit[*]}; do
        for file_size in ${arrary_file_size[*]}; do
            for compute_times in ${arrary_compute_times[*]}; do
                echo ${memory_limit}MB ${file_size}MB $compute_times
                output_path="${output_sub_dir}/${memory_limit}MB_${file_size}MB_${compute_times}.txt"
                ./script/single_start_in_docker.sh $output_path $memory_limit $file_size $compute_times
            done
        done
    done
}

# 1. impact of memory limit:
arrary_memory_limit=(32 64 128 256)
arrary_file_size=(60)
arrary_compute_times=(500000)

output_sub_dir=$output_dir/impact_of_memory_limit
echo "impact of memory limit"
mkdir -p $output_sub_dir

invoke_functions

# 2. impact of file size:
arrary_memory_limit=(150)
arrary_file_size=(32 64 128 160)
arrary_compute_times=(500000)

output_sub_dir=$output_dir/impact_of_file_size
echo "impact of file size"
mkdir -p $output_sub_dir

invoke_functions

# 3. impact of operation times:
arrary_memory_limit=(300)
arrary_file_size=(128)
arrary_compute_times=(500 5000 50000 500000)

output_sub_dir=$output_dir/impact_of_operation_times
echo "impact of operation times"
mkdir -p $output_sub_dir

invoke_functions