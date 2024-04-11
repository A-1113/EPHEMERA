
# cpu intensive task using EPHEMERA
cp ./src/test_functions/big_factorial.c ./src/func/serverless_function.c
make
docker build -f ./docker/Dockerfile -t cpu_task_ephemera ./build
# cpu intensive task using original OverlayFS
cp ./src/test_functions/big_factorial.c ./src/func/serverless_function.c
CFLAGS="-D ONLY_IN_DISK" make single
docker build -f ./docker/Dockerfile -t cpu_task_overlayfs ./build

# io intensive task using EPHEMERA
cp ./src/test_functions/test_impact.c ./src/func/serverless_function.c
make
docker build -f ./docker/Dockerfile -t io_task_ephemera ./build
# io intensive task using original OverlayFS
cp ./src/test_functions/test_impact.c ./src/func/serverless_function.c
CFLAGS="-D ONLY_IN_DISK" make single
docker build -f ./docker/Dockerfile -t io_task_overlayfs ./build