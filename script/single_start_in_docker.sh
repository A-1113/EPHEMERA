# $1: output path
# $2: Docker memory limit
# $3...: function parameters

# parse input parameters
output_path=$1
memory_limit=$2
shift 2
args=$@

docker_image_name="ephemera-test:v1"
docker_name="ephemera-test"

# remove Docker
docker rm -f $docker_name > /dev/null 2>&1
# build Docker image 
docker build -t $docker_image_name ./docker_single > /dev/null 2>&1
# execute funtion with specific parameters
docker run --name $docker_name -it --cpuset-cpus=3 -m "$memory_limit"m -v $(pwd)/:/code $docker_image_name /bin/bash -c "./build/launcher_single $memory_limit $args" > $output_path