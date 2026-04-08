# hperf

硬件性能分析工具套件，支持 Android 移动端和 Linux 服务器两个平台。

核心功能：
- **CPU PMU 性能分析**（hperf）：通过复用计数器，在一次测量中获得所有需要的微架构指标；通过事件分组大幅减少系统调用开销
- **Arm CMN 内存带宽监测**（hperf `--monitor`，仅 Linux）：通过 CMN 片上网络 watchpoint 事件监测内存控制器上下行流量
- **GPU 性能分析**（gperf，仅 Android）：基于 Arm hwcpipe 实时监测 Mali GPU 性能指标

## 编译与安装

项目使用 CMake 构建，提供两种构建预设：

### Android 构建（交叉编译）

工具链由 NDK 提供。

> 开发使用的 NDK 版本：29.0.13599879

构建前，确保环境变量 `NDK_HOME` 指向 NDK 的根目录。

```bash
# 配置
cmake --preset android-arm64

# 编译
cmake --build --preset android-arm64

# 部署到设备（按 CPU 型号选择 target）
cmake --build --preset android-arm64 --target deploy-oryon       # 高通 Oryon 核心
cmake --build --preset android-arm64 --target deploy-cortex_x4   # Arm 公版 Cortex-X4
cmake --build --preset android-arm64 --target deploy-c1_ultra    # C1-Ultra
cmake --build --preset android-arm64 --target deploy-ustress     # Ustress 评估配置
```

deploy target 会将可执行程序和对应的 TOML 配置文件推送到设备的 `/data/local/tmp/` 目录下。

### Linux 构建（原生编译）

使用系统编译器（g++ 或 clang++），无需 NDK。

```bash
cmake --preset linux
cmake --build --preset linux
```

Linux 构建包含 hperf + CMN 带宽监测模块，不包含 gperf。

## 运行

### CPU 工具 hperf

使用 `-h` 选项列出使用说明与测量的性能事件。

#### 模式1: 全局测量

全量采集，会收集每个 CPU 的 PMU 数据，开销较大：

```
$ adb root
$ adb shell
[以 root 身份进入 adb shell]
# cd /data/local/tmp
# ./hperf -a -d 10 -i 1000 -o system.csv
```

`-a` 指定全局测量，`-d 10` 采集时间 10s，`-i 1000` 复用间隔 1000ms，`-o system.csv` 原始数据以 CSV 格式输出到 system.csv 文件中。

> 若不加 `-o` 选项，原始数据将直接输出到控制台。

原始数据格式：`timestamp,cpu,group,event,value` 时间戳，CPU ID，事件组序号，事件名称，在此间隔内的事件计数值。

#### 模式2: 跟踪进程

指定进程号，仅收集该进程的 PMU 数据：

```
$ adb root
$ adb shell
[以 root 身份进入 adb shell]
# cd /data/local/tmp
# ./hperf -p <pid> -d 3 -i 500 -o process.csv
```

`-p <pid>` 指定跟踪进程测量，进程号是 `<pid>`，`-d 3` 采集时间 3s（采集结束后不影响进程继续运行），`-i 500` 复用间隔 500ms，`-o process.csv` 原始数据以 CSV 格式输出到 process.csv 文件中。

或者附带一个命令行程序，收集该进程的 PMU 数据。

```
# ./hperf -i 500 ./ustress/div64_workload 10
```

原始数据格式：`timestamp,cpu,group,event,value` 时间戳，CPU ID，事件组序号，事件名称，在此间隔内的事件计数值。其中 CPU ID 固定为 -1。

> 已知问题：若附带的命令行程序会创建子进程，那么子进程产生的微架构事件不计——部分测试场景会存在问题（例如跟踪一个 shell 脚本，shell 脚本中的命令通常都是子进程）——这个问题可以通过设置事件配置的 inherit 比特位让子进程继承，但 inherit 位和事件组的统一控制是存在冲突的，如果使用 inherit 则每次事件组的调度需要产生大量系统调用，进而产生大量开销。
> 
> 临时解决方案：先启动命令行程序，使之后台运行，获得其 PID 后调用 hperf 跟踪，例如：
> 
> 不使用
> `$ ./hperf ./script.sh`
> 
> 而是
> `./script.sh & ./hperf -p $!`
>
> 先启动脚本，使用 `&` 让其在后台运行，接着使用 `$!` 环境变量获得其 PID，令其作为命令行选项参数传给 hperf 进行跟踪测量。

