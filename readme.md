# 基于Vitis HLS的LK光流算法 FPGA加速实现 - Linux环境运行指南

## 项目概述

本项目基于 FPGA 的高层次综合（HLS）技术实现了 Lucas-Kanade（LK）光流算法的硬件加速。项目包含完整的硬件函数实现、测试验证程序及工程自动化脚本，所有开发工作均在 AI 大模型助教的协助下完成。该方案可高效计算两帧图像间的运动向量，适用于实时视觉导航、运动目标跟踪等应用场景。

本项目专门针对Linux环境进行优化，通过Vitis HLS工具将C++代码转换为RTL硬件描述，实现高效的实时光流计算。

## 系统环境要求

### 硬件要求
- CPU: Intel/AMD x86_64处理器
- 内存: 8GB以上（推荐16GB）
- 存储: 20GB以上可用空间

### 软件环境
- **操作系统**: Ubuntu 18.04/20.04/22.04 LTS (推荐)
- **开发工具**: Xilinx Vitis HLS 2022.2 或更新版本 / AMD Vitis HLS 2024.2（推荐）
- **图像处理库**: OpenCV 3.4+ （用于C仿真测试）
- **编译器**: GCC 7.5+ 或 Clang 8.0+

## 核心功能与技术参数

### 算法功能
1. **LK 光流算法硬件加速**：通过 HLS 实现光流计算的核心逻辑，包括图像滤波、梯度计算和运动向量求解
2. **高效数据处理**：采用像素打包技术（双像素存储）减少内存访问开销，优化硬件资源利用率
3. **完整测试验证**：提供基于 OpenCV 的测试框架，支持图像加载、结果可视化及性能评估

### 技术参数

| 参数项       | 具体配置                   |
| ------------ | -------------------------- |
| 最大图像尺寸 | 398x594 像素               |
| 像素位宽     | 8 位（灰度图）             |
| 滤波窗口大小 | 5x5（各向同性高斯滤波）    |
| 积分窗口大小 | 5x5（LK 算法核心计算窗口） |
| 亚像素精度   | 6 位（1/64 像素）          |
| 数据接口     | 数组形式（便于 HLS 综合）  |

## 项目目录结构

```
reference/LucasKanade/
├── build_linux/           # Linux构建目录
│   ├── run_hls.tcl        # 主要TCL自动化脚本（完整流程）
│   ├── vitis_hls.log      # HLS运行日志
│   └── lk_prj/            # HLS工程目录（运行后生成）
│       └── solution1/     # 解决方案目录
├── src/                   # 源码目录
│   ├── lk_define.h        # 硬件函数参数定义头文件
│   ├── lk_hls.cpp         # LK光流算法硬件实现
│   ├── lk_hls.h           # 硬件函数声明头文件
│   └── LKof_main.cpp      # 测试程序（C仿真用）
├── test_data/             # 测试图像数据
│   ├── frame00.png        # 第一帧测试图像
│   └── frame01.png        # 第二帧测试图像
├── result/                # 结果输出目录（运行后生成）
├── sw_lk/                 # 软件参考实现目录
└── readme_linux.md        # Linux环境说明文档
```

## 模块说明

### 1. 硬件函数（src/lk_hls.cpp）

实现 LK 光流算法的硬件加速逻辑，核心函数包括：

- `hls_isotropic_kernel`：5x5 各向同性滤波，抑制图像噪声
- `hls_twoIsotropicFilters`：同时对两帧图像进行滤波，优化数据访问
- `hls_derivatives_kernel`：计算图像空间梯度（Ix、Iy）和时间导数（It）
- `hls_LK`：主函数，整合上述模块，输出运动向量（vx、vy）

### 2. 参数定义（src/lk_define.h）

定义算法关键参数和数据类型：

- 图像尺寸、窗口大小等配置参数
- 基于 `ap_int` 的自定义数据类型（优化硬件资源）
- 滤波器系数、阈值等算法参数

