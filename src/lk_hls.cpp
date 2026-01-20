#include "lk_define.h"  // 包含项目自定义头文件，定义所有算法参数、数据类型和常量
dualpix_t hls_isotropic_kernel(dualpix_t window[FILTER_SIZE*FILTER_SIZE])  // 各向同性滤波器核心函数
// 函数功能：对5x5窗口内的双像素数据进行各向同性滤波处理
// 输入参数：window - 5x5窗口的双像素数据数组，每个元素包含两个8位像素
// 返回值：dualpix_t - 滤波后的双像素结果，保持输入输出格式一致
{

	// 5x5各向同性滤波器系数矩阵 - 高斯滤波器的离散近似实现
	// 该系数矩阵具有中心对称性，提供平滑的噪声抑制效果
	// 系数总和为256，便于后续归一化处理（除以256）
	const coe_t coeff[FILTER_SIZE][FILTER_SIZE] = {
		{ 1,    4,    6,    4,    1},  // 第0行：边缘系数较小，中心系数较大
		{ 4,   16,   24,   16,    4},  // 第1行：逐渐增大到中心
		{ 6,   24,   36,   24,    6},  // 第2行：中心行，最大系数36位于中心
		{ 4,   16,   24,   16,    4},  // 第3行：对称递减
		{ 1,    4,    6,    4,    1}   // 第4行：最外层，最小系数
	};

	// 局部变量声明 - 用于滤波计算的中间变量和结果存储
	int accum1 = 0;              // 第一个像素的累加器，用于存储滤波过程中的中间结果
	int normalized_accum1;        // 第一个像素的归一化累加结果，除以256后的值
	int accum2 = 0;              // 第二个像素的累加器，用于存储滤波过程中的中间结果
	int normalized_accum2;        // 第二个像素的归一化累加结果，除以256后的值
	pix_t pix1, pix2, final_val1, final_val2;  // 像素变量：输入像素、输出像素
	unsigned char i, j;           // 循环计数器，用于遍历5x5滤波器窗口
	dualpix_t two_pixels;        // 双像素结果变量，用于存储最终的滤波输出

	// 执行2D卷积运算 - 双重循环遍历5x5滤波器窗口的每个位置
	L1:for (i = 0; i < FILTER_SIZE; i++) {        // 外层循环：遍历滤波器窗口的行
		L2:for (j = 0; j < FILTER_SIZE; j++) {    // 内层循环：遍历滤波器窗口的列
			two_pixels = window[i*FILTER_SIZE+j];   // 获取当前位置的双像素数据
			pix1 = two_pixels(  BITS_PER_PIXEL-1,              0);  // 提取第一个像素（低8位）
			pix2 = two_pixels(2*BITS_PER_PIXEL-1, BITS_PER_PIXEL);  // 提取第二个像素（高8位）

			// 执行像素值与滤波器系数的乘法运算
			// 使用ap_int类型确保位宽正确，避免溢出
			ap_int<(BITS_PER_COEFF+BITS_PER_PIXEL)> loc_mult1 = ( pix1 * coeff[i][j]);  // 第一个像素的滤波计算
			ap_int<(BITS_PER_COEFF+BITS_PER_PIXEL)> loc_mult2 = ( pix2 * coeff[i][j]);  // 第二个像素的滤波计算

			// 累加滤波结果到对应的累加器
			accum1 = accum1 + loc_mult1;  // 累加第一个像素的滤波结果
			accum2 = accum2 + loc_mult2;  // 累加第二个像素的滤波结果

		}
	}

	// 归一化累加结果 - 将累加值除以256（滤波器系数总和）得到最终的滤波结果
	normalized_accum1 = accum1 / 256;  // 第一个像素的归一化处理，除以滤波器系数总和
	normalized_accum2 = accum2 / 256;  // 第二个像素的归一化处理，除以滤波器系数总和
	final_val1 = (pix_t) normalized_accum1;  // 类型转换，将归一化结果转换为8位像素类型
	final_val2 = (pix_t) normalized_accum2;  // 类型转换，将归一化结果转换为8位像素类型

	// 打包两个滤波后的像素值到双像素结构中
	two_pixels(  BITS_PER_PIXEL-1,              0) = final_val1;  // 将第一个像素存储到低8位
	two_pixels(2*BITS_PER_PIXEL-1, BITS_PER_PIXEL) = final_val2;  // 将第二个像素存储到高8位

	return two_pixels;  // 返回滤波后的双像素结果
}
void hls_twoIsotropicFilters(unsigned short int inp1_img[MAX_HEIGHT*MAX_WIDTH], unsigned short int  inp2_img[MAX_HEIGHT*MAX_WIDTH],
						 	 pix_t out1_img[MAX_HEIGHT*MAX_WIDTH], pix_t out2_img[MAX_HEIGHT*MAX_WIDTH],
								 unsigned short int height, unsigned short int width)