当跟踪的进程运行结束后（无论是指定进程号还是指定命令行），或者达到 `-d` 选项指定的测量时间后，hperf 即停止采集数据。

若通过 `-d` 指定采集时间，到达指定时间之后停止采集数据，此时不影响进程继续运行。

> 假设这个程序需要运行 5s，但是命令行指定采集 3s，那么还是只采集 3s 的数据。

#### 探测可用性能计数器数量与自适应分组

可以使用 `--detect-counters` 选项探测当前平台每个 CPU 上可用硬件性能计数器的数量。

> simpleperf 也有类似的功能（`simpleperf stat --print-hw-counter`），但是在 MTK 平台通常会失效（探测结果是 0，不符合实际情况），hperf 重新实现了探测逻辑。

例如在高通的某台机器上，探测结果如下：

```
# ./hperf --detect-counters
Detecting available programmable counters on each CPU ...
11 available programmable counters on CPU 0
11 available programmable counters on CPU 1
11 available programmable counters on CPU 2
11 available programmable counters on CPU 3
11 available programmable counters on CPU 4
11 available programmable counters on CPU 5
11 available programmable counters on CPU 6
11 available programmable counters on CPU 7
```

完成探测后，可用性能计数器的数量可以用于优化事件分组，以最大程度提高性能计数器的利用率与提高测量效率：在测量时，加上 `--optimize-event-groups` 的选项，会基于探测结果优化分组，例如：

```
./hperf --optimize-event-groups -a -d 10 -c 7
Detecting available programmable counters on each CPU ...
18 available programmable counters on CPU 0
18 available programmable counters on CPU 1
18 available programmable counters on CPU 2
18 available programmable counters on CPU 3
29 available programmable counters on CPU 4
29 available programmable counters on CPU 5
29 available programmable counters on CPU 6
29 available programmable counters on CPU 7
Adaptive Grouping:
Before:
[0]: { inst_spec, ld_spec, st_spec, dp_spec, vfp_spec, ase_spec, br_immed_spec, br_indirect_spec, br_return_spec }
[1]: { l1d_cache_refill, l1i_cache_refill, l2d_cache_refill, l3d_cache_refill, l1d_tlb_refill, l1i_tlb_refill, br_mis_pred_retired }
[2]: { bus_access_rd, bus_access_wr, mem_access_rd, mem_access_rd_percyc, dtlb_walk, itlb_walk, dtlb_walk_percyc, itlb_walk_percyc }
After:
[0]: { inst_spec, ld_spec, st_spec, dp_spec, vfp_spec, ase_spec, br_immed_spec, br_indirect_spec, br_return_spec }
[1]: { l1i_cache_refill, l1i_tlb_refill, l1d_cache_refill, l1d_tlb_refill, l2d_cache_refill, br_mis_pred_retired, l3d_cache_refill, dtlb_walk, itlb_walk, bus_access_rd, bus_access_wr, mem_access_rd, mem_access_rd_percyc, dtlb_walk_percyc, itlb_walk_percyc }
```

> 由于探测计数器数量需要一定时间，因此做了探测结果的缓存：在同一台机器上，只需要完成一次探测即可，后续会利用缓存的结果进行分组优化。

#### 输出

数据完成采集后，输出性能事件的统计报告，其中输出的计数值是根据复用计数器各事件占用的事件比例进行估计后的结果。

此外还会输出微架构性能指标，指标体系是根据 CPI 拆解的理论构建的，即通过两种不同的拆解方式，判断对 CPI 贡献最大的那一个部分。