### 3. 测试程序（src/LKof_main.cpp）

验证硬件函数的正确性，主要功能：

- 图像加载与格式转换（OpenCV Mat ↔ HLS 数组）
- 调用硬件加速函数并计时
- 光流向量可视化（绘制箭头）
- 结果保存与性能统计

## 环境配置

### 1. 安装Vitis HLS

#### 方式一：官方安装包
```bash
# 下载Vitis统一软件平台（包含HLS）
# 访问: https://www.xilinx.com/support/download.html

# 解压并安装
tar -xf Xilinx_Unified_2022.2_1014_8888.tar.gz
cd Xilinx_Unified_2022.2_1014_8888
sudo ./xsetup

# 安装后配置环境变量
echo 'source /opt/Xilinx/Vitis_HLS/2022.2/settings64.sh' >> ~/.bashrc
source ~/.bashrc
```

#### 方式二：验证安装
```bash
# 检查HLS工具是否可用
which vitis_hls
vitis_hls -version

# 预期输出类似：
# ****** Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2022.2.2 (64-bit)
```

### 2. 安装OpenCV（用于C仿真）

#### 使用conda安装（推荐）
```bash
# 一键部署（与当前环境一致）
conda create -n opencv_env python=3.8 -y && conda activate opencv_env && pip install numpy==1.24.4 opencv-python==4.13.0.90

# 验证安装
python -c "import cv2; import numpy as np; print(cv2.__version__, np.__version__)"
```

#### 使用apt安装
```bash
sudo apt update
sudo apt install libopencv-dev libopencv-contrib-dev
```

### 3. 环境变量配置

```bash
# 编辑环境配置文件
vim ~/.bashrc

# 添加以下内容
export XILINX_HLS=/opt/Xilinx/Vitis_HLS/2022.2
export PATH=$XILINX_HLS/bin:$PATH
export LD_LIBRARY_PATH=$XILINX_HLS/lib/lnx64.o:$LD_LIBRARY_PATH

# 如果使用conda安装的OpenCV
export OPENCV_ROOT=~/.conda/envs/opencv_env
export LD_LIBRARY_PATH=$OPENCV_ROOT/lib:$LD_LIBRARY_PATH

# 重新加载配置
source ~/.bashrc
```

## 快速开始

### 1. 进入工程目录

```bash
# 从工程根目录进入build_linux目录
cd reference/LucasKanade/build_linux

# 确认文件结构
ls -la
# 应该看到：run_hls.tcl
```

### 2. 图像路径说明

程序会根据当前工作目录自动定位到 `LucasKanade` 根目录，然后读取 `test_data` 下的图片，无需手工改路径。只要在工程目录内运行即可。

### 3. 验证图像文件存在

```bash
# 确认测试图像存在
ls -la ../test_data/frame*.png

# 预期输出：
# frame00.png
# frame01.png
# （或其他frame开头的png文件）

# 如果没有图像文件，可以从其他目录复制
cp /path/to/your/images/*.png ../test_data/
```

### 4. 运行完整构建流程

```bash
# 执行HLS自动化脚本
vitis_hls -f run_hls.tcl

# 或者使用hls命令（如果配置了别名）
hls run_hls.tcl
```

### 5. 脚本执行过程说明

脚本会依次执行以下阶段，每个阶段都有明确的进度提示：

```
== 清理旧日志文件 ==
== 开始执行脚本，时间: [时间戳] ==
== 配置OpenCV环境 ==
== 创建/打开HLS项目 ==
== 设置顶层函数 ==
== 添加设计源文件 ==
== 添加测试文件 ==
== 打开/重置解决方案 ==
== 设置目标FPGA器件 ==
== 创建时钟约束 ==
== 运行C仿真（CSIM） ==
== 运行C综合（CSYNTH） ==
== 运行C/RTL协同仿真（COSIM） ==
== 脚本执行结束，退出Vitis HLS工具 ==
```