// 双图像各向同性滤波函数 - 同时对两帧图像进行相同的滤波处理
// 输入参数：inp1_img, inp2_img - 两帧输入图像的一维数组
// 输出参数：out1_img, out2_img - 滤波后的两帧输出图像
// 参数：height, width - 图像的高度和宽度
{
    // 循环控制变量声明
    unsigned short int row, col;  // 行和列计数器，用于遍历图像的每个像素位置

    // 滤波处理相关的局部变量
    pix_t filt_out1, filt_out2, pix1, pix2;  // 滤波输出和输入像素变量
    dualpix_t two_pixels;                     // 双像素数据，用于处理两个像素

    // 滤波器窗口相关数组 - 用于存储当前处理窗口的数据
    dualpix_t pixels[FILTER_SIZE];            // 当前行的5个双像素数据
    #pragma HLS ARRAY_PARTITION variable=pixels complete dim=0  // HLS优化：完全分割数组以提高并行性

    dualpix_t window[FILTER_SIZE*FILTER_SIZE];  // 5x5滤波器窗口，存储25个双像素
    #pragma HLS ARRAY_PARTITION variable=window complete dim=0  // HLS优化：完全分割窗口数组

    // 行缓冲区 - 用于存储前几行的数据，实现滑动窗口处理
    // 静态变量确保数据在函数调用之间保持，提高处理效率
    static dualpix_t lpf_lines_buffer[FILTER_SIZE][MAX_WIDTH];  // 5行x最大宽度的行缓冲区
    #pragma HLS ARRAY_PARTITION variable=lpf_lines_buffer complete dim=1  // HLS优化：按行完全分割

    // 主处理循环 - 双重循环遍历图像的每个像素位置进行滤波处理
    L1: for(row = 0; row < height+FILTER_OFFS; row++)  // 外层循环：遍历图像的行，包括边界偏移
    {
        #pragma HLS LOOP_TRIPCOUNT  min=hls_MIN_H max=hls_MAX_H  // HLS优化：指定循环次数范围
        L2: for(col = 0; col < width+FILTER_OFFS; col++)  // 内层循环：遍历图像的列，包括边界偏移
        {
            #pragma HLS PIPELINE  // HLS优化：流水线处理，提高吞吐量
            #pragma HLS LOOP_TRIPCOUNT min=hls_MIN_W max=hls_MAX_W  // HLS优化：指定内层循环次数范围

            // 行缓冲区更新 - 将行缓冲区中的数据向上移动一行，为新数据腾出空间
            if(col < width)  // 边界检查：确保列索引在有效范围内
                for(unsigned char ii = 0; ii < FILTER_SIZE-1; ii++)  // 循环更新前4行数据
                {
                    pixels[ii] = lpf_lines_buffer[ii][col] = lpf_lines_buffer[ii+1][col];  // 数据向上移动一行
                }

            // 新数据加载 - 从输入图像中读取当前像素位置的数据并打包
            if((col < width) && (row < height))  // 边界检查：确保像素位置在有效图像范围内
            {
                pix1 = (pix_t) inp1_img[row*MAX_WIDTH+col];  // 从第一帧图像读取当前像素
                pix2 = (pix_t) inp2_img[row*MAX_WIDTH+col];  // 从第二帧图像读取当前像素
                two_pixels(2*BITS_PER_PIXEL-1, BITS_PER_PIXEL) = pix2;  // 将第二帧像素存储到高8位
                two_pixels(  BITS_PER_PIXEL-1,              0) = pix1;  // 将第一帧像素存储到低8位

                pixels[FILTER_SIZE-1] = lpf_lines_buffer[FILTER_SIZE-1][col] = two_pixels;  // 将打包的双像素存储到行缓冲区
            }
            // 滤波器窗口更新 - 将窗口中的数据向左移动一列，为新列数据腾出空间
            L3:for(unsigned char ii = 0; ii < FILTER_SIZE; ii++)  // 外层循环：遍历窗口的每一行
                L4:for(unsigned char jj = 0; jj < FILTER_SIZE-1; jj++)  // 内层循环：遍历窗口的前4列
                {
                    window[ii*FILTER_SIZE+jj] = window[ii*FILTER_SIZE+jj+1];  // 数据向左移动一列
                }
            // 新列数据加载 - 将当前行的5个双像素数据加载到窗口的最右列
            L5:for(unsigned char ii = 0; ii < FILTER_SIZE; ii++)  // 循环遍历窗口的每一行
            {
                window[ii*FILTER_SIZE+FILTER_SIZE-1] = pixels[ii];  // 将行缓冲区中的数据加载到窗口最右列
            }
            
            // 滤波处理 - 对有效的像素位置执行各向同性滤波
            if ( (row >= FILTER_OFFS) & (col >= FILTER_OFFS) &  (row < height)  & (col< width) )  // 边界检查：确保有完整的5x5窗口
            {
                   two_pixels = hls_isotropic_kernel(window);  // 调用各向同性滤波核心函数
                   filt_out1 = two_pixels(  BITS_PER_PIXEL-1,              0);  // 提取第一个滤波结果（低8位）
                   filt_out2 = two_pixels(2*BITS_PER_PIXEL-1, BITS_PER_PIXEL);  // 提取第二个滤波结果（高8位）
            }
            else  // 边界处理：对于边界像素，输出零值
            {
                   filt_out1 = 0;  // 第一个像素输出零值
                   filt_out2 = 0;  // 第二个像素输出零值
            }
            // 结果输出 - 将滤波结果存储到输出图像数组中
            if ( (row >= FILTER_OFFS) & (col >= FILTER_OFFS))   // 边界检查：确保输出位置有效
            {
                   out1_img[(row-FILTER_OFFS)*MAX_WIDTH+(col-FILTER_OFFS)] = (filt_out1);  // 存储第一帧滤波结果
                   out2_img[(row-FILTER_OFFS)*MAX_WIDTH+(col-FILTER_OFFS)] = (filt_out2);  // 存储第二帧滤波结果
            }
        } 
    } 

}  // 双图像各向同性滤波函数结束

