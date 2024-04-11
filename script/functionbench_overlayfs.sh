output_dir="output/functionbench/ephemera"
docker_image_name="ephemera-test:v1"
docker_name="ephemera-test"

# gzip compress
cp ./src/test_functions/gzip_compression.c ./src/func/serverless_function.c
CFLAGS="-D ONLY_IN_DISK" make single

arrary_memory_limit=(64)
arrary_file_size=(1 2 3 4 5 16)
output_sub_dir=$output_dir/compress
mkdir -p $output_sub_dir

echo "compress"
echo memory_limit file_size
for memory_limit in ${arrary_memory_limit[*]}; do
    for file_size in ${arrary_file_size[*]}; do
        echo ${memory_limit}MB ${file_size}MB
        output_path="${output_sub_dir}/${memory_limit}MB_${file_size}MB.txt"
        ./script/single_start_in_docker.sh $output_path $memory_limit $file_size
    done
done

# sequential io
cp ./src/test_functions/sequential_disk_io.c ./src/func/serverless_function.c
CFLAGS="-D ONLY_IN_DISK" make single

arrary_memory_limit=(150)
arrary_file_size=(32)
arrary_compute_times=(500000)
output_sub_dir=$output_dir/sequential_io
mkdir -p $output_sub_dir

echo "sequential io"
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

# sequential io
cp ./src/test_functions/random_disk_io.c ./src/func/serverless_function.c
CFLAGS="-D ONLY_IN_DISK" make single

arrary_memory_limit=(150)
arrary_file_size=(32)
arrary_compute_times=(500000)
output_sub_dir=$output_dir/random_io
mkdir -p $output_sub_dir

echo "random io"
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