### 6. 输出结果位置说明

运行完成后，所有结果文件都会生成在 `lk_prj` 目录下：

#### 6.1 结果说明

测试程序运行后会生成以下输出文件：

- `output_img1_original.jpg`/`output_img2_original.jpg`：输入图像
- `output_optical_flow.jpg`：光流向量可视化（绿色箭头表示运动方向）
- `output_vx.jpg`/`output_vy.jpg`：X/Y 方向运动向量分量图（亮度表示大小）

#### 6.2 HLS工程结果目录结构

```
lk_prj/
├── hls.app                    # HLS工程配置文件
└── solution1/                 # 解决方案目录
    ├── csim/                  # C仿真结果
    │   ├── build/            # 编译生成的可执行文件
    │   │   ├── csim.exe      # C仿真可执行程序
    │   │   └── apcc_db/      # 编译数据库
    │   └── report/           # C仿真报告
    │       └── hls_LK_csim.log
    ├── syn/                   # 综合结果
    │   ├── report/           # 综合报告
    │   │   ├── hls_LK_csynth.rpt    # 【重要】综合报告（资源使用、时序）
    │   │   ├── hls_LK_csynth.xml    # XML格式综合报告
    │   │   └── hls_LK.aps           # 分析数据
    │   ├── verilog/          # 【重要】生成的RTL代码
    │   │   ├── hls_LK.v      # 顶层Verilog文件
    │   │   ├── hls_LK_*.v    # 子模块Verilog文件
    │   │   └── *.dat         # 初始化数据文件
    │   └── vhdl/             # VHDL格式RTL代码（如果选择）
    └── impl/                  # 协同仿真结果
        ├── report/           # 协同仿真报告
        │   └── hls_LK_cosim.rpt     # 协同仿真时序报告
        └── verilog/          # 仿真用RTL文件
```

#### 6.3 C仿真输出图像（如果C仿真成功）

```
result/                        # 在LKof_main.cpp中指定的输出目录
├── output_img1_original.jpg   # 输入第一帧图像
├── output_img2_original.jpg   # 输入第二帧图像
├── output_optical_flow.jpg    # 【重要】光流可视化结果（绿色箭头）
├── output_vx.jpg              # X方向运动向量可视化
└── output_vy.jpg              # Y方向运动向量可视化
```

**注意：** result目录的位置取决于C仿真的执行目录，通常在：
- `lk_prj/solution1/csim/build/result/` 
- 或者在build_linux目录下的 `result/`

#### 6.4 关键输出文件说明

| 文件路径 | 内容说明 | 重要程度 |
|---------|----------|----------|
| `lk_prj/solution1/syn/report/hls_LK_csynth.rpt` | 综合报告：资源使用、时序分析、性能估计 | ⭐⭐⭐⭐⭐ |
| `lk_prj/solution1/syn/verilog/hls_LK.v` | 生成的顶层Verilog RTL代码 | ⭐⭐⭐⭐⭐ |
| `lk_prj/solution1/impl/report/hls_LK_cosim.rpt` | 协同仿真报告：RTL与C代码对比验证 | ⭐⭐⭐⭐ |
| `result/output_optical_flow.jpg` | 光流可视化结果图像 | ⭐⭐⭐ |
| `vitis_hls.log` | 完整的HLS运行日志 | ⭐⭐⭐ |

## 分步执行指南

如果不需要运行完整流程，可以选择性地执行某个步骤。只需要修改 `run_hls.tcl` 脚本，注释掉不需要的部分。

### 方法1：修改现有TCL脚本

编辑 `run_hls.tcl` 文件，在不需要的步骤前加 `#` 注释：

#### 1.1 仅运行C仿真（CSIM）

```bash
# 编辑TCL脚本
vim run_hls.tcl

# 在以下两行前加 # 注释：
# csynth_design
# cosim_design -ldflags "-L$opencv_lib -Wl,-rpath,$opencv_lib -lopencv_core -lopencv_imgproc -lopencv_imgcodecs"
```