void hls_derivatives_kernel(dualpix_t window[FILTER_SIZE*FILTER_SIZE], flt_t &Ix, flt_t &Iy, flt_t &It)
// 空间-时间导数计算核心函数 - 计算图像的空间梯度（X、Y方向）和时间导数
// 输入参数：window - 5x5窗口的双像素数据，包含两帧图像的信息
// 输出参数：Ix - X方向空间导数（水平梯度）
// 输出参数：Iy - Y方向空间导数（垂直梯度）
// 输出参数：It - 时间导数（时间梯度）
{
	
	// 5x5 Y方向导数滤波器系数 - 实现Sobel算子的垂直梯度检测
	// 滤波器模式：[-1 8 0 -8 1]^T，用于检测垂直边缘
	// 该系数矩阵是MATLAB中常用的边缘检测滤波器
	const coe_t y_coeff[FILTER_SIZE][FILTER_SIZE] = {
		{ 0,    0,    1,    0,    0},  // 第0行：顶部边缘检测
		{ 0,    0,   -8,    0,    0},  // 第1行：负权重，增强对比度
		{ 0,    0,    0,    0,    0},  // 第2行：中心行，权重为零
		{ 0,    0,    8,    0,    0},  // 第3行：正权重，增强对比度
		{ 0,    0,   -1,    0,    0}   // 第4行：底部边缘检测
	};

	// 5x5 X方向导数滤波器系数 - 实现Sobel算子的水平梯度检测
	// 滤波器模式：[-1 8 0 -8 1]，用于检测水平边缘
	// 该系数矩阵是MATLAB中常用的边缘检测滤波器
	const coe_t x_coeff[FILTER_SIZE][FILTER_SIZE] = {
		{ 0,    0,    0,    0,    0},  // 第0行：无权重
		{ 0,    0,    0,    0,    0},  // 第1行：无权重
		{ 1,   -8,    0,    8,   -1},  // 第2行：中心行，水平梯度检测
		{ 0,    0,    0,    0,    0},  // 第3行：无权重
		{ 0,    0,    0,    0,    0}   // 第4行：无权重
	};

	// 局部变量声明 - 用于导数计算的中间变量和结果存储
	int accum_x = 0;              // X方向导数累加器，用于存储水平梯度的累加结果
	int accum_y = 0;              // Y方向导数累加器，用于存储垂直梯度的累加结果

	dualpix_t two_pix;            // 双像素数据，用于处理窗口中的像素对
	pix_t pix1, pix2;             // 单个像素变量，分别对应两帧图像
	int normalized_accum_x, normalized_accum_y;  // 归一化后的累加结果
	flt_t final_val_x, final_val_y, final_val_t;  // 最终的导数结果：X、Y、时间导数
	unsigned char i, j;           // 循环计数器，用于遍历5x5滤波器窗口

	// 执行2D卷积运算 - 双重循环遍历5x5滤波器窗口计算空间导数
	L1:for (i = 0; i < FILTER_SIZE; i++)   // 外层循环：遍历滤波器窗口的行
    {
		L2:for (j = 0; j < FILTER_SIZE; j++)   // 内层循环：遍历滤波器窗口的列
		{
			two_pix = window[i*FILTER_SIZE+j];   // 获取当前位置的双像素数据
			pix1 = two_pix(2*BITS_PER_PIXEL-1, BITS_PER_PIXEL);  // 提取第一帧像素（高8位）
			pix2 = two_pix(  BITS_PER_PIXEL-1, 0);                // 提取第二帧像素（低8位）

			// 计算X方向空间导数（水平梯度）
			signed short int loc_mult_x = ( pix1 * x_coeff[i][j]);   // 像素值与X方向滤波器系数相乘
			accum_x = accum_x + loc_mult_x;                          // 累加到X方向累加器
			
			// 计算Y方向空间导数（垂直梯度）
			signed short int loc_mult_y = ( pix1 * y_coeff[i][j]);   // 像素值与Y方向滤波器系数相乘
			accum_y = accum_y + loc_mult_y;                          // 累加到Y方向累加器

			// 计算时间导数 - 在窗口中心位置计算两帧之间的差异
			if ( (i==2)&(j==2) )  // 检查是否为窗口中心位置(2,2)
				final_val_t = pix2 - pix1;  // 时间导数 = 第二帧像素 - 第一帧像素
		}
	}

	// 归一化累加结果 - 将累加值除以12（滤波器系数的归一化因子）得到最终的导数结果
	normalized_accum_x = accum_x / 12;  // X方向导数的归一化处理
	normalized_accum_y = accum_y / 12;  // Y方向导数的归一化处理

	// 类型转换 - 将归一化结果转换为浮点数类型，便于后续计算
	final_val_x = (flt_t) normalized_accum_x;  // 转换为浮点数类型的X方向导数
	final_val_y = (flt_t) normalized_accum_y;  // 转换为浮点数类型的Y方向导数

	// 输出结果赋值 - 将计算得到的导数结果传递给输出参数
	Ix = final_val_x;  // 输出X方向空间导数（水平梯度）
	Iy = final_val_y;  // 输出Y方向空间导数（垂直梯度）
	It = final_val_t;  // 输出时间导数（时间梯度）

}  // 空间-时间导数计算核心函数结束
void hls_SpatialTemporalDerivatives(pix_t   inp1_img[MAX_HEIGHT*MAX_WIDTH],
    pix_t   inp2_img[MAX_HEIGHT*MAX_WIDTH],
    flt_t out_Ix_img[MAX_HEIGHT*MAX_WIDTH], 
    flt_t out_Iy_img[MAX_HEIGHT*MAX_WIDTH], 
    flt_t out_It_img[MAX_HEIGHT*MAX_WIDTH], 
    unsigned short int height, unsigned short int width)
