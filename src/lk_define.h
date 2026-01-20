
#include <stdio.h>      // 标准输入输出库，提供printf、scanf等函数
#include <stdlib.h>     // 标准库，提供malloc、free、exit等函数
#include <math.h>       // 数学库，提供sin、cos、sqrt等数学函数
#include <assert.h>     // 断言库，提供assert宏用于调试
#include <string.h>     // 字符串处理库，提供strcpy、strcmp等函数
#include <malloc.h>     // 内存分配库，提供动态内存管理函数

/* ******************************************************************************* */
// 架构选择的常量参数 - 这些参数决定了HLS设计的硬件资源需求和性能特征

#define MAX_PATH      10000  // 文件路径最大长度，用于文件I/O操作时的缓冲区大小
#define MAX_HEIGHT    398    // 图像最大高度（行数），决定了行缓冲区和垂直处理范围
#define MAX_WIDTH     594    // 图像最大宽度（列数），决定了列缓冲区和水平处理范围
                            // 注意：398x594 = 236,412像素，总内存需求约为236KB（8位像素）


#define BITS_PER_PIXEL   8  // 像素数据位宽，8位表示0-255的灰度值范围
                            // 原设计为12位，现改为8位以节省硬件资源
#define BITS_PER_COEFF   7  // 滤波器系数位宽，7位表示-64到+63的系数范围
                            // 用于5x5各向同性滤波器和导数计算滤波器

// 积分2D窗口大小配置 - 用于Lucas-Kanade光流算法的空间积分计算
// 窗口大小影响算法的精度和计算复杂度，更大的窗口提供更稳定的结果但需要更多资源
#define SIZE_5x5  // 当前选择5x5窗口，在精度和性能之间取得平衡
#define WINDOW_SIZE  5      // 5x5积分窗口，当前选择，平衡精度和性能
#define ACC_BITS     5      // 5位累加器，可处理5x5=25个像素的累加
#define FILTER_SIZE  5     // 滤波器2D窗口大小，用于各向同性滤波和空间导数计算
                           // 5x5窗口提供良好的噪声抑制和边缘检测能力
#define FILTER_OFFS   (FILTER_SIZE/2)  // 滤波器偏移量，用于边界处理
                                       // 值为2，表示滤波器中心到边缘的距离
#define WINDOW_OFFS   (WINDOW_SIZE/2)  // 积分窗口偏移量，用于边界处理
                                       // 值为2（对于5x5窗口），确保积分计算不越界

#define SUBPIX_BITS  6  // 亚像素精度位数，用于提高光流计算的精度
                        // 6位表示1/64像素精度，即运动向量可以表示1/64像素的位移
                        // 1表示1/2, 2表示1/4, 3表示1/8, 6表示1/64像素精度

/* ******************************************************************************* */
// 依赖于上述参数的参数(请勿修改) - 这些参数由上面的宏定义自动计算得出

const int hls_IMGSZ = (MAX_HEIGHT)*(MAX_WIDTH); // 图像总像素数，用于HLS pragma FIFO接口
                                                 // 值为398*594=236,412，决定FIFO缓冲区大小
const int hls_MIN_H = 120/2;                    // 最小图像高度，用于HLS pragma LOOP TRIPCOUNT
                                                // 值为60，用于循环优化和资源估计
const int hls_MIN_W = 188/2;                    // 最小图像宽度，用于HLS pragma LOOP TRIPCOUNT
                                                // 值为94，用于循环优化和资源估计
const int hls_MAX_H = (MAX_HEIGHT);             // 最大图像高度，用于HLS pragma LOOP TRIPCOUNT
                                                // 值为398，用于循环优化和资源估计
const int hls_MAX_W = (MAX_WIDTH);              // 最大图像宽度，用于HLS pragma LOOP TRIPCOUNT
                                                // 值为594，用于循环优化和资源估计
const int hls_WNDSZ = WINDOW_SIZE;              // 积分窗口大小，用于HLS pragma LOOP TRIPCOUNT
                                                // 值为5，用于循环优化和资源估计
const int HLS_STREAM_DEPTH = 10;                // 流深度，用于HLS pragma STREAM
                                                // 值为10，决定数据流缓冲区的深度
/* ******************************************************************************* */
// 数据类型定义 - 使用HLS任意宽度整数类型，优化硬件资源使用

#include "ap_int.h" // HLS任意宽度整数数据类型，提供ap_uint和ap_int模板类
                    // 允许定义任意位宽的整数类型，精确控制硬件资源使用

typedef ap_uint<BITS_PER_PIXEL  >      pix_t;      // 输入像素类型，8位无符号整数
                                                    // 表示0-255的灰度值，用于图像数据存储
typedef ap_int<BITS_PER_PIXEL*2>       dualpix_t;  // 双像素类型，16位有符号整数
                                                    // 将2个8位像素打包成单个16位字，节省内存带宽
