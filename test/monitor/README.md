# 单元测试

## 测试框架

1. 选择了catch2

2. 使用cmake的fetch content引入，故项目代码和环境中可以不存在测试框架

3. 使用单体可执行文件

## 测试目录结构

所有monitor的单元测试代码都放在`test/monitor/`

编译后得到`<build_dir>/test/test_monitor`

```sh
test/
├── CMakeLists.txt
├── monitor
│   ├── CMakeLists.txt
│   ├── README.md
│   ├── event_controller
│   │   └── single_event_controller_test.cpp
│   ├── general_monitor_test.cpp
│   ├── mocks
│   │   ├── mock_event_controller.cpp
│   │   ├── mock_event_controller.hpp
│   │   ├── perf_mocks.cpp
│   │   └── perf_mocks.hpp
│   ├── monitor_util_test.cpp
│   └── perf_event_attr_test.cpp
├── test_counter_detector.cpp
└── test_set.cpp
```

## 测试方法

1. 构建

```sh
[tuchunxu@tcx-honor ~/WorkSpace/hperf-cpp_test]$ cmake --build build/ -t test_monitor
[0/2] Re-checking globbed directories...
[1/2] Re-running CMake...
-- Configuring for CPU type: Neoverse-N1
-- Configuring done (0.5s)
-- Generating done (0.0s)
-- Build files have been written to: /home/tuchunxu/WorkSpace/hperf-cpp_test/build
[0/4] Re-checking globbed directories...
[4/7] Linking CXX executable test/monitor/test_monitor
```

2. 测试

测试命令`build/test/test_monitor`

实际结果（我cross compile然后放服务器跑的）

```sh
cxtu@ampere1:~/test_hperf_mem_bw/refactor2_tmp_test$ ./test_monitor
Randomness seeded to: 2774618513
===============================================================================
All tests passed (169 assertions in 18 test cases)
```

# 集成测试

## 测试脚本

以下为指定程序运行完后自动发SIGINT的脚本，指定运行时间的就不用脚本了

```sh
#!/usr/bin/env sh

# example
# test_hperf.sh arm_cmn_mem_bw_down 1000 ./mc_pos.txt ./result.log -- ./stream

MONITOR_TARGET=$1
INTERVAL=$2
MC_POS_PATH=$3
OUTPUT_PATH=$4

./hperf --monitor ${MONITOR_TARGET} -i ${INTERVAL} --cmn-mc-pos ${MC_POS_PATH} -o ${OUTPUT_PATH} &
PID=$!

while [ "$1" != "--" ] && [ $# -gt 0 ]; do
    shift
done
shift
$@
kill -INT $
```

## 测试结果

### 使用信号

1. all（up和down）

cmd

```sh
cxtu@ampere1:~/test_hperf_mem_bw/refactor2_tmp_test$ ./test_hperf.sh arm_cmn_mem_bw_all 1000 ../mc_pos.txt ./result_all.log -- ../cmp_hperf_perf_>
-------------------------------------------------------------
STREAM version $Revision: 5.10 $
-------------------------------------------------------------
This system uses 8 bytes per array element.
-------------------------------------------------------------
Array size = 10000000 (elements), Offset = 0 (elements)
Memory per array = 76.3 MiB (= 0.1 GiB).
Total memory required = 228.9 MiB (= 0.2 GiB).
Each kernel will be executed 1000 times.
 The *best* time for each kernel (excluding the first iteration)
 will be used to compute the reported bandwidth.
-------------------------------------------------------------
Number of Threads requested = 160
Number of Threads counted = 160
-------------------------------------------------------------
Your clock granularity/precision appears to be 1 microseconds.
Each test below will take on the order of 1468 microseconds.
   (= 1468 clock ticks)
Increase the size of the arrays if this shows that
you are not getting at least 20 clock ticks per test.
-------------------------------------------------------------
WARNING -- The above is only a rough guideline.
For best results, please be sure you know the
precision of your system timer.
-------------------------------------------------------------
Function    Best Rate MB/s  Avg time     Min time     Max time
Copy:          121200.8     0.002297     0.001320     0.017365
Scale:         153251.6     0.002299     0.001044     0.017955
Add:           145278.2     0.003166     0.001652     0.020981
Triad:         133258.3     0.003251     0.001801     0.095951
-------------------------------------------------------------
Solution Validates: avg error less than 1.000000e-13 on all three arrays
-------------------------------------------------------------
```

result

```sh
cxtu@ampere1:~/test_hperf_mem_bw/refactor2_tmp_test$ cat ./result_all.log 
1774680029.235,24592762912,20093630592
1774680030.235,25055338720,20370682560
1774680031.236,24620592672,19900256704
1774680032.236,26909522816,22602129984
1774680033.235,26608297536,22451156544
1774680034.235,27159989120,22659480192
1774680035.236,27436948608,22613134464
1774680036.235,26567077600,22704609408
1774680037.236,27491102400,22588010528
1774680038.237,27664172928,23250029472
1774680039.235,26776651200,23129709408
```

2. up

cmd

