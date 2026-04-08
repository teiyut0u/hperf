# hperf — 开发者参考

## 项目定位

hperf 是一个硬件性能分析工具套件，通过 Linux `perf_event_open` 系统调用读取 PMU 硬件计数器。支持两个平台：
- **Android 移动端**：通过 adb 推送到设备运行，进行 CPU/GPU 性能分析
- **Linux 服务器**：原生编译运行，进行 CPU 性能分析和 Arm CMN 内存带宽监测

包含两个独立工具：
- **hperf**：CPU PMU 性能分析（支持全局采样、进程级追踪），Linux 上额外支持 Arm CMN 内存带宽监测
- **gperf**：GPU 性能分析（基于 Arm hwcpipe/libGPUCounters，仅 Android）

---

## 目录结构

```
hperf/
├── CMakeLists.txt              # 顶层构建脚本
├── CMakePresets.json           # 构建预设（android-arm64, linux）
├── config/                     # 运行时 CPU 配置文件（TOML）
│   ├── cpu_oryon.toml
│   ├── cpu_cortex_x4.toml
│   ├── cpu_c1_ultra.toml
│   └── cpu_ustress.toml
├── include/
│   ├── hperf/               # CPU 工具头文件（16 个）
│   │   ├── monitor/            # CMN 带宽监测模块头文件（仅 Linux 编译）
│   │   └── ...
│   └── gperf/               # GPU 工具头文件
├── src/
│   ├── hperf/               # CPU 工具源文件（12 个）
│   │   ├── monitor/            # CMN 带宽监测模块源文件（仅 Linux 编译）
│   │   └── ...
│   └── gperf/               # GPU 工具源文件（仅 Android 编译）
├── test/                       # 单元测试（GoogleTest）
├── tools/
│   ├── scripts/                # 辅助脚本
│   │   └── detect_mc_pos.py    # Arm CMN 内存控制器位置探测（KMeans 聚类）
│   └── src/                    # 辅助工具源码
│       ├── my_workload.c       # 内存访问负载（用于 MC 探测）
│       └── stream.c            # STREAM 带宽基准测试
└── third_party/
    ├── tomlplusplus/           # TOML 解析库（单头文件 toml.hpp）
    └── libGPUCounters/         # Arm hwcpipe GPU 计数器库（仅 Android）
```

---

## 架构概览

### hperf（CPU 工具）

```
main.cpp
  │
  ├─► ArgsParser                     解析 argv → ProfileConfig
  │     [args_parser.h/cpp]
  │
  ├─► CounterDetector                探测 PMU 硬件计数器数量
  │     [counter_detector.h/cpp]
  │     ├─ detect()                  逐步增加事件，检测 time_enabled != time_running
  │     ├─ adaptive_grouping()       结果缓存至 /tmp/.hperf
  │     └─ TestResult { kNoMux, kMuxDetected, kError }  （私有枚举）
  │
  ├─► PMUConfig                      运行时从 .hperf.toml 加载配置
  │     [pmu_config.h/cpp]
  │     ├─ fixed_events_[]           固定事件（每轮都采集）
  │     ├─ event_groups_[][]         可调度事件分组
  │     ├─ metric_sections_[]        指标公式（MetricSection / MetricItem）
  │     ├─ load_from_file()          TOML 解析入口
  │     └─ adaptive_grouping()       按计数器预算合并分组
  │
  ├─► EventScheduler                 管理 perf fd，控制采样调度
  │     [event_scheduler.h/cpp]
  │     ├─ fds_[][]                  每个事件组的 fd 列表
  │     ├─ read_buffers_[]           每个事件组的 GroupReadBuffer
  │     ├─ initialize()              创建所有 perf_event_open fd
  │     ├─ reset / enable / disable  控制计数器状态
  │     ├─ switch_to_next_group()    轮换到下一个事件组
  │     └─ read_active_group_data()  读取当前组计数值
  │
  │     GroupReadBuffer              [read_buffer.h] 对 perf read 格式的封装
  │       ├─ Header: { nr, time_enabled, time_running }
  │       └─ Entry[]: { value, id }
  │
  └─► Reporter                       汇总统计 + 指标计算输出
        [reporter.h/cpp]
        ├─ process_a_record()        累积原始计数
        ├─ estimation()              多路复用时间补偿
        ├─ print_stats()             打印事件计数汇总
        └─ print_metrics()           遍历 MetricSection，调用 ExprEvaluator 求值
              └─► ExprEvaluator      [expr_evaluator.h/cpp] 算术表达式解析器
```