修改后的脚本末尾应该是：
```tcl
# ============ HLS流程执行阶段 ============
puts "== 运行C仿真（CSIM） =="
csim_design -ldflags "-L$opencv_lib -Wl,-rpath,$opencv_lib -lopencv_core -lopencv_imgproc -lopencv_imgcodecs"
puts "== 运行C综合（CSYNTH） =="
# csynth_design
puts "== 运行C/RTL协同仿真（COSIM） =="
# cosim_design -ldflags "-L$opencv_lib -Wl,-rpath,$opencv_lib -lopencv_core -lopencv_imgproc -lopencv_imgcodecs"
puts "== 脚本执行结束，退出Vitis HLS工具 =="
exit
```

#### 1.2 仅运行C综合（CSYNTH）

```tcl
# ============ HLS流程执行阶段 ============
puts "== 运行C仿真（CSIM） =="
# csim_design -ldflags "-L$opencv_lib -Wl,-rpath,$opencv_lib -lopencv_core -lopencv_imgproc -lopencv_imgcodecs"
puts "== 运行C综合（CSYNTH） =="
csynth_design
puts "== 运行C/RTL协同仿真（COSIM） =="
# cosim_design -ldflags "-L$opencv_lib -Wl,-rpath,$opencv_lib -lopencv_core -lopencv_imgproc -lopencv_imgcodecs"
puts "== 脚本执行结束，退出Vitis HLS工具 =="
exit
```

#### 1.3 仅运行协同仿真（COSIM）

**注意：协同仿真需要先有综合结果，建议保留综合步骤**

```tcl
# ============ HLS流程执行阶段 ============
puts "== 运行C仿真（CSIM） =="
# csim_design -ldflags "-L$opencv_lib -Wl,-rpath,$opencv_lib -lopencv_core -lopencv_imgproc -lopencv_imgcodecs"
puts "== 运行C综合（CSYNTH） =="
csynth_design
puts "== 运行C/RTL协同仿真（COSIM） =="
cosim_design -ldflags "-L$opencv_lib -Wl,-rpath,$opencv_lib -lopencv_core -lopencv_imgproc -lopencv_imgcodecs"
puts "== 脚本执行结束，退出Vitis HLS工具 =="
exit
```

#### 1.4 仅综合+协同仿真（跳过C仿真）

```tcl
# ============ HLS流程执行阶段 ============
puts "== 运行C仿真（CSIM） =="
# csim_design -ldflags "-L$opencv_lib -Wl,-rpath,$opencv_lib -lopencv_core -lopencv_imgproc -lopencv_imgcodecs"
puts "== 运行C综合（CSYNTH） =="
csynth_design
puts "== 运行C/RTL协同仿真（COSIM） =="
cosim_design -ldflags "-L$opencv_lib -Wl,-rpath,$opencv_lib -lopencv_core -lopencv_imgproc -lopencv_imgcodecs"
puts "== 脚本执行结束，退出Vitis HLS工具 =="
exit
```

### 方法2：创建专用脚本

#### 2.1 仅C仿真脚本