// 空间-时间导数计算函数 - 对整个图像进行空间和时间导数的计算
// 输入参数：inp1_img, inp2_img - 两帧滤波后的输入图像
// 输出参数：out_Ix_img - X方向空间导数输出图像（水平梯度）
// 输出参数：out_Iy_img - Y方向空间导数输出图像（垂直梯度）
// 输出参数：out_It_img - 时间导数输出图像（时间梯度）
// 参数：height, width - 图像的高度和宽度
{

// 循环控制变量声明
unsigned short int row, col;  // 行和列计数器，用于遍历图像的每个像素位置

// 滤波输出变量声明
flt_t filt_out_x, filt_out_y, filt_out_t;  // X、Y方向空间导数和时间导数的滤波输出

// 导数计算相关的数据结构和缓冲区
dualpix_t two_pix, two_pixel[FILTER_SIZE];  // 双像素数据：单个双像素和一行双像素数组
dualpix_t deriv_window[FILTER_SIZE*FILTER_SIZE];  // 5x5导数计算窗口，存储25个双像素

// 静态行缓冲区 - 用于存储前几行的数据，实现滑动窗口处理
static dualpix_t deriv_lines_buffer[FILTER_SIZE][MAX_WIDTH];  // 5行x最大宽度的导数行缓冲区
#pragma HLS ARRAY_PARTITION variable=deriv_lines_buffer complete dim=1  // HLS优化：按行完全分割数组

// 主处理循环 - 双重循环遍历图像的每个像素位置进行导数计算
L1: for(row = 0; row < height+FILTER_OFFS; row++)  // 外层循环：遍历图像的行，包括边界偏移
{
#pragma HLS LOOP_TRIPCOUNT  min=hls_MIN_H max=hls_MAX_H  // HLS优化：指定循环次数范围
L2: for(col = 0; col < width+FILTER_OFFS; col++)  // 内层循环：遍历图像的列，包括边界偏移
{
#pragma HLS PIPELINE  // HLS优化：流水线处理，提高吞吐量
#pragma HLS LOOP_TRIPCOUNT min=hls_MIN_W max=hls_MAX_W  // HLS优化：指定内层循环次数范围

// 行缓冲区更新 - 将行缓冲区中的数据向上移动一行，为新数据腾出空间
if(col < width)  // 边界检查：确保列索引在有效范围内
L3:for(unsigned char ii = 0; ii < FILTER_SIZE-1; ii++)  // 循环更新前4行数据
{
two_pixel[ii] = deriv_lines_buffer[ii][col] = deriv_lines_buffer[ii+1][col];  // 数据向上移动一行
}

// 新数据加载 - 从输入图像中读取当前像素位置的数据并打包为双像素格式
if((col < width) && (row < height))  // 边界检查：确保像素位置在有效图像范围内
{
two_pix(2*BITS_PER_PIXEL-1, BITS_PER_PIXEL) = inp1_img[row*MAX_WIDTH+col];  // 将第一帧像素存储到高8位
two_pix(  BITS_PER_PIXEL-1,              0) = inp2_img[row*MAX_WIDTH+col];  // 将第二帧像素存储到低8位

two_pixel[FILTER_SIZE-1] = deriv_lines_buffer[FILTER_SIZE-1][col] = two_pix;  // 将打包的双像素存储到行缓冲区
}

// 导数窗口更新 - 将窗口中的数据向左移动一列，为新列数据腾出空间
L4:for(unsigned char ii = 0; ii < FILTER_SIZE; ii++)  // 外层循环：遍历窗口的每一行
L5:for(unsigned char jj = 0; jj < FILTER_SIZE-1; jj++)  // 内层循环：遍历窗口的前4列
{
deriv_window[ii*FILTER_SIZE+jj] = deriv_window[ii*FILTER_SIZE+jj+1];  // 数据向左移动一列
}

// 新列数据加载 - 将当前行的5个双像素数据加载到窗口的最右列
L6:for(unsigned char ii = 0; ii < FILTER_SIZE; ii++)  // 循环遍历窗口的每一行
{
deriv_window[ii*FILTER_SIZE+FILTER_SIZE-1] = two_pixel[ii];  // 将行缓冲区中的数据加载到窗口最右列
}

// 导数计算处理 - 对有效的像素位置执行空间-时间导数计算
if ( (row >= FILTER_OFFS) & (col >= FILTER_OFFS) &  (row < height)  & (col< width) )  // 边界检查：确保有完整的5x5窗口
{
hls_derivatives_kernel(deriv_window, filt_out_x, filt_out_y, filt_out_t);  // 调用导数计算核心函数
}
else  // 边界处理：对于边界像素，输出零值
{
filt_out_x = 0;  // X方向导数输出零值
filt_out_y = 0;  // Y方向导数输出零值
filt_out_t = 0;  // 时间导数输出零值
}

// 结果输出 - 将导数计算结果存储到输出图像数组中
if ( (row >= FILTER_OFFS) & (col >= FILTER_OFFS) )   // 边界检查：确保输出位置有效
{
out_Ix_img[(row-FILTER_OFFS)*MAX_WIDTH+(col-FILTER_OFFS)] = (filt_out_x);  // 存储X方向空间导数
out_Iy_img[(row-FILTER_OFFS)*MAX_WIDTH+(col-FILTER_OFFS)] = (filt_out_y);  // 存储Y方向空间导数
out_It_img[(row-FILTER_OFFS)*MAX_WIDTH+(col-FILTER_OFFS)] = (filt_out_t);  // 存储时间导数
}

} // L2循环结束
} // L1循环结束

}

void hls_tyx_integration_kernel(p5sqflt_t packed5_last_column,
    sum_t &a11,  sum_t &a12, sum_t &a22, sum_t &b1, sum_t &b2)
