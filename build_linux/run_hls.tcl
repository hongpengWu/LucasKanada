set CSIM 1
set CSYNTH 1
set COSIM 1
set VIVADO_SYN 1
set VIVADO_IMPL 1

set CUR_DIR [pwd]
set SCRIPT_DIR [file dirname [info script]]
set PROJ "lk_prj"
set SOLN "solution1"
set csynth_xml "$PROJ/$SOLN/syn/report/hls_LK_csynth.xml"
set cosim_rpt "$PROJ/$SOLN/sim/report/hls_LK_cosim.rpt"

puts "== 清理旧日志文件 =="
exec sh -c "rm -f flex*.log"
puts "== 开始执行脚本，时间: [clock format [clock seconds]] =="

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

puts "== 创建/打开HLS项目 =="
open_project $PROJ
puts "== 设置顶层函数 =="
set_top hls_LK
puts "== 添加设计源文件 =="
add_files ../src/lk_hls.cpp
add_files ../src/lk_define.h
puts "== 添加测试文件 =="
add_files -tb ../src/LKof_main.cpp -cflags "-I$opencv_include -std=c++14" -csimflags "-L$stdcxx_lib -Wl,-rpath,$stdcxx_lib -L$opencv_lib -Wl,-rpath,$opencv_lib -Wl,--allow-shlib-undefined $opencv_libs"
puts "== 打开/重置解决方案 =="
open_solution -reset $SOLN
puts "== 设置目标FPGA器件 =="
set_part {xc7z020clg400-1}
puts "== 创建时钟约束 =="
create_clock -period 10 -name default

if {$CSIM == 1} {
  puts "== 运行C仿真（CSIM） =="
  csim_design -ldflags "-L$stdcxx_lib -Wl,-rpath,$stdcxx_lib -L$opencv_lib -Wl,-rpath,$opencv_lib -Wl,--allow-shlib-undefined $opencv_libs"
}

if {$CSYNTH == 1} {
  puts "== 运行C综合（CSYNTH） =="
  csynth_design
}

if {$COSIM == 1} {
  puts "== 运行C/RTL协同仿真（COSIM） =="
  cosim_design -ldflags "-L$stdcxx_lib -Wl,-rpath,$stdcxx_lib -L$opencv_lib -Wl,-rpath,$opencv_lib -Wl,--allow-shlib-undefined $opencv_libs"
}

# Summarize Estimated Clock and Cosim Latency to compute T_exec
set est_clk ""
set lat_min ""
set lat_avg ""
set lat_max ""
set total_cycles ""

if {[file exists $csynth_xml]} {
  set fp [open $csynth_xml r]
  set data [read $fp]
  close $fp
  if {[regexp -line {<EstimatedClockPeriod>([0-9.]+)</EstimatedClockPeriod>} $data -> est]} {
    set est_clk $est
  }
}

if {[file exists $cosim_rpt]} {
  set fp [open $cosim_rpt r]
  set rpt [read $fp]
  close $fp
  if {[regexp -line {\|\s*Verilog\|\s*Pass\|\s*([0-9]+)\|\s*([0-9]+)\|\s*([0-9]+)\|.*\|\s*([0-9]+)\|} $rpt -> lat_min lat_avg lat_max total_cycles]} {
  }
}

set t_exec ""
if {![string equal $est_clk ""] && ![string equal $total_cycles ""]} {
  set t_exec [format "%.3f" [expr {$est_clk * $total_cycles}]]
}

file mkdir reports
set summary_file "reports/summary_T_exec.txt"
set sfp [open $summary_file w]
puts $sfp "EstimatedClockPeriod = $est_clk ns"
puts $sfp "TotalExecution(cycles) = $total_cycles cycles"
puts $sfp "T_exec = EstimatedClockPeriod × TotalExecution(cycles) = $t_exec ns"
close $sfp

set sol_report_dir "$PROJ/$SOLN/syn/report"
file mkdir $sol_report_dir
set summary_file2 "$sol_report_dir/summary_T_exec.txt"
set sfp2 [open $summary_file2 w]
puts $sfp2 "EstimatedClockPeriod = $est_clk ns"
puts $sfp2 "TotalExecution(cycles) = $total_cycles cycles"
puts $sfp2 "T_exec = EstimatedClockPeriod × TotalExecution(cycles) = $t_exec ns"
close $sfp2

puts "== 进入Vivado运行环节 =="
if {$VIVADO_SYN == 1} {
  export_design -flow syn -rtl verilog
}

if {$VIVADO_IMPL == 1} {
  export_design -flow impl -rtl verilog
}
exit