```bash
cat > csim_only.tcl << 'EOF'
# ============ 清理环境阶段 ============
puts "== 清理旧日志文件 =="
exec sh -c "rm -f flex*.log"
puts "== 开始执行脚本，时间: [clock format [clock seconds]] =="

# ============ OpenCV环境配置阶段 ============
puts "== 配置OpenCV环境 =="
set opencv_path "/home/fyt/.conda/envs/opencv_env"
set opencv_include "$opencv_path/include/opencv4"
set opencv_lib "$opencv_path/lib"
set ::env(LD_LIBRARY_PATH) "$opencv_lib:$::env(LD_LIBRARY_PATH)"

# ============ HLS项目配置阶段 ============
puts "== 创建/打开HLS项目 =="
open_project lk_prj
puts "== 设置顶层函数 =="
set_top hls_LK
puts "== 添加设计源文件 =="
add_files ../src/lk_hls.cpp
add_files ../src/lk_define.h
puts "== 添加测试文件 =="
add_files -tb ../src/LKof_main.cpp -cflags "-I$opencv_include -std=c++14" -csimflags "-L$opencv_lib -Wl,-rpath,$opencv_lib -lopencv_core -lopencv_imgproc -lopencv_imgcodecs"
puts "== 打开/重置解决方案 =="
open_solution -reset solution1
puts "== 设置目标FPGA器件 =="
set_part {xc7z020clg400-1}
puts "== 创建时钟约束 =="
create_clock -period 10 -name default

# ============ 仅C仿真 ============
puts "== 运行C仿真（CSIM） =="
csim_design -ldflags "-L$opencv_lib -Wl,-rpath,$opencv_lib -lopencv_core -lopencv_imgproc -lopencv_imgcodecs"
puts "== C仿真完成，退出Vitis HLS工具 =="
exit
EOF

# 运行仅C仿真
vitis_hls -f csim_only.tcl
```

#### 2.2 仅综合脚本

```bash
cat > synthesis_only.tcl << 'EOF'
# ============ HLS项目配置阶段 ============
puts "== 打开已有HLS项目 =="
open_project lk_prj
open_solution solution1

# ============ 仅综合 ============
puts "== 运行C综合（CSYNTH） =="
csynth_design
puts "== C综合完成，退出Vitis HLS工具 =="
exit
EOF

# 运行仅综合
vitis_hls -f synthesis_only.tcl
```

#### 2.3 仅协同仿真脚本

```bash
cat > cosim_only.tcl << 'EOF'
# ============ OpenCV环境配置阶段 ============
puts "== 配置OpenCV环境 =="
set opencv_path "/home/fyt/.conda/envs/opencv_env"
set opencv_lib "$opencv_path/lib"

# ============ HLS项目配置阶段 ============
puts "== 打开已有HLS项目 =="
open_project lk_prj
open_solution solution1

# ============ 仅协同仿真 ============
puts "== 运行C/RTL协同仿真（COSIM） =="
cosim_design -ldflags "-L$opencv_lib -Wl,-rpath,$opencv_lib -lopencv_core -lopencv_imgproc -lopencv_imgcodecs"
puts "== 协同仿真完成，退出Vitis HLS工具 =="
exit
EOF

# 运行仅协同仿真
vitis_hls -f cosim_only.tcl
```

### 方法3：使用命令行参数控制

创建一个智能脚本，根据参数选择执行步骤：

```bash
cat > run_selective.sh << 'EOF'
#!/bin/bash

# 用法说明
usage() {
    echo "用法: $0 [csim|csynth|cosim|all]"
    echo "  csim   - 仅运行C仿真"
    echo "  csynth - 仅运行C综合"
    echo "  cosim  - 运行综合+协同仿真"
    echo "  all    - 运行完整流程（默认）"
    exit 1
}

# 检查参数
STEP=${1:-all}

case $STEP in
    csim)
        echo "运行仅C仿真..."
        sed 's/^csynth_design/#csynth_design/' run_hls.tcl | \
        sed 's/^cosim_design/#cosim_design/' > temp_csim.tcl
        vitis_hls -f temp_csim.tcl
        rm temp_csim.tcl
        ;;
    csynth)
        echo "运行仅C综合..."
        sed 's/^csim_design/#csim_design/' run_hls.tcl | \
        sed 's/^cosim_design/#cosim_design/' > temp_csynth.tcl
        vitis_hls -f temp_csynth.tcl
        rm temp_csynth.tcl
        ;;
    cosim)
        echo "运行综合+协同仿真..."
        sed 's/^csim_design/#csim_design/' run_hls.tcl > temp_cosim.tcl
        vitis_hls -f temp_cosim.tcl
        rm temp_cosim.tcl
        ;;
    all)
        echo "运行完整流程..."
        vitis_hls -f run_hls.tcl
        ;;
    *)
        usage
        ;;
esac

echo "执行完成！"
EOF

chmod +x run_selective.sh

# 使用示例：
# ./run_selective.sh csim     # 仅C仿真
# ./run_selective.sh csynth   # 仅综合
# ./run_selective.sh cosim    # 综合+协同仿真
# ./run_selective.sh all      # 完整流程
```

