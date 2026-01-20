
# ============ 清理环境阶段 ============
puts "== 清理旧日志文件 =="
exec sh -c "rm -f flex*.log"
puts "== 开始执行脚本，时间: [clock format [clock seconds]] =="


# ============ OpenCV环境配置阶段 ============
puts "== 配置OpenCV环境 =="
set opencv_path "/home/whp/anaconda3/envs/opencv_env"
set opencv_include "/usr/include/opencv4"
set opencv_lib "/lib/x86_64-linux-gnu"
set opencv_libs "-lopencv_imgcodecs -lopencv_imgproc -lopencv_core"
set stdcxx_lib "/lib/x86_64-linux-gnu"
set ::env(CC) "/usr/bin/gcc"
set ::env(CXX) "/usr/bin/g++"
set ::env(LD_LIBRARY_PATH) "$stdcxx_lib:$opencv_lib:$::env(LD_LIBRARY_PATH)"
set ::env(LD_PRELOAD) "/lib/x86_64-linux-gnu/libstdc++.so.6"


# ============ HLS项目配置阶段 ============
puts "== 创建/打开HLS项目 =="
open_project lk_prj
puts "== 设置顶层函数 =="
set_top hls_LK
puts "== 添加设计源文件 =="
add_files ../src/lk_hls.cpp
add_files ../src/lk_define.h
puts "== 添加测试文件 =="
add_files -tb ../src/LKof_main.cpp -cflags "-I$opencv_include -std=c++14" -csimflags "-L$stdcxx_lib -Wl,-rpath,$stdcxx_lib -L$opencv_lib -Wl,-rpath,$opencv_lib -Wl,--allow-shlib-undefined $opencv_libs"
puts "== 打开/重置解决方案 =="
open_solution -reset solution1
puts "== 设置目标FPGA器件 =="
set_part {xc7z020clg400-1}
puts "== 创建时钟约束 =="
create_clock -period 10 -name default


# ============ HLS流程执行阶段 ============
puts "== 运行C仿真（CSIM） =="
csim_design -ldflags "-L$stdcxx_lib -Wl,-rpath,$stdcxx_lib -L$opencv_lib -Wl,-rpath,$opencv_lib -Wl,--allow-shlib-undefined $opencv_libs"
puts "== 运行C综合（CSYNTH） =="
csynth_design
puts "== 运行C/RTL协同仿真（COSIM） =="
cosim_design -ldflags "-L$stdcxx_lib -Wl,-rpath,$stdcxx_lib -L$opencv_lib -Wl,-rpath,$opencv_lib -Wl,--allow-shlib-undefined $opencv_libs"
puts "== 脚本执行结束，退出Vitis HLS工具 =="
exit