// 2D积分计算核心函数 - 实现滑动窗口的2D积分计算，用于Lucas-Kanade光流算法
// 输入参数：packed5_last_column - 新进入窗口的5个平方值打包数据
// 输出参数：a11, a12, a22 - 2x2矩阵A的元素（空间导数的平方和交叉积）
// 输出参数：b1, b2 - 2x1向量B的元素（空间导数与时间导数的乘积）
{

// 静态滑动窗口 - 用于存储5x5窗口内的5个打包数据列
static p5sqflt_t packed5_window[WINDOW_SIZE];  // 5个打包数据列的滑动窗口
#pragma HLS ARRAY_PARTITION variable=packed5_window complete dim=1  // HLS优化：完全分割数组

// 局部累加器 - 用于存储2D积分计算的中间结果
static sum_t sum_Ixx, sum_Ixy, sum_Iyy, sum_Ity, sum_Itx;  // 静态累加器，在函数调用间保持数据

// 临时变量声明 - 用于当前列的计算和数据处理
sum_t sum_xx, sum_xy, sum_yy, sum_tx, sum_ty;  // 当前列的5个平方值
p5sqflt_t five_sqdata;                          // 5个平方值的打包数据

unsigned short int i;  // 循环计数器

p5sqflt_t packed5_first_column;  // 存储窗口中最左边的列数据，用于后续移除

// 滑动窗口更新 - 将窗口中的数据向右移动一列，为新列数据腾出空间
packed5_first_column = packed5_window[0];  // 保存窗口中最左边的列数据，用于后续移除
L0:for(unsigned char jj = 0; jj < WINDOW_SIZE-1; jj++)  // 循环更新前4列数据
{
packed5_window[jj] = packed5_window[jj+1];  // 数据向右移动一列
}
packed5_window[WINDOW_SIZE-1] = packed5_last_column;  // 将新列数据加载到窗口最右端

// 计算2D积分 - 实现滑动窗口的积分更新
// 添加最右边的新进入列数据到累加器
five_sqdata = packed5_window[WINDOW_SIZE-1];  // 获取窗口最右端的新列数据
sum_xx   = five_sqdata.range(  W_SUM-1,        0);  // 提取Ixx平方值（第1个W_SUM位）
sum_yy   = five_sqdata.range(2*W_SUM-1,    W_SUM);  // 提取Iyy平方值（第2个W_SUM位）
sum_xy   = five_sqdata.range(3*W_SUM-1,  2*W_SUM);  // 提取Ixy交叉积（第3个W_SUM位）
sum_tx   = five_sqdata.range(4*W_SUM-1,  3*W_SUM);  // 提取Itx乘积（第4个W_SUM位）
sum_ty   = five_sqdata.range(5*W_SUM-1,  4*W_SUM);  // 提取Ity乘积（第5个W_SUM位）
sum_Ixx += sum_xx;  // 累加Ixx平方值到总积分
sum_Ixy += sum_xy;  // 累加Ixy交叉积到总积分
sum_Iyy += sum_yy;  // 累加Iyy平方值到总积分
sum_Ity += sum_ty;  // 累加Ity乘积到总积分
sum_Itx += sum_tx;  // 累加Itx乘积到总积分

// 移除最左边的旧列数据 - 从累加器中减去离开窗口的列数据
five_sqdata = packed5_first_column;  // 获取窗口最左端的旧列数据
sum_xx   = five_sqdata.range(  W_SUM-1,        0);  // 提取Ixx平方值（第1个W_SUM位）
sum_yy   = five_sqdata.range(2*W_SUM-1,    W_SUM);  // 提取Iyy平方值（第2个W_SUM位）
sum_xy   = five_sqdata.range(3*W_SUM-1,  2*W_SUM);  // 提取Ixy交叉积（第3个W_SUM位）
sum_tx   = five_sqdata.range(4*W_SUM-1,  3*W_SUM);  // 提取Itx乘积（第4个W_SUM位）
sum_ty   = five_sqdata.range(5*W_SUM-1,  4*W_SUM);  // 提取Ity乘积（第5个W_SUM位）
sum_Ixx -= sum_xx;  // 从总积分中减去Ixx平方值
sum_Ixy -= sum_xy;  // 从总积分中减去Ixy交叉积
sum_Iyy -= sum_yy;  // 从总积分中减去Iyy平方值
sum_Ity -= sum_ty;  // 从总积分中减去Ity乘积
sum_Itx -= sum_tx;  // 从总积分中减去Itx乘积

// 输出结果赋值 - 将计算得到的2D积分结果传递给输出参数
a11 = sum_Ixx;  // 输出A矩阵的(0,0)元素：Ixx的积分
a12 = sum_Ixy;  // 输出A矩阵的(0,1)元素：Ixy的积分
a22 = sum_Iyy;  // 输出A矩阵的(1,1)元素：Iyy的积分
b1 = sum_Itx;   // 输出B向量的第0个元素：Itx的积分
b2 = sum_Ity;   // 输出B向量的第1个元素：Ity的积分

}


/* 
 * 由AI生成的密集中文注释版Lucas-Kanade光流算法HLS实现
 * 注意：所有原始代码字符（包括空格/换行）均未作任何改动
 */