```sh
cxtu@ampere1:~/test_hperf_mem_bw/refactor2_tmp_test$ ./test_hperf.sh arm_cmn_mem_bw_up 1000 ../mc_pos.txt ./result_up.log -- ../cmp_hperf_perf_wp>
-------------------------------------------------------------
STREAM version $Revision: 5.10 $
-------------------------------------------------------------
This system uses 8 bytes per array element.
-------------------------------------------------------------
Array size = 10000000 (elements), Offset = 0 (elements)
Memory per array = 76.3 MiB (= 0.1 GiB).
Total memory required = 228.9 MiB (= 0.2 GiB).
Each kernel will be executed 1000 times.
 The *best* time for each kernel (excluding the first iteration)
 will be used to compute the reported bandwidth.
-------------------------------------------------------------
Number of Threads requested = 160
Number of Threads counted = 160
-------------------------------------------------------------
Your clock granularity/precision appears to be 1 microseconds.
Each test below will take on the order of 1987 microseconds.
   (= 1987 clock ticks)
Increase the size of the arrays if this shows that
you are not getting at least 20 clock ticks per test.
-------------------------------------------------------------
WARNING -- The above is only a rough guideline.
For best results, please be sure you know the
precision of your system timer.
-------------------------------------------------------------
Function    Best Rate MB/s  Avg time     Min time     Max time
Copy:          134352.1     0.002222     0.001191     0.016042
Scale:         151795.7     0.002315     0.001054     0.074768
Add:           143969.2     0.003105     0.001667     0.040216
Triad:         131003.8     0.003090     0.001832     0.018801
-------------------------------------------------------------
Solution Validates: avg error less than 1.000000e-13 on all three arrays
-------------------------------------------------------------
```

result

```sh
cxtu@ampere1:~/test_hperf_mem_bw/refactor2_tmp_test$ cat ./result_up.log  
1774680068.681,25565609376,none
1774680069.681,26725252672,none
1774680070.681,23857830080,none
1774680071.681,27133350752,none
1774680072.681,27054104960,none
1774680073.681,27409126432,none
1774680074.681,27409448064,none
1774680075.681,27418493568,none
1774680076.681,27625233568,none
1774680077.681,28215830560,none
```

3. down

cmd

```sh
cxtu@ampere1:~/test_hperf_mem_bw/refactor2_tmp_test$ ./test_hperf.sh arm_cmn_mem_bw_down 1000 ../mc_pos.txt ./result_down.log -- ../cmp_hperf_per>
-------------------------------------------------------------
STREAM version $Revision: 5.10 $
-------------------------------------------------------------
This system uses 8 bytes per array element.
-------------------------------------------------------------
Array size = 10000000 (elements), Offset = 0 (elements)
Memory per array = 76.3 MiB (= 0.1 GiB).
Total memory required = 228.9 MiB (= 0.2 GiB).
Each kernel will be executed 1000 times.
 The *best* time for each kernel (excluding the first iteration)
 will be used to compute the reported bandwidth.
-------------------------------------------------------------
Number of Threads requested = 160
Number of Threads counted = 160
-------------------------------------------------------------
Your clock granularity/precision appears to be 1 microseconds.
Each test below will take on the order of 1804 microseconds.
   (= 1804 clock ticks)
Increase the size of the arrays if this shows that
you are not getting at least 20 clock ticks per test.
-------------------------------------------------------------
WARNING -- The above is only a rough guideline.
For best results, please be sure you know the
precision of your system timer.
-------------------------------------------------------------
Function    Best Rate MB/s  Avg time     Min time     Max time
Copy:          134244.6     0.002254     0.001192     0.015315
Scale:         132130.1     0.002268     0.001211     0.018154
Add:           131500.1     0.003123     0.001825     0.020723
Triad:         137218.2     0.003187     0.001749     0.107377
-------------------------------------------------------------
Solution Validates: avg error less than 1.000000e-13 on all three arrays
-------------------------------------------------------------
```

result

```sh
cxtu@ampere1:~/test_hperf_mem_bw/refactor2_tmp_test$ cat ./result_down.log 
1774680102.697,none,21343845888
1774680103.697,none,22283701216
1774680104.697,none,20186294464
1774680105.697,none,22938025504
1774680106.697,none,22733745792
1774680107.697,none,23025987200
1774680108.697,none,22163871488
1774680109.697,none,22524141664
1774680110.697,none,23240105952
1774680111.697,none,22156587808
1774680112.697,none,21158873248
```

### 定时

1. all（up和down）

```sh
cxtu@ampere1:~/test_hperf_mem_bw/refactor2_tmp_test$ ./hperf --monitor arm_cmn_mem_bw_all -i 1000 -d 5 --cmn-mc-pos ../mc_pos.txt 
1774681053.211,16047904,18543776
1774681054.211,31318080,18707360
1774681055.211,26697472,17535104
1774681056.211,27252672,16738496
1774681057.211,18844224,18175808
```

2. up

```sh
cxtu@ampere1:~/test_hperf_mem_bw/refactor2_tmp_test$ ./hperf --monitor arm_cmn_mem_bw_up -i 1000 -d 3 --cmn-mc-pos ../mc_pos.txt  
1774681464.500,35796384,none
1774681465.500,32298880,none
1774681466.500,30254464,none
```

3. down

```sh
cxtu@ampere1:~/test_hperf_mem_bw/refactor2_tmp_test$ ./hperf --monitor arm_cmn_mem_bw_down -i 1000 -d 6 --cmn-mc-pos ../mc_pos.txt 
1774681476.854,none,18880704
1774681477.854,none,18873632
1774681478.854,none,17424160
1774681479.854,none,17975168
1774681480.854,none,17853472
1774681481.854,none,17966464
```