**数据流**：

```
.hperf.toml ──load_from_file()──► PMUConfig
                                          │ fixed_events, event_groups,
argv ──parse()──► ProfileConfig           │ metric_sections
                        │                 │
                        ▼                 ▼
                   EventScheduler ◄────────
                        │
           每个采样间隔：reset → enable → sleep → disable → read
                        │
                        ▼  Record { timestamp, cpu_id, group_id, event_id, value }
                     Reporter
                        │
                   estimation()        ← 多路复用时间补偿
                        │
               print_stats()  print_metrics()
                                        │
                                 ExprEvaluator::evaluate(expr, vars)
```

**三种工作模式**：

| 模式 | 启动方式 | 说明 |
|------|----------|------|
| 全局（`-a`） | 每个 CPU 核心建一个 EventScheduler | `target_pid=-1, target_cpu=N` |
| 进程（`-p <pid>`） | 单个 EventScheduler 跟踪目标进程 | `target_pid=PID, target_cpu=-1` |
| CMN 带宽监测（`--monitor`） | 独立流程，不使用 EventScheduler | 仅 Linux，见下方 |

### CMN 带宽监测模块（仅 Linux）

```
main.cpp
  │  if (monitor_target != NO_MONITOR_TARGET)
  │
  └─► cmn_bandwidth_monitor()         入口函数
        [monitor/cmn_bandwidth_monitor.h/cpp]
        │
        ├─ read_mc_pos_file()          解析 MC 位置配置文件
        │
        ├─ add_arm_cmn_mem_bw_monitor()  构建 GeneralMonitor 实例
        │    │
        │    ├─► MonitorUtil             [monitor/monitor_util.h/cpp]
        │    │   ├─ add_watchpoint_monitor_attr()  配置 watchpoint 事件参数
        │    │   ├─ read_file()          读取 sysfs 文件
        │    │   └─ string2integer()     支持 hex/bin/oct/dec 解析
        │    │
        │    ├─► PerfEventAttr           [monitor/perf_event_attr.h/cpp]
        │    │   ├─ set_type()           从 device/type 读取类型
        │    │   ├─ add_event()          从 device/events/ 读取事件编码
        │    │   └─ add_field()          从 device/format/ 读取字段格式，打包到 config
        │    │
        │    └─► SingleEventController   [monitor/single_event_controller.h/cpp]
        │        ├─ open_event()         打开单个 perf_event fd
        │        └─ implements EventControllerInterface [monitor/event_controller_interface.h]
        │
        └─ do_monitor()               主循环：定时读取 → 缩放 → 输出
             │
             └─► GeneralMonitor        [monitor/general_monitor.h/cpp]
                  ├─ start() / stop()  控制所有 EventController
                  └─ get_scaled_count() 读取并按 time_enabled/time_running 缩放
```

**CMN 带宽监测数据流**：

```
mc_pos.txt ──read_mc_pos_file()──► MC positions (device_path, nodeid[])
                                          │
argv ──parse()──► ProfileConfig           │
  (--monitor, --cmn-mc-pos)               │
                        │                 │
                        ▼                 ▼
              add_arm_cmn_mem_bw_monitor()
                        │
           为每个 MC 端口创建 SingleEventController
           (watchpoint_up / watchpoint_down 事件)
                        │
                        ▼
                  do_monitor() 主循环
                  每个间隔：get_scaled_count() → 累加 flit 数 × 32 字节
                        │
                        ▼
              输出 CSV: timestamp, bandwidth_up, bandwidth_down
```