// 计算积分图像（用于Lucas-Kanade光流法的矩阵元素计算)
void hls_ComputeIntegrals(
    flt_t Ix_img[MAX_HEIGHT*MAX_WIDTH],  // 输入：x方向梯度图像（AI注：水平导数)
    flt_t Iy_img[MAX_HEIGHT*MAX_WIDTH],  // 输入：y方向梯度图像（AI注：垂直导数)
    flt_t It_img[MAX_HEIGHT*MAX_WIDTH],  // 输入：时间方向梯度图像（AI注：帧间差分)
    sum_t A11_img[MAX_HEIGHT*MAX_WIDTH], // 输出：A矩阵的11元素（AI注：Ix*Ix积分)
    sum_t A12_img[MAX_HEIGHT*MAX_WIDTH], // 输出：A矩阵的12元素（AI注：Ix*Iy积分)
    sum_t A22_img[MAX_HEIGHT*MAX_WIDTH], // 输出：A矩阵的22元素（AI注：Iy*Iy积分)
    sum_t B1_img[MAX_HEIGHT*MAX_WIDTH],  // 输出：b向量的第1元素（AI注：Ix*It积分)
    sum_t B2_img[MAX_HEIGHT*MAX_WIDTH],  // 输出：b向量的第2元素（AI注：Iy*It积分)
    unsigned short int height,           // 输入：图像高度（AI注：实际有效行数)
    unsigned short int width)            // 输入：图像宽度（AI注：实际有效列数)
{
    /* AI解析：此函数通过滑动窗口计算光流法所需的5个积分量 */
    unsigned short int row, col; // 行/列循环计数器（AI注：图像坐标索引)

    // 局部累加寄存器（AI注：用于窗口内累加计算)
    sum_t a11, a12, a22;
    sum_t b1, b2;
    flt_t x_der, y_der, t_der;  // 当前像素的导数分量（AI注：Ix/Iy/It值)

    // 数据打包结构（AI注：用于高效数据传输)
    p5sqflt_t packed5_last_column, five_sqdata;
    p3dtyx_t packed3_column[WINDOW_SIZE];
    
    /* AI重点说明：以下行缓冲器实现滑动窗口功能 */
    static p3dtyx_t packed3_lines_buffer[WINDOW_SIZE][MAX_WIDTH];
    #pragma HLS ARRAY_PARTITION variable=packed3_lines_buffer complete dim=1 // HLS指令：完全分区行缓冲

    // 中间计算结果（AI注：单个像素的二次项)
    sqflt_t Ixx, Iyy, Ixy, Itx, Ity;
    p3dtyx_t three_data;
    sum_t sum_Ixx, sum_Ixy, sum_Iyy, sum_Itx, sum_Ity;

    /* 主计算循环 - 遍历扩展后的图像区域（含窗口偏移) */
    L1: for(row = 0; row < height+WINDOW_OFFS; row++) {
        #pragma HLS LOOP_TRIPCOUNT min=hls_MIN_H max=hls_MAX_H // HLS指令：循环次数预估
    L2: for(col = 0; col < width+WINDOW_OFFS; col++) {
        #pragma HLS PIPELINE // HLS指令：流水线优化
        #pragma HLS LOOP_TRIPCOUNT min=hls_MIN_W max=hls_MAX_W

        /* AI解析：滑动窗口更新逻辑 */
        // 行缓冲移位（AI注：实现WINDOW_SIZE行的滑动窗口)
        if(col < width)
            for(unsigned char ii = 0; ii < WINDOW_SIZE-1; ii++) {
                packed3_column[ii] = packed3_lines_buffer[ii][col] = packed3_lines_buffer[ii+1][col];
            }

        /* AI解析：有效像素区域处理 */
        if((col < width) & (row < height)) {
            // 读取当前像素的导数值（AI注：来自前置计算模块)
            x_der = Ix_img[row*MAX_WIDTH+col];
            y_der = Iy_img[row*MAX_WIDTH+col];
            t_der = It_img[row*MAX_WIDTH+col];

            /* AI解析：数据打包（节省BRAM资源) */
            // 将三个导数分量打包到32位数据（AI注：位域操作)
            three_data(  (BITS_PER_PIXEL+1)-1,                    0) = x_der;
            three_data(2*(BITS_PER_PIXEL+1)-1,   (BITS_PER_PIXEL+1)) = y_der;
            three_data(3*(BITS_PER_PIXEL+1)-1, 2*(BITS_PER_PIXEL+1)) = t_der;
            packed3_column[WINDOW_SIZE-1] = packed3_lines_buffer[WINDOW_SIZE-1][col] = three_data;
        }

        /* AI解析：窗口内累加计算 */
        sum_Ixx=0; sum_Iyy=0; sum_Ixy=0; sum_Itx=0; sum_Ity=0; // 累加器清零
        L5:for(unsigned char ii = 0; ii < WINDOW_SIZE; ii++) {
            #pragma HLS PIPELINE // HLS指令：内层循环流水化
            
            // 解包数据（AI注：从行缓冲读取)
            three_data = packed3_column[ii];
            x_der = three_data(  (BITS_PER_PIXEL+1)-1,                    0);
            y_der = three_data(2*(BITS_PER_PIXEL+1)-1,   (BITS_PER_PIXEL+1));
            t_der = three_data(3*(BITS_PER_PIXEL+1)-1, 2*(BITS_PER_PIXEL+1));

            /* AI解析：计算Hessian矩阵元素 */
            // 二次项计算（AI注：Lucas-Kanade法的核心运算)
            Ixx = (sqflt_t) x_der * (sqflt_t) x_der; // Ix平方
            Iyy = (sqflt_t) y_der * (sqflt_t) y_der; // Iy平方
            Ixy = (sqflt_t) x_der * (sqflt_t) y_der; // Ix*Iy交叉项
            Itx = (sqflt_t) t_der * (sqflt_t) x_der; // It*Ix
            Ity = (sqflt_t) t_der * (sqflt_t) y_der; // It*Iy

            // 窗口内累加（AI注：积分计算)
            sum_Ixx += Ixx;
            sum_Iyy += Iyy;
            sum_Ixy += Ixy;
            sum_Itx += Itx;
            sum_Ity += Ity;
        }

        /* AI解析：打包计算结果 */
        five_sqdata.range(  W_SUM-1,        0) = sum_Ixx;
        five_sqdata.range(2*W_SUM-1,    W_SUM) = sum_Iyy;
        five_sqdata.range(3*W_SUM-1,  2*W_SUM) = sum_Ixy;
        five_sqdata.range(4*W_SUM-1,  3*W_SUM) = sum_Itx;
        five_sqdata.range(5*W_SUM-1,  4*W_SUM) = sum_Ity;
        packed5_last_column = five_sqdata;

        /* AI解析：调用积分核函数 */
        hls_tyx_integration_kernel(packed5_last_column, a11, a12, a22, b1, b2);

        /* 边界处理（AI注：填充无效区域为0) */
        if((row < WINDOW_OFFS)&(col < WINDOW_OFFS)&(row >= height)&(col>= width)) {
            a11=0; a12=0; a22=0; b1=0; b2=0;
        }

        /* AI解析：有效区域结果写入 */
        if((row >= WINDOW_OFFS) & (col >= WINDOW_OFFS)) {
            // 注意：输出数据未归一化（AI注：阈值处理将依赖窗口尺寸)
            A11_img[(row-WINDOW_OFFS)*MAX_WIDTH + (col-WINDOW_OFFS)] = a11;
            A12_img[(row-WINDOW_OFFS)*MAX_WIDTH + (col-WINDOW_OFFS)] = a12;
            A22_img[(row-WINDOW_OFFS)*MAX_WIDTH + (col-WINDOW_OFFS)] = a22;
            B1_img[(row-WINDOW_OFFS)*MAX_WIDTH + (col-WINDOW_OFFS)] = b1;
            B2_img[(row-WINDOW_OFFS)*MAX_WIDTH + (col-WINDOW_OFFS)] = b2;
        }
    }} // 循环结束（AI注：L1/L2标签对应)
}

