#! /bin/bash

memory_size=$1

aws lambda update-function-configuration --function-name pyaes --memory-size "$memory_size"

# output="./out_${memory_size}.txt"

for i in {1..4}; do
    echo "Running test $i with memory size $memory_size"
    aws lambda invoke --function-name pyaes "./out_${memory_size}_$i.txt" --payload '{"length_of_message": 1000, "num_of_iterations": 100}'
    echo "Test $i completed"
done