例如，在 `android-arm64-cortex-x4` 预设下和天玑 9400+ 平台，输出的性能报告如下：

```
========== Performance Statistics ==========
Fixed events (10070.35 ms, 100.00 %)
  cpu_cycles                     677,438,887
  cnt_cycles                      18,323,463
  inst_retired                   425,690,280
Group 1 (4025.45 ms, 39.97 %)
  inst_spec                      571,789,107
  ld_spec                         31,409,556
  st_spec                        159,663,267
  dp_spec                        256,171,804
  vfp_spec                            50,443
  ase_spec                           333,712
  br_immed_spec                    3,262,234
  br_indirect_spec                   874,482
  br_return_spec                     713,540
Group 2 (3023.34 ms, 30.02 %)
  l1d_cache_refill                 3,773,532
  l1i_cache_refill                16,747,969
  l2d_cache_refill                13,905,970
  l3d_cache_refill                 4,538,687
  l1d_tlb_refill                   3,801,009
  l1i_tlb_refill                   2,430,739
  br_mis_pred_retired              3,212,271
Group 3 (3021.56 ms, 30.00 %)
  bus_access_rd                   38,515,399
  bus_access_wr                   10,113,233
  mem_access_rd                  129,516,746
  mem_access_rd_percyc         2,718,803,303
  dtlb_walk                          912,123
  itlb_walk                          321,088
  dtlb_walk_percyc               159,531,729
  itlb_walk_percyc                32,943,594
=========== Performance Metrics ============
Pipeline basic metrics:
  CPI                                 1.5914
  CPU utilization                    14.00 %
  Average frequency               0.4806 GHz
Breakdown based on instruction mix:
  Load                                5.49 %
  Store                              27.92 %
  Integer data processing            44.80 %
  Floating point                      0.01 %
  Advanced SIMD                       0.06 %
  Immediate branch                    0.57 %
  Indirect branch                     0.15 %
  Return branch                       0.12 %
Breakdown based on misses:
 Cache:
  L1D cache MPKI                     10.1053
  L1I cache MPKI                     44.8501
  L2 cache MPKI                      37.2394
  L3 cache MPKI                      12.1543
 TLB:
  L1D TLB MPKI                       10.1789
  L1I TLB MPKI                        6.5094
  DTLB walk PKI                       1.8382
  ITLB walk PKI                       0.6471
 Branch predictor:
  Branch MPKI                         8.6023
Memory access latency:
  Memory read latency         20.9919 cycles
  DTLB walk latency          174.9014 cycles
  ITLB walk latency          102.5997 cycles
============================================
```

> 不同平台由于硬件差异，支持的性能指标存在差异。

若系统全局测量，事件计数值是每个 CPU 上事件计数值之和，并且是估计后的结果。

#### 模式4: CMN 内存带宽监测（仅 Linux）

针对使用 Arm CMN（Coherent Mesh Network）片上网络的服务器处理器，监测内存控制器的上下行数据流量。

**前置步骤：探测内存控制器位置**

首次使用前，需要运行探测脚本确定内存控制器在 CMN 网格中的物理位置：

```bash
# 编译内存负载工具（用于探测）
gcc -O2 -fopenmp tools/src/my_workload.c -o tools/bin/test_mc_pos_workload

# 运行探测脚本（需要 root 权限和 perf 工具）
python3 tools/scripts/detect_mc_pos.py /tmp/mc_positions.txt
```

> 探测脚本依赖 `pandas` 和 `scikit-learn`（`pip install pandas scikit-learn`），通过 KMeans 聚类分析 perf 计数器数据自动识别 MC 端口位置。

探测结果示例（`/tmp/mc_positions.txt`）：
```
arm_cmn_0 2
0x28
0x48
```

**运行带宽监测**

```bash
# 监测所有方向的内存带宽，持续 30 秒，每秒采样一次
./hperf --monitor arm_cmn_mem_bw_all --cmn-mc-pos /tmp/mc_positions.txt -d 30 -i 1000

# 仅监测上行（读）带宽，输出到 CSV 文件
./hperf --monitor arm_cmn_mem_bw_up --cmn-mc-pos /tmp/mc_positions.txt -d 10 -i 500 -o bandwidth.csv

# 不限时长，Ctrl+C 停止
./hperf --monitor arm_cmn_mem_bw_all --cmn-mc-pos /tmp/mc_positions.txt -i 1000
```