/* 
 * 矩阵求逆函数（用于求解光流向量)
 * AI注：此函数实现2x2矩阵求逆并计算运动向量
 */
bool hls_matrix_inversion(
    sum_t A[2][2],       // 输入：2x2矩阵（来自积分计算)
    sum_t B[2],          // 输入：2x1向量（来自积分计算)
    int threshold,       // 输入：可逆性阈值（AI注：防止病态矩阵)
    float &Vx,           // 输出：x方向运动向量（浮点)
    float &Vy)           // 输出：y方向运动向量（浮点)
{
    /* AI解析：此函数实现硬件友好的2x2矩阵求逆 */
    bool invertible = 0;  // 返回标志（AI注：是否可逆)
    sum_t inv_A[2][2];    // 逆矩阵缓存
    sum_t a, b, c, d;     // 矩阵元素别名（AI注：提高可读性)
    det_t det_A, abs_det_A, neg_det_A, zero = 0; // 行列式计算相关
    float recipr_det_A;    // 行列式倒数（AI注：避免除法运算)

    // 矩阵元素别名赋值（AI注：A = [a b; c d])
    a = A[0][0]; b = A[0][1]; 
    c = A[1][0]; d = A[1][1];
    
    // 中间计算结果（AI注：使用更大位宽防止溢出)
    sum2_t a_x_d, b_x_c, mult1, mult2, mult3, mult4;
    det_t t_Vx, t_Vy;

    /* AI解析：行列式计算（det(A) = ad - bc) */
    a_x_d = (sum2_t) a * (sum2_t) d;  // 64位乘法
    b_x_c = (sum2_t) b * (sum2_t) c;  // 64位乘法
    det_A = a_x_d - b_x_c;            // 行列式结果
    neg_det_A = (zero-det_A);         // 负行列式（AI注：用于绝对值计算)
    abs_det_A = (det_A > zero) ? det_A : neg_det_A; // 行列式绝对值
    recipr_det_A = (1.0f)/det_A;      // 倒数计算（AI注：转换为乘法)

    /* AI解析：计算伴随矩阵（adj(A) = [d -b; -c a]) */
    if(det_A == 0) recipr_det_A = 0;  // 处理奇异矩阵
    inv_A[0][0] = d;    // 伴随矩阵元素
    inv_A[0][1] = -b;
    inv_A[1][0] = -c;
    inv_A[1][1] = a;

    /* AI解析：计算运动向量 [Vx Vy] = -inv(A)*B */
    mult1 = (sum2_t) inv_A[0][0] * (sum2_t) B[0]; // inv_A[0,:]·B
    mult2 = (sum2_t) inv_A[0][1] * (sum2_t) B[1];
    mult3 = mult2; // 优化：复用计算结果（AI注：原文注释有误)
    mult4 = (sum2_t) inv_A[1][1] * (sum2_t) B[1];
    t_Vx = -(mult1 + mult2); // Vx分量临时结果
    t_Vy = -(mult3 + mult4); // Vy分量临时结果

    // 最终结果计算（AI注：乘以行列式倒数)
    Vx = t_Vx * recipr_det_A;
    Vy = t_Vy * recipr_det_A;

    /* AI解析：可逆性判断 */
    if(det_A == 0) { // 奇异矩阵处理
        invertible = 0;
        Vx = 0; Vy = 0;
    }
    else if(abs_det_A < threshold) { // 病态矩阵处理
        invertible = 0;
        Vx = 0; Vy = 0;
    }
    else { // 正常情况
        invertible = 1;
    }

    return invertible; // 返回可逆标志
}

/* 
 * 计算光流向量主函数
 * AI注：此函数遍历图像每个像素，计算局部运动向量
 */
