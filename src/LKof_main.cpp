#include <opencv2/opencv.hpp>  // OpenCV核心库，提供图像处理、矩阵运算、绘图等功能
#include <iostream>             // 标准输入输出库，提供cout、cin等流操作
#include <vector>               // STL向量容器，提供动态数组功能
#include <chrono>               // 时间库，提供高精度计时功能，用于性能测量
#include <fstream>
#include <limits.h>
#include <unistd.h>
#include "lk_define.h"          // 项目自定义头文件，包含所有算法参数和数据类型定义

using namespace cv;             // 使用OpenCV命名空间，简化代码编写
using namespace std;            // 使用标准命名空间，简化代码编写

// 将OpenCV Mat转换为hls_LK函数需要的格式 - 数据格式转换函数
// 输入参数：input - OpenCV的Mat类型图像，包含像素数据
// 输出参数：output - 指向unsigned short int数组的指针，用于存储转换后的数据
// 功能说明：将OpenCV的Mat格式图像数据转换为HLS函数期望的一维数组格式
void matToHlsFormat(const Mat& input, unsigned short int* output) {
    // 双重循环遍历图像的每个像素
    for (int i = 0; i < input.rows; i++) {        // 外层循环：遍历图像的行（高度方向）
        for (int j = 0; j < input.cols; j++) {    // 内层循环：遍历图像的列（宽度方向）
            // 将二维坐标(i,j)转换为一维数组索引，使用行优先存储方式
            // MAX_WIDTH是预定义的最大图像宽度，确保数组索引不越界
            // input.at<uchar>(i,j)获取Mat中位置(i,j)的8位无符号像素值
            // (unsigned short int)进行类型转换，确保数据类型匹配
            output[i * MAX_WIDTH + j] = (unsigned short int)input.at<uchar>(i, j);
        }
    }
}

// 将hls_LK函数输出转换为OpenCV Mat - 数据格式反向转换函数
// 输入参数：vx_data - X方向运动向量的一维数组指针
// 输入参数：vy_data - Y方向运动向量的一维数组指针
// 输入参数：height - 图像高度，用于创建Mat矩阵
// 输入参数：width - 图像宽度，用于创建Mat矩阵
// 输出参数：vx_mat - 引用传递的X方向运动向量Mat矩阵
// 输出参数：vy_mat - 引用传递的Y方向运动向量Mat矩阵
// 功能说明：将HLS函数返回的一维数组格式转换为OpenCV的Mat格式，便于后续处理和可视化
void hlsOutputToMat(const signed short int* vx_data, const signed short int* vy_data, 
                    int height, int width, Mat& vx_mat, Mat& vy_mat) {
    // 创建两个Mat矩阵，用于存储X和Y方向的运动向量
    // CV_16S表示16位有符号整数类型，与HLS函数的输出类型匹配
    vx_mat = Mat(height, width, CV_16S);  // 创建X方向运动向量矩阵
    vy_mat = Mat(height, width, CV_16S);  // 创建Y方向运动向量矩阵
    
    // 双重循环遍历每个像素位置
    for (int i = 0; i < height; i++) {        // 外层循环：遍历图像的行
        for (int j = 0; j < width; j++) {     // 内层循环：遍历图像的列
            // 将一维数组索引转换为二维Mat矩阵位置
            // 使用行优先存储方式，与matToHlsFormat函数保持一致
            // MAX_WIDTH确保索引计算正确，即使实际图像宽度小于最大宽度
            vx_mat.at<short>(i, j) = vx_data[i * MAX_WIDTH + j];  // 复制X方向运动向量
            vy_mat.at<short>(i, j) = vy_data[i * MAX_WIDTH + j];  // 复制Y方向运动向量
        }
    }
}