### gperf（GPU 工具，仅 Android）

```
main.cpp
  ├── hwcpipe (third_party/libGPUCounters)  — Arm Mali GPU 计数器读取库
  ├── Record        — GPU 性能数据的记录与存储
  └── Visualizer    — 实时可视化 GPU 指标
```

---

## 构建

项目支持两种构建预设：

### Android 构建（交叉编译）

**前置条件**：`NDK_HOME` 环境变量指向 Android NDK（推荐 v29.0.13599879）

```bash
cmake --preset android-arm64
cmake --build --preset android-arm64

# 推送到设备（按目标设备的 CPU 选择 target）
cmake --build --preset android-arm64 --target deploy-oryon       # Qualcomm Oryon
cmake --build --preset android-arm64 --target deploy-cortex_x4   # Arm Cortex-X4
cmake --build --preset android-arm64 --target deploy-c1_ultra    # C1-Ultra
cmake --build --preset android-arm64 --target deploy-ustress     # Ustress 评估
```

各 `deploy-*` target 推送的配置文件：

| Target | 推送的配置文件 |
|--------|--------------|
| `deploy-oryon` | `config/cpu_oryon.toml` |
| `deploy-cortex_x4` | `config/cpu_cortex_x4.toml` |
| `deploy-c1_ultra` | `config/cpu_c1_ultra.toml` |
| `deploy-ustress` | `config/cpu_ustress.toml` |

编译产物与 CPU 无关，区别仅在于 `deploy-*` target 将对应的 TOML 文件推送到设备上的 `/data/local/tmp/.hperf.toml`。

Android 构建包含 hperf + gperf，**不包含** monitor 模块。

### Linux 构建（原生编译）

```bash
cmake --preset linux
cmake --build --preset linux
```

使用系统编译器（g++ 或 clang++），无需 NDK。Linux 构建包含 hperf + monitor 模块，**不包含** gperf。

### 条件编译说明

| 模块 | Android | Linux |
|------|---------|-------|
| hperf 核心（PMU 采样） | 编译 | 编译 |
| monitor（CMN 带宽监测） | **不编译** | 编译 |
| gperf（GPU） | 编译 | **不编译** |

CMake 使用 `if(ANDROID)` / `if(NOT ANDROID)` 控制，C++ 侧使用 `#ifndef __ANDROID__` 守卫 monitor 相关 `#include`。

---

## 添加新 CPU 平台支持

1. 在 `config/` 下新建 `cpu_<name>.toml`，定义 `fixed_events`、`event_groups`、`metric_sections`
2. 在 `CMakeLists.txt` 的 `CPU_DEPLOY_LIST` 中追加 `<name>`（`set(CPU_DEPLOY_LIST oryon cortex_x4 ...)`）

**无需修改任何 C++ 源文件。**

---

## 编码规范

### 格式

- **格式化工具**：`clang-format`，配置文件 `.clang-format`（Google 风格，不限行长）
- **提交检查**：pre-commit hook 会自动检查所有暂存的 `.cpp` / `.h` 文件，不合规则拒绝提交

#### 首次配置 Git Hooks（克隆仓库后执行一次）

```bash
git config core.hooksPath scripts/hooks
```

Hook 脚本位于 `scripts/hooks/pre-commit`，已纳入版本控制。`clang-format` 查找顺序：
1. `$NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/clang-format`（推荐，无需额外安装）
2. 系统全局 `clang-format`
3. 两者均未找到时报错并中止提交

#### 手动格式化

```bash
# 单个文件
"$NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/clang-format" -i <file>

# 整个项目
find src include -name '*.cpp' -o -name '*.h' | \
  xargs "$NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/clang-format" -i
```

### C++ 风格