### 执行步骤说明

#### C仿真（CSIM）
- **目的**：验证算法功能正确性
- **输出**：结果图像、性能数据
- **时间**：通常几分钟
- **依赖**：需要OpenCV环境

#### C综合（CSYNTH）
- **目的**：生成RTL代码
- **输出**：Verilog/VHDL文件、资源报告
- **时间**：通常5-15分钟
- **依赖**：无特殊依赖

#### 协同仿真（COSIM）
- **目的**：验证RTL与C代码一致性
- **输出**：时序验证报告
- **时间**：可能较长（30分钟+）
- **依赖**：需要先完成综合

### 推荐执行策略

1. **开发阶段**：先运行 `csim` 验证功能
2. **综合测试**：运行 `csynth` 查看资源使用
3. **最终验证**：运行 `cosim` 确保RTL正确性
4. **完整流程**：最终提交前运行 `all`

## 结果分析

### 1. 日志文件查看

```bash
# 查看HLS运行日志
cat vitis_hls.log

# 查看C仿真输出
cat lk_prj/solution1/csim/build/csim.log

# 查看综合报告
cat lk_prj/solution1/syn/report/hls_LK_csynth.rpt
```

### 2. 快速查看关键结果

#### 2.1 综合结果摘要
```bash
# 查看资源使用情况
grep -A 10 "Utilization Estimates" lk_prj/solution1/syn/report/hls_LK_csynth.rpt

# 查看时序结果
grep -A 5 "Timing" lk_prj/solution1/syn/report/hls_LK_csynth.rpt

# 查看延迟信息
grep -A 10 "Latency" lk_prj/solution1/syn/report/hls_LK_csynth.rpt
```

#### 2.2 C仿真结果验证
```bash
# 检查是否生成了结果图像
find . -name "output_*.jpg" -exec ls -la {} \;

# 查看C仿真是否成功
grep -i "test completed successfully" lk_prj/solution1/csim/build/csim.log
```

#### 2.3 协同仿真验证
```bash
# 检查协同仿真是否通过
grep -i "pass\|fail" lk_prj/solution1/impl/report/hls_LK_cosim.rpt
```

### 3. 结果文件位置总结

**运行完成后，主要查看以下位置的文件：**

#### 3.1 综合报告（最重要）
```bash
# 资源使用和性能分析
lk_prj/solution1/syn/report/hls_LK_csynth.rpt
```

#### 3.2 生成的RTL代码
```bash
# Verilog顶层文件
lk_prj/solution1/syn/verilog/hls_LK.v

# 查看生成的所有Verilog文件
ls -la lk_prj/solution1/syn/verilog/
```

#### 3.3 C仿真输出图像
```bash
# 方法1：在csim/build目录下查找
find lk_prj/solution1/csim/build -name "*.jpg"

# 方法2：在当前目录下查找
find . -name "output_*.jpg"

# 方法3：检查常见位置
ls -la result/ 2>/dev/null || echo "result目录不存在"
ls -la lk_prj/solution1/csim/build/result/ 2>/dev/null || echo "csim/build/result目录不存在"
```

### 4. 快速结果检查脚本

创建一个快速检查脚本：