typedef ap_int<    BITS_PER_PIXEL+1>   flt_t;      // 滤波后像素类型，9位有符号整数
                                                    // 用于局部导数计算，支持负值表示梯度方向
typedef ap_uint<3*(BITS_PER_PIXEL+1)>  p3dtyx_t;   // 3D导数打包类型，27位无符号整数
                                                    // 将3个9位导数打包，用于空间-时间导数存储
typedef ap_int< 2*(BITS_PER_PIXEL+1)>  sqflt_t;    // 平方滤波类型，18位有符号整数
                                                    // 用于局部导数的平方值，支持更大的数值范围
typedef ap_int<BITS_PER_COEFF>         coe_t;      // 滤波器系数类型，7位有符号整数
                                                    // 用于5x5滤波器的系数，范围-64到+63


#define  W_SUM      (2*(BITS_PER_PIXEL+1)+ACC_BITS)  // 累加器位宽计算，值为2*9+5=23位
                                                       // 考虑最坏情况下的累加需求，确保数值不溢出
typedef ap_int<W_SUM>    sum_t;                 // 积分累加器类型，23位有符号整数
                                                // 用于Lucas-Kanade算法中的空间积分计算
typedef ap_uint<5*W_SUM> p5sqflt_t;             // 5个平方值打包类型，115位无符号整数
                                                // 用于5个23位累加器的打包存储，优化内存访问

typedef ap_int<2*W_SUM>    sum2_t;              // 双倍累加器类型，46位有符号整数
                                                // 用于矩阵求逆中的中间计算，支持更大的数值范围
typedef ap_int<2*W_SUM+3>  det_t;               // 行列式类型，49位有符号整数
                                                // 用于矩阵求逆中的行列式计算，确保精度不丢失


typedef float  vec_t;    // 运动向量分量类型，32位浮点数
                         // 用于存储最终的光流向量Vx和Vy，提供高精度结果
typedef float  dout_t;   // 输出数据类型，32位浮点数
                         // 用于任何输出数据，确保与外部接口的兼容性
typedef float  mat_t;    // 中间结果类型，32位浮点数
                         // 用于调试和中间计算，仅在调试模式下使用

typedef struct ap_rgb {
    unsigned char  B;  // 蓝色分量，8位无符号整数，范围0-255
    unsigned char  G;  // 绿色分量，8位无符号整数，范围0-255
    unsigned char  R;  // 红色分量，8位无符号整数，范围0-255
  } RGB_t;            // RGB颜色结构体，用于彩色图像处理（当前项目主要使用灰度图）

typedef enum {INTEGER, HALF, QUARTER, EIGTH} subpix_t;  // 亚像素精度枚举类型
                                                         // INTEGER: 整数像素精度
                                                         // HALF: 1/2像素精度
                                                         // QUARTER: 1/4像素精度
                                                         // EIGTH: 1/8像素精度（注意拼写错误，应为EIGHTH）

typedef struct lk_vect {
   signed short int Vx;  // X方向运动向量，16位有符号整数
   signed short int Vy;  // Y方向运动向量，16位有符号整数
} lk_vect_st;           // Lucas-Kanade光流向量结构体，存储最终的运动估计结果

typedef union lk_union_vect {
	int val;              // 32位整数值，用于整体访问
	lk_vect_st packed;    // 打包的光流向量结构体，用于位操作
} lk_union_vect_t;       // 光流向量联合体，允许以不同方式访问相同数据

/* ******************************************************************************* */
// 算法选择的参数 - 这些参数影响Lucas-Kanade光流算法的精度和稳定性

const int     THRESHOLD = (WINDOW_SIZE)*(WINDOW_SIZE);    // 行列式阈值，值为5*5=25
                                                           // 用于检查2x2矩阵是否可逆，小于此值认为矩阵奇异
const int REF_THRESHOLD = (1.0f);                         // 参考阈值，浮点数1.0
                                                           // 用于与浮点数比较，确保数值稳定性

const int MAX_NUM_OF_WRONG_VECTORS = 1000;               // 允许的错误向量最大数量
                                                           // 用于统计和限制算法中的异常结果数量

/* ******************************************************************************* */



int hls_LK(unsigned short int inp1_img[MAX_HEIGHT*MAX_WIDTH],  // 第一帧输入图像，398x594像素数组
			unsigned short int inp2_img[MAX_HEIGHT*MAX_WIDTH],  // 第二帧输入图像，398x594像素数组
			signed short int vx_img[MAX_HEIGHT*MAX_WIDTH],     // X方向运动向量输出，398x594数组
			signed short int vy_img[MAX_HEIGHT*MAX_WIDTH],     // Y方向运动向量输出，398x594数组
		   unsigned short int height, unsigned short int width); // 实际图像高度和宽度，用于边界处理