- 标准：**C++17**，不使用 GNU 扩展（`CMAKE_CXX_EXTENSIONS OFF`）
- 头文件保护：使用 `#pragma once`
- 禁止拷贝的类显式声明 `= delete`
- 资源管理优先使用 RAII（析构函数/移动语义），不使用裸指针管理生命周期
- 错误处理：初始化操作抛出 `HperfError` 异常（main() 统一 catch），运行时操作返回 `bool`；monitor 模块使用 `std::error_code` / `std::errc`
- 新增 PMU 事件名称使用全小写加下划线，与现有事件表保持一致

### 命名

- 类名：`PascalCase`
- 函数/变量：`snake_case`
- 私有成员：`trailing_underscore_`
- 宏/编译定义：`UPPER_CASE`

---

## 测试

```bash
# Android：一键编译、推送、在设备上执行所有单元测试
cmake --build --preset android-arm64 --target run_tests

# Android：仅推送（手动在设备上执行）
cmake --build --preset android-arm64 --target push_tests
adb shell /data/local/tmp/test/test_pmu_config
adb shell /data/local/tmp/test/test_reporter
adb shell /data/local/tmp/test/test_read_buffer
adb shell /data/local/tmp/test/test_expr_evaluator
adb shell /data/local/tmp/test/test_counter_detector

# Linux：本地编译并执行所有单元测试
cmake --build --preset linux --target run_tests
```

---

## 关键数据结构

### 核心模块

| 类型 | 文件 | 字段 / 说明 |
|------|------|-------------|
| `PMUEvent` | `include/hperf/pmu_event.h` | `{ name, description, encoding }` |
| `PMUConfig` | `include/hperf/pmu_config.h` | `fixed_events[]`, `event_groups[][]`, `metric_sections[]` |
| `MetricItem` | `include/hperf/pmu_config.h` | `{ name, type, expr }` — type: `decimal/percentage/GHz/cycles` |
| `MetricSection` | `include/hperf/pmu_config.h` | `{ title, items[] }` |
| `ExprEvaluator` | `include/hperf/expr_evaluator.h` | `static evaluate(expr, vars)` — 递归下降，支持 `+−×÷()` 和变量名 |
| `EventScheduler` | `include/hperf/event_scheduler.h` | `fds_[][]`, `read_buffers_[]`；管理 perf fd，控制采样调度 |
| `GroupReadBuffer` | `include/hperf/read_buffer.h` | `Header { nr, time_enabled, time_running }` + `Entry[] { value, id }` |
| `Record` | `include/hperf/reporter.h` | `{ timestamp, cpu_id, group_id, event_id, value }` |
| `EventStats` | `include/hperf/reporter.h` | `{ total_value, estimated_value }` |
| `Reporter` | `include/hperf/reporter.h` | `stat_[][]`, `enabled_time_in_ns_[]`, `total_time_in_ns_` |
| `ProfileConfig` | `include/hperf/profile_config.h` | `mode`, `monitor_target`, `test_duration`, `switch_group_interval`, `cpu_id_list`, `target_pid`, `mc_position_file`, `output_stream` |
| `MonitorTarget` | `include/hperf/profile_config.h` | 枚举：`NO_MONITOR_TARGET`, `ARM_CMN_MEM_BW_UP/DOWN/ALL` |

### Monitor 模块（仅 Linux）

| 类型 | 文件 | 说明 |
|------|------|------|
| `PerfEventAttr` | `include/hperf/monitor/perf_event_attr.h` | `struct perf_event_attr` 封装，解析 sysfs 设备格式 |
| `EventControllerInterface` | `include/hperf/monitor/event_controller_interface.h` | 纯虚接口：`enable/disable/reset/read/close` |
| `SingleEventController` | `include/hperf/monitor/single_event_controller.h` | 单个 perf_event fd 的 RAII 管理 |
| `GeneralMonitor` | `include/hperf/monitor/general_monitor.h` | 管理多个 EventController，提供 `get_scaled_count()` |
| `MonitorUtil` | `include/hperf/monitor/monitor_util.h` | 工具类：`read_file()`, `string2integer()`, `add_watchpoint_monitor_attr()` |