// 可视化光流向量并保存 - 光流结果可视化函数
// 输入参数：img1 - 第一帧图像，用作可视化背景
// 输入参数：img2 - 第二帧图像，用于对比（当前未使用）
// 输入参数：vx_mat - X方向运动向量的Mat矩阵
// 输入参数：vy_mat - Y方向运动向量的Mat矩阵
// 输入参数：filename - 输出图像的文件名
// 功能说明：在图像上绘制光流向量箭头，直观显示运动方向和大小，并保存为图像文件
void visualizeOpticalFlow(const Mat& img1, const Mat& img2, 
                         const Mat& vx_mat, const Mat& vy_mat, 
                         const string& filename) {
    // 克隆第一帧图像作为可视化背景，避免修改原始图像
    Mat flow_vis = img1.clone();
    // 如果输入是灰度图像，转换为彩色图像以便绘制彩色箭头
    if (flow_vis.channels() == 1) {
        cvtColor(flow_vis, flow_vis, COLOR_GRAY2BGR);  // 灰度转BGR彩色
    }
    
    // 绘制光流向量 - 每隔8个像素绘制一个箭头，避免图像过于密集
    for (int i = 0; i < flow_vis.rows; i += 8) {        // 外层循环：每隔8行绘制
        for (int j = 0; j < flow_vis.cols; j += 8) {     // 内层循环：每隔8列绘制
            // 将整数运动向量转换为浮点数，考虑亚像素精度
            // SUBPIX_BITS=6，表示1/64像素精度，所以除以2^6=64
            float vx = vx_mat.at<short>(i, j) / (1 << SUBPIX_BITS); // X方向运动向量（像素/帧）
            float vy = vy_mat.at<short>(i, j) / (1 << SUBPIX_BITS); // Y方向运动向量（像素/帧）
            
            // 只绘制有意义的运动向量，过滤掉噪声和静止区域
            // 阈值0.1像素/帧，小于此值的运动认为是噪声
            if (abs(vx) > 0.1 || abs(vy) > 0.1) {
                Point2f pt1(j, i);                    // 箭头起点：当前像素位置
                Point2f pt2(j + vx, i + vy);          // 箭头终点：当前位置加上运动向量
                // 绘制绿色箭头，线宽1像素，箭头大小8像素，透明度0.3
                arrowedLine(flow_vis, pt1, pt2, Scalar(0, 255, 0), 1, 8, 0, 0.3);
            }
        }
    }
    
    // 保存可视化结果到指定文件
    imwrite(filename, flow_vis);
}

bool copyFileBinary(const std::string& src, const std::string& dst) {
    std::ifstream in(src, std::ios::binary);
    if (!in) {
        return false;
    }
    std::ofstream out(dst, std::ios::binary);
    if (!out) {
        return false;
    }
    out << in.rdbuf();
    return out.good();
}

