#! /bin/bash

memory_size=$1

aws lambda update-function-configuration --function-name linpack_1 --memory-size "$memory_size"

# output="./out_${memory_size}.txt"

for i in {1..4}; do
    echo "Running test $i with memory size $memory_size"
    aws lambda invoke --function-name linpack_1 "./out_${memory_size}_$i.txt" --payload '{"n": 2000}'
    echo "Test $i completed"
done