#! /bin/bash

memory_size=$1

aws lambda update-function-configuration --function-name gzip --memory-size "$memory_size"

# output="./out_${memory_size}.txt"

for i in {1..4}; do
    echo "Running test $i with memory size $memory_size"
    aws lambda invoke --function-name gzip "./out_${memory_size}_$i.txt" --payload '{"file_size": 32}'
    echo "Test $i completed"
done