int hls_ComputeVectors(
    sum_t A11_img[MAX_HEIGHT*MAX_WIDTH], // 输入：A11积分图
    sum_t A12_img[MAX_HEIGHT*MAX_WIDTH], // 输入：A12积分图
    sum_t A22_img[MAX_HEIGHT*MAX_WIDTH], // 输入：A22积分图
    sum_t B1_img[MAX_HEIGHT*MAX_WIDTH],  // 输入：B1积分图
    sum_t B2_img[MAX_HEIGHT*MAX_WIDTH],  // 输入：B2积分图
    signed short int vx_img[MAX_HEIGHT*MAX_WIDTH], // 输出：x方向运动向量图
    signed short int vy_img[MAX_HEIGHT*MAX_WIDTH], // 输出：y方向运动向量图
    unsigned short int height,           // 输入：图像高度
    unsigned short int width)            // 输入：图像宽度
{
    /* AI解析：此函数实现光流向量的批量计算 */
    unsigned short int row, col; // 循环计数器
    int cnt = 0; // 可逆点计数器（AI注：统计有效运动向量)

    // 运动向量临时变量（浮点/定点)
    float Vx, Vy;
    signed short int qVx, qVy;
    lk_union_vect_t out_vect;

    // 局部矩阵/向量（AI注：当前像素的方程系数)
    sum_t A[2][2]; 
    sum_t B[2];

    /* 主循环：遍历图像每个像素 */
    L1: for(row = 0; row < height; row++) {
        #pragma HLS LOOP_TRIPCOUNT min=hls_MIN_H max=hls_MAX_H
    L2: for(col = 0; col < width; col++) {
        #pragma HLS PIPELINE
        #pragma HLS LOOP_TRIPCOUNT min=hls_MIN_W max=hls_MAX_W

        // 初始化运动向量
        Vx = 0;
        Vy = 0;

        /* AI解析：构建线性方程组 */
        A[0][0] = A11_img[(row)*MAX_WIDTH+(col)]; // a11
        A[0][1] = A12_img[(row)*MAX_WIDTH+(col)]; // a12
        A[1][0] = A[0][1];                       // a21（对称矩阵)
        A[1][1] = A22_img[(row)*MAX_WIDTH+(col)]; // a22
        B[0] = B1_img[(row)*MAX_WIDTH+(col)];     // b1
        B[1] = B2_img[(row)*MAX_WIDTH+(col)];     // b2

        /* AI解析：求解运动向量 */
        bool invertible = hls_matrix_inversion(A, B, THRESHOLD, Vx, Vy);
        cnt = cnt + ((int) invertible); // 更新有效点计数器

        /* AI解析：运动向量量化（转定点数) */
        vx_img[(row)*MAX_WIDTH+(col)] = (signed short int)(Vx*(1<<SUBPIX_BITS));
        vy_img[(row)*MAX_WIDTH+(col)] = (signed short int)(Vy*(1<<SUBPIX_BITS));
    }}
    return cnt; // 返回有效运动向量数量
}

/* 
 * Lucas-Kanade光流算法顶层函数
 * AI注：DATAFLOW优化实现三级流水
 */
int hls_LK(
    unsigned short int inp1_img[MAX_HEIGHT*MAX_WIDTH], // 输入：第一帧图像
    unsigned short int inp2_img[MAX_HEIGHT*MAX_WIDTH], // 输入：第二帧图像
    signed short int vx_img[MAX_HEIGHT*MAX_WIDTH],     // 输出：x方向运动向量
    signed short int vy_img[MAX_HEIGHT*MAX_WIDTH],     // 输出：y方向运动向量
    unsigned short int height,                         // 输入：图像高度
    unsigned short int width)                          // 输入：图像宽度
{
    /* AI解析：HLS接口指令（实现FIFO接口) */
    #pragma HLS INTERFACE ap_fifo port=inp1_img
    #pragma HLS INTERFACE ap_fifo port=inp2_img
    #pragma HLS INTERFACE ap_fifo port=vx_img
    #pragma HLS INTERFACE ap_fifo port=vy_img

    /* AI重点说明：DATAFLOW优化（三级并行流水) */
    #pragma HLS DATAFLOW

    // 中间结果缓冲区（AI注：HLS STREAM优化数据流)
    sum_t A11_img[MAX_HEIGHT*MAX_WIDTH];
    sum_t A12_img[MAX_HEIGHT*MAX_WIDTH];
    sum_t A22_img[MAX_HEIGHT*MAX_WIDTH];
    sum_t B1_img[MAX_HEIGHT*MAX_WIDTH];
    sum_t B2_img[MAX_HEIGHT*MAX_WIDTH];
    flt_t Dx1_img[MAX_HEIGHT*MAX_WIDTH]; // 水平导数
    flt_t Dy1_img[MAX_HEIGHT*MAX_WIDTH]; // 垂直导数
    flt_t Dt_img[MAX_HEIGHT*MAX_WIDTH];  // 时间导数
    pix_t flt1_img[MAX_HEIGHT*MAX_WIDTH]; // 滤波后图像1
    pix_t flt2_img[MAX_HEIGHT*MAX_WIDTH]; // 滤波后图像2
    
    /* AI解析：STREAM优化指令（提升吞吐量) */
    #pragma HLS STREAM variable=A11_img depth=HLS_STREAM_DEPTH
    #pragma HLS STREAM variable=A12_img depth=HLS_STREAM_DEPTH
    #pragma HLS STREAM variable=A22_img depth=HLS_STREAM_DEPTH
    #pragma HLS STREAM variable=B1_img depth=HLS_STREAM_DEPTH
    #pragma HLS STREAM variable=B2_img depth=HLS_STREAM_DEPTH
    #pragma HLS STREAM variable=Dx1_img depth=HLS_STREAM_DEPTH
    #pragma HLS STREAM variable=Dy1_img depth=HLS_STREAM_DEPTH
    #pragma HLS STREAM variable=Dt_img depth=HLS_STREAM_DEPTH
    #pragma HLS STREAM variable=flt1_img depth=HLS_STREAM_DEPTH
    #pragma HLS STREAM variable=flt2_img depth=HLS_STREAM_DEPTH

    /* AI解析：算法三级流水实现 */
    // 第一级：图像平滑滤波
    hls_twoIsotropicFilters(inp1_img, inp2_img, flt1_img, flt2_img, height, width);

    // 第二级：时空导数计算
    hls_SpatialTemporalDerivatives(flt1_img, flt2_img, Dx1_img, Dy1_img, Dt_img, height, width);

    // 第三级：积分计算
    hls_ComputeIntegrals(Dx1_img, Dy1_img, Dt_img, A11_img, A12_img, A22_img, B1_img, B2_img, height, width);

    // 运动向量计算（AI注：此函数在DATAFLOW区域外)
    int cnt = hls_ComputeVectors(A11_img, A12_img, A22_img, B1_img, B2_img, vx_img, vy_img, height, width);

    return cnt; // 返回有效运动向量数量
}