```bash
cat > check_results.sh << 'EOF'
#!/bin/bash
echo "=== HLS构建结果检查 ==="

# 检查综合是否成功
if [ -f "lk_prj/solution1/syn/report/hls_LK_csynth.rpt" ]; then
    echo "✓ 综合成功 - 报告文件存在"
    echo "  位置: lk_prj/solution1/syn/report/hls_LK_csynth.rpt"
else
    echo "✗ 综合失败 - 报告文件不存在"
fi

# 检查RTL代码生成
if [ -f "lk_prj/solution1/syn/verilog/hls_LK.v" ]; then
    echo "✓ RTL代码生成成功"
    echo "  位置: lk_prj/solution1/syn/verilog/hls_LK.v"
    echo "  文件大小: $(wc -l < lk_prj/solution1/syn/verilog/hls_LK.v) 行"
else
    echo "✗ RTL代码生成失败"
fi

# 检查C仿真结果
RESULT_IMAGES=$(find . -name "output_*.jpg" | wc -l)
if [ $RESULT_IMAGES -gt 0 ]; then
    echo "✓ C仿真成功 - 找到 $RESULT_IMAGES 个结果图像"
    find . -name "output_*.jpg" -exec echo "  {}" \;
else
    echo "✗ C仿真可能失败 - 未找到结果图像"
fi

# 检查协同仿真
if [ -f "lk_prj/solution1/impl/report/hls_LK_cosim.rpt" ]; then
    echo "✓ 协同仿真完成"
    echo "  位置: lk_prj/solution1/impl/report/hls_LK_cosim.rpt"
else
    echo "✗ 协同仿真未完成或失败"
fi

# 显示综合结果摘要
if [ -f "lk_prj/solution1/syn/report/hls_LK_csynth.rpt" ]; then
    echo ""
    echo "=== 综合结果摘要 ==="
    grep -A 5 "Utilization Estimates" lk_prj/solution1/syn/report/hls_LK_csynth.rpt | head -10
fi

echo ""
echo "=== 检查完成 ==="
EOF

chmod +x check_results.sh
./check_results.sh
```

## 性能优化

在 AI 大模型助教的建议下，项目采用了多项优化策略：

- **数据打包**：将 2 个 8 位像素打包为 16 位数据，减少内存访问次数
- **数组分割**：使用 `#pragma HLS ARRAY_PARTITION` 优化并行访问
- **流水线设计**：通过 `#pragma HLS PIPELINE` 提高硬件吞吐量
- **定点计算**：采用定点数替代浮点数，平衡精度与资源消耗

### 1. 修改时钟频率

编辑 `run_hls.tcl`，修改时钟周期：

```tcl
# 默认10ns（100MHz）
create_clock -period 10 -name default

# 更高频率：5ns（200MHz）
create_clock -period 5 -name default

# 更低频率：20ns（50MHz）
create_clock -period 20 -name default
```

### 2. 修改目标器件

```tcl
# 默认Zynq-7020
set_part {xc7z020clg400-1}

# UltraScale+器件
set_part {xczu7ev-ffvc1156-2-e}

# Kintex器件
set_part {xc7k325tffg900-2}
```

### 3. 优化编译选项

```tcl
# 添加优化选项
add_files ../src/lk_hls.cpp -cflags "-std=c++14 -O3"

# 添加更多优化指令
config_compile -name_max_length 50
config_interface -m_axi_latency 0
config_schedule -enable_dsp_full_reg
```

## 软件光流算法测试

软件参考实现位于 `sw_lk/` 目录，用于对比验证。

### 测试准备（Python环境）

1. **生成测试数据**：
   ```bash
   cd sw_lk
   python create_test_images.py
   ```

2. **运行光流测试**：
   ```bash
   python lk.py
   ```

#### 预期结果

- **平均端点误差（AEE）**：约0.038568
- **平均角度误差（AAE）**：约0.365587°
- **整体准确率**：约89%

## 故障排除

### 1. 常见错误及解决方案

#### OpenCV路径错误
```bash
# 错误：opencv2/opencv.hpp: No such file or directory
# 解决：检查OpenCV安装路径
find ~/.conda/envs -name "opencv.hpp" 2>/dev/null
# 修改run_hls.tcl中的opencv_path变量
```