int main() {
    // 程序主函数 - HLS Lucas-Kanade光流算法测试程序
    // 功能说明：加载两帧图像，调用HLS光流算法，可视化结果并保存
    cout << "HLS Lucas-Kanade Optical Flow Test" << endl;  // 打印程序标题
    cout << "==================================" << endl;    // 打印分隔线
    
    std::string base_dir;
    std::string current_cwd;
    char cwd_buf[PATH_MAX];
    if (getcwd(cwd_buf, sizeof(cwd_buf)) != nullptr) {
        current_cwd = cwd_buf;
        size_t pos = current_cwd.rfind("/LucasKanade");
        if (pos != std::string::npos) {
            base_dir = current_cwd.substr(0, pos + std::string("/LucasKanade").size());
        }
    }

    string img1_path = base_dir.empty() ? "test_data/frame00.png" : base_dir + "/test_data/frame00.png";
    string img2_path = base_dir.empty() ? "test_data/frame01.png" : base_dir + "/test_data/frame01.png";  // 测试图像路径设置 - 指定输入图像文件的完整路径

    // 加载测试图像 - 使用OpenCV读取图像文件
    Mat img1 = imread(img1_path, IMREAD_GRAYSCALE);  // 以灰度模式读取第一帧图像
    Mat img2 = imread(img2_path, IMREAD_GRAYSCALE);  // 以灰度模式读取第二帧图像
    
    // 图像加载错误处理 - 检查图像是否成功加载
    if (img1.empty() || img2.empty()) {
        cout << "Error: Could not load test images!" << endl;  // 打印错误信息
        cout << "Trying alternative paths..." << endl;         // 提示尝试备用路径
        
        // 尝试其他路径 - 使用相对路径作为备用方案
        img1_path = base_dir.empty() ? "test_data/frame07.png" : base_dir + "/test_data/frame07.png";  // 相对路径：第一帧图像
        img2_path = base_dir.empty() ? "test_data/frame08.png" : base_dir + "/test_data/frame08.png";  // 相对路径：第二帧图像
        img1 = imread(img1_path, IMREAD_GRAYSCALE);  // 重新尝试加载第一帧
        img2 = imread(img2_path, IMREAD_GRAYSCALE);  // 重新尝试加载第二帧
        
        // 如果仍然无法加载图像，创建合成测试图像
        if (img1.empty() || img2.empty()) {
            cout << "Creating synthetic test images..." << endl;  // 提示创建合成图像
            
            // 创建合成测试图像 - 生成人工测试数据
            img1 = Mat(388, 584, CV_8UC1, Scalar(128));  // 创建388x584的灰度图像，初始值128
            img2 = Mat(388, 584, CV_8UC1, Scalar(128));  // 创建388x584的灰度图像，初始值128
            
            // 添加一些运动模式 - 在图像中创建人工运动模式用于测试
            for (int i = 0; i < img1.rows; i++) {        // 外层循环：遍历图像的行
                for (int j = 0; j < img1.cols; j++) {    // 内层循环：遍历图像的列
                    if (j < img1.cols - 10) {            // 边界检查：避免在图像边缘创建模式
                        // 第一帧：使用正弦和余弦函数创建纹理模式
                        img1.at<uchar>(i, j) = 128 + 50 * sin(i * 0.1) * cos(j * 0.1);
                        // 第二帧：在X方向偏移2个像素，模拟水平运动
                        img2.at<uchar>(i, j) = 128 + 50 * sin(i * 0.1) * cos((j + 2) * 0.1);
                    }
                }
            }
        }
    }
    
    
    // 图像尺寸处理 - 使用原始图像尺寸
    int height = img1.rows;  // 使用原始图像高度
    int width = img1.cols;   // 使用原始图像宽度
    
    // 打印图像尺寸信息 - 显示图像尺寸和最大允许尺寸
    cout << "Image size: " << width << "x" << height << endl;                           // 图像尺寸
    cout << "Max allowed size: " << MAX_WIDTH << "x" << MAX_HEIGHT << endl;              // 最大允许尺寸（来自头文件）
    
    // 分配内存 - 为HLS算法分配输入和输出数组
    unsigned short int* inp1_img = new unsigned short int[MAX_HEIGHT * MAX_WIDTH];  // 第一帧输入图像数组
    unsigned short int* inp2_img = new unsigned short int[MAX_HEIGHT * MAX_WIDTH];  // 第二帧输入图像数组
    signed short int* vx_img = new signed short int[MAX_HEIGHT * MAX_WIDTH];        // X方向运动向量输出数组
    signed short int* vy_img = new signed short int[MAX_HEIGHT * MAX_WIDTH];        // Y方向运动向量输出数组
    
    // 初始化数组 - 将所有数组元素初始化为0，确保数据一致性
    memset(inp1_img, 0, MAX_HEIGHT * MAX_WIDTH * sizeof(unsigned short int));  // 初始化第一帧图像数组
    memset(inp2_img, 0, MAX_HEIGHT * MAX_WIDTH * sizeof(unsigned short int));  // 初始化第二帧图像数组
    memset(vx_img, 0, MAX_HEIGHT * MAX_WIDTH * sizeof(signed short int));      // 初始化X方向运动向量数组
    memset(vy_img, 0, MAX_HEIGHT * MAX_WIDTH * sizeof(signed short int));      // 初始化Y方向运动向量数组
    
    // 转换图像格式 - 将OpenCV的Mat格式转换为HLS函数期望的数组格式
    matToHlsFormat(img1, inp1_img);  // 转换第一帧图像数据格式
    matToHlsFormat(img2, inp2_img);  // 转换第二帧图像数据格式
    
    cout << "Running HLS Lucas-Kanade optical flow..." << endl;  // 提示开始执行光流算法
    
    // 计时开始 - 使用高精度时钟测量算法执行时间
    auto start_time = chrono::high_resolution_clock::now();  // 记录开始时间点
    
    // 调用HLS Lucas-Kanade函数 - 执行核心光流算法
    int num_valid_vectors = hls_LK(inp1_img, inp2_img, vx_img, vy_img, height, width);  // 调用HLS光流函数
    
    // 计时结束 - 计算算法执行时间
    auto end_time = chrono::high_resolution_clock::now();  // 记录结束时间点
    auto duration = chrono::duration_cast<chrono::microseconds>(end_time - start_time);  // 计算时间差（微秒）
    
    // 输出处理结果和性能信息
    cout << "Processing completed!" << endl;  // 提示处理完成
    cout << "Valid motion vectors found: " << num_valid_vectors << endl;  // 输出有效运动向量数量
    cout << "Processing time: " << duration.count() << " microseconds" << endl;  // 输出处理时间（微秒）
    cout << "Processing time: " << duration.count() / 1000.0 << " milliseconds" << endl;  // 输出处理时间（毫秒）
    
    // 转换输出格式 - 将HLS函数的输出数组转换为OpenCV的Mat格式
    Mat vx_mat, vy_mat;  // 声明运动向量Mat对象
    hlsOutputToMat(vx_img, vy_img, height, width, vx_mat, vy_mat);  // 转换数据格式
    
    // 保存结果 - 将处理结果保存为图像文件
    cout << "\nSaving results..." << endl;  // 提示开始保存结果
    
    // 保存原始图像 - 用于对比和验证
    // 创建结果文件夹（如果不存在）
    system("mkdir -p result");
    imwrite("result/output_img1_original.jpg", img1);        // 保存第一帧图像
    imwrite("result/output_img2_original.jpg", img2);        // 保存第二帧图像

    // 保存光流可视化 - 生成带箭头的光流可视化图像
    visualizeOpticalFlow(img1, img2, vx_mat, vy_mat, "result/output_optical_flow.jpg");  // 保存光流可视化结果

    // 保存运动向量分量 - 将运动向量转换为可视化图像
    Mat vx_display, vy_display;  // 声明显示用的Mat对象
    vx_mat.convertTo(vx_display, CV_8U, 0.1, 128);  // 将X方向运动向量转换为8位图像，缩放因子0.1，偏移128
    vy_mat.convertTo(vy_display, CV_8U, 0.1, 128);  // 将Y方向运动向量转换为8位图像，缩放因子0.1，偏移128

    imwrite("result/output_vx.jpg", vx_display);  // 保存X方向运动向量图像
    imwrite("result/output_vy.jpg", vy_display);  // 保存Y方向运动向量图像

    cout << "Results saved to result/output_*.jpg files" << endl;  // 提示所有结果已保存

    if (!current_cwd.empty()) {
        if (current_cwd.find("sim/wrapc_pc") != std::string::npos) {
            size_t pos = current_cwd.rfind("/LucasKanade");
            if (pos != std::string::npos) {
                std::string base = current_cwd.substr(0, pos + std::string("/LucasKanade").size());
                std::string src = current_cwd + "/result/output_optical_flow.jpg";
                std::string dst_dir = base + "/result";
                std::string dst = dst_dir + "/output_optical_flow.jpg";
                std::string cmd = "mkdir -p \"" + dst_dir + "\"";
                system(cmd.c_str());
                copyFileBinary(src, dst);
            }
        }
    }
    
    // 清理内存 - 释放动态分配的内存，防止内存泄漏
    delete[] inp1_img;  // 释放第一帧输入图像数组内存
    delete[] inp2_img;  // 释放第二帧输入图像数组内存
    delete[] vx_img;    // 释放X方向运动向量数组内存
    delete[] vy_img;    // 释放Y方向运动向量数组内存
    
    cout << "Test completed successfully!" << endl;  // 提示测试成功完成
    
    return 0;  // 程序正常结束，返回0表示成功
} 