监测目标选项：
- `arm_cmn_mem_bw_all`：同时监测上行（读）和下行（写）带宽
- `arm_cmn_mem_bw_up`：仅监测上行带宽
- `arm_cmn_mem_bw_down`：仅监测下行带宽

输出格式：`timestamp,bandwidth_up,bandwidth_down`（单位：字节/间隔）

### GPU 工具 gperf

使用 `-h` 选项列出使用说明。

使用 `--print-info` 选项列出 Arm Mali GPU 的硬件架构配置信息和支持的性能事件。

#### 全局测量

目前 gperf 针对 Arm Mali GPU 的工作队列和系统内存读写，设计了固定的性能指标，并做了可视化，例如：

```
./gperf -d 10 -i 1000
```

其中 `-d 10` 采集时间 10s，`-i 1000` 采样间隔 1000 ms。这之后，会在屏幕上以可视化方式实时显示信息。

```
                  GPU Active Cycles  [    416117310 ]
                                     (   41.532 MHz )
                                             │
                        ┌────────────────────┼─────────────────────┐
                        │                    │                     │
          Jobs     [     788 ]          [   27833 ]           [    2433 ]
                  (     79 /s )        (   2778 /s )         (    243 /s )
                        │                    │                     │
   Work Queues    Compute Queue      Binning Phase Queue    Main Phase Queue
 Active Cycles   [      4049926 ]     [     23585821 ]      [    211589665 ]
                 (    0.404 MHz )     (    2.354 MHz )      (   21.118 MHz )
                        │                    │                     │
   Utilization     [  0.97 % ]          [  5.67 % ]           [ 50.85 % ]
                        │                    │                     │
         Tasks    [      3851 ]        [    947389 ]         [    889303 ]
                 (      384 /s )      (    94558 /s )       (    88760 /s )
                        │                    │                     │
                ┌───────┴────────────────────┴─────────────────────┴───────┐
                │                       Shader Cores                       │
                └───────────────┬────────────────────────┬─────────────────┘
                                │                        │
                        External Read Port      External Write Port
                                │                        │
     Bandwidth          [   137.105 MiB/s ]     [   143.455 MiB/s ]
       Latency          [   248.37 Cycles ]
```

测量结束后，输出这一段时间聚合后的性能数据。

也可以加上 `-o test.csv` 选项，将测量结果以 CSV 文件格式输出。 

## 代码开发相关备注

### Git Hooks 配置

仓库提供了 pre-commit hook，在提交代码时自动检查 C++ 文件的格式规范（基于 `.clang-format`）。Hook 脚本位于 `scripts/hooks/pre-commit`，**克隆仓库后需执行一次以下命令启用**：

```bash
git config core.hooksPath scripts/hooks
```

Hook 会优先使用 `$NDK_HOME` 中自带的 `clang-format`，无需额外安装。若格式检查不通过，提交会被中止，并提示修复命令。

### clangd 代码提示

CMake 配置时会自动在项目根目录创建 `compile_commands.json` 的符号链接，指向最近一次配置的构建目录。切换预设时只需重新运行 `cmake --preset <name>` 即可自动更新。

`.clang-format` 文件用于按照 Google 风格格式化代码。

### 增加针对新 CPU 平台的支持

PMU 事件和指标的配置已外置为 TOML 文件，添加新平台支持**无需修改任何 C++ 源文件**：

1. 在 `config/` 下新建 `cpu_<name>.toml`，定义 `fixed_events`、`event_groups`、`metric_sections`
2. 在 `CMakeLists.txt` 的 `CPU_DEPLOY_LIST` 中追加 `<name>`

即可通过 `cmake --build --preset android-arm64 --target deploy-<name>` 部署到设备。