#### 权限错误
```bash
# 错误：Permission denied
# 解决：确保目录有写权限
chmod 755 /home/fyt/A/fpgachina25-amd/reference/LucasKanade/build_linux
```

#### 内存不足
```bash
# 错误：synthesis failed due to memory
# 解决：降低图像尺寸或优化算法参数
# 修改src/lk_define.h中的MAX_WIDTH、MAX_HEIGHT
```

### 2. 调试技巧

#### 启用详细日志
```tcl
# 在TCL脚本开头添加
config_debug -check all
config_compile -name_max_length 256
```

#### 分步验证
```bash
# 1. 先验证C仿真
vitis_hls -f csim_only.tcl

# 2. 再运行综合
vitis_hls -f synthesis_only.tcl

# 3. 最后协同仿真
vitis_hls -f cosim_only.tcl
```

#### 查看资源使用
```bash
# 综合后查看资源报告
grep -A 20 "Utilization Estimates" lk_prj/solution1/syn/report/hls_LK_csynth.rpt
```

## 高级使用

### 1. 自定义测试数据

```bash
# 替换测试图像
cp your_frame1.png ../test_data/frame00.png
cp your_frame2.png ../test_data/frame01.png

# 确保图像格式正确（灰度图像，尺寸在限制范围内）
```

### 2. 批量测试

```bash
# 创建批量测试脚本
cat > batch_test.sh << 'EOF'
#!/bin/bash
for freq in 5 10 15 20; do
    echo "Testing with ${freq}ns clock period..."
    sed -i "s/create_clock -period [0-9]*/create_clock -period ${freq}/" run_hls.tcl
    vitis_hls -f run_hls.tcl > test_${freq}ns.log 2>&1
    echo "Test completed for ${freq}ns"
done
EOF

chmod +x batch_test.sh
./batch_test.sh
```

### 3. 结果对比

```bash
# 提取关键性能指标
grep "Latency" lk_prj/solution1/syn/report/hls_LK_csynth.rpt
grep "DSP" lk_prj/solution1/syn/report/hls_LK_csynth.rpt
grep "BRAM" lk_prj/solution1/syn/report/hls_LK_csynth.rpt
```

## 技术支持

### 1. 官方文档
- [Vitis HLS用户指南](https://docs.xilinx.com/r/en-US/ug1399-vitis-hls)
- [HLS优化指南](https://docs.xilinx.com/r/en-US/ug1270-vitis-hls-opt)

### 2. HLS基础学习资源
- **推荐教程**：若需要HLS基础学习，可以参考【Vitis HLS教学，包括TCL脚本使用及图像处理（一）-哔哩哔哩】 https://b23.tv/mr5kJvm
- **适用对象**：HLS初学者、TCL脚本入门、图像处理算法实现
- **内容涵盖**：Vitis HLS基础操作、TCL自动化脚本编写、图像处理算法HLS实现

### 3. 社区资源
- [Xilinx论坛](https://forums.xilinx.com/)
- [GitHub示例项目](https://github.com/Xilinx/Vitis-HLS-Introductory-Examples)

### 3. 本项目相关
- 主要算法实现：`../src/lk_hls.cpp`
- 参数配置：`../src/lk_define.h`
- 测试程序：`../src/LKof_main.cpp`

---

## 总结

本指南提供了在Linux环境下运行Lucas-Kanade光流算法HLS实现的完整流程。通过自动化TCL脚本，可以一键完成从C仿真到RTL生成的全过程。如遇到问题，请按照故障排除部分的建议进行调试，或参考相关技术文档。

**关键文件**：
- `run_hls.tcl`：主要自动化脚本
- `vitis_hls.log`：运行日志，问题排查的重要依据
- `lk_prj/solution1/syn/report/`：性能和资源使用报告

**预期结果**：
- C仿真：验证算法功能正确性
- 综合：生成优化的RTL代码
- 协同仿真：验证RTL与C代码行为一致性
