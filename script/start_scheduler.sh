# args: [-h] [--tradition_schedule] input_path

mkdir -p output/schedule
python3 ./src/cluster_controller/scheduler.py $@ > output/schedule/schedule_results.txt