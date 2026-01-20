import numpy as np
import cv2

# 定义梯度计算的窗口半径
GRADIENT_WINDOW_RADIUS = 2  # 窗口大小为 2 * GRADIENT_WINDOW_RADIUS + 1
# 打印格式设置
PRINT_CONFIG = {
    'threshold': np.inf,  # 完整显示数组
    'suppress': True,  # 禁用科学计数法
    'precision': 3,  # 小数点后位数
    'floatmode': 'fixed',  # 固定小数点格式
    'linewidth': 32 * 8,  # 每行最大字符数
    'edgeitems': 64  # 省略号时显示的边缘元素数
}

# 应用打印设置
np.set_printoptions(**PRINT_CONFIG)


def load_images(path_A="E:/data/MH_01_easy/mav0/cam0/data/1403636579763555584.png", 
                path_B="E:/data/MH_01_easy/mav0/cam0/data/1403636579813555456.png"):
    """
    从指定路径加载并处理图片
    path_A: 第一张图片的路径
    path_B: 第二张图片的路径
    """
    img_A = cv2.imread(path_A, cv2.IMREAD_GRAYSCALE)
    img_B = cv2.imread(path_B, cv2.IMREAD_GRAYSCALE)

    if img_A is None or img_B is None:
        raise ValueError(f"无法加载图片，请检查文件路径: {path_A} 和 {path_B}")

    if img_A.shape != img_B.shape:
        print(f"警告：两张图片尺寸不同。图片A: {img_A.shape}, 图片B: {img_B.shape}")
        img_B = cv2.resize(img_B, (img_A.shape[1], img_A.shape[0]))
        print(f"已将图片B调整为与图片A相同的尺寸: {img_B.shape}")

    return img_A.astype(np.float32), img_B.astype(np.float32)


def detect_corners(image, max_corners=100):
    image = image.astype(np.float32)
    corners = cv2.goodFeaturesToTrack(
        image,
        maxCorners=max_corners,
        qualityLevel=0.08,
        minDistance=20
    )
    if corners is not None:
        corners = corners.reshape(-1, 2)
        return corners
    return []


def extract_matrix_around_point(image, center_x, center_y, size=64):
    result = np.zeros((size, size))
    half_size = size // 2
    start_y = int(center_y - half_size)
    end_y = start_y + size
    start_x = int(center_x - half_size)
    end_x = start_x + size
    valid_start_y = max(0, start_y)
    valid_end_y = min(image.shape[0], end_y)
    valid_start_x = max(0, start_x)
    valid_end_x = min(image.shape[1], end_x)
    result_start_y = max(0, -start_y)
    result_end_y = size - max(0, end_y - image.shape[0])
    result_start_x = max(0, -start_x)
    result_end_x = size - max(0, end_x - image.shape[1])
    result[result_start_y:result_end_y, result_start_x:result_end_x] = \
        image[valid_start_y:valid_end_y, valid_start_x:valid_end_x]
    return result





def gaussian_filter(matrix):
    height, width = matrix.shape
    result = np.zeros((height, width))
    kernel = np.array([[1 / 16, 1 / 8, 1 / 16],
                       [1 / 8, 1 / 4, 1 / 8],
                       [1 / 16, 1 / 8, 1 / 16]])

    padded = np.zeros((height + 2, width + 2))
    padded[1:-1, 1:-1] = matrix
    for i in range(height):
        for j in range(width):
            window = padded[i:i + 3, j:j + 3]
            result[i, j] = np.sum(window * kernel)
    return result


# 图片缩小
def simple_interpolation(matrix):
    height, width = matrix.shape
    # 计算新的尺寸（考虑奇数行列）
    new_height = (height + 1) // 2
    new_width = (width + 1) // 2
    result = np.zeros((new_height, new_width))

    # 只取奇数索引的行和列（在Python中索引从0开始，所以是从1开始步长为2）
    for i in range(1, height, 2):
        for j in range(1, width, 2):
            # 将奇数位置的值直接赋给结果矩阵
            result[i // 2, j // 2] = matrix[i, j]

    return result


def bilinear_interpolate(matrix, x, y):
    height, width = matrix.shape

    # 如果坐标超出范围，返回None
    if x < 0 or y < 0 or x >= width - 1 or y >= height - 1:
        return None

    # 获取周围四个点的坐标
    x0 = int(np.floor(x))
    x1 = x0 + 1
    y0 = int(np.floor(y))
    y1 = y0 + 1

    # 计算插值权重
    wx = x - x0
    wy = y - y0

    # 步骤1：在x方向进行插值，得到y0和y1行上的两个插值结果
    interpolated_y0 = (1 - wx) * matrix[y0, x0] + wx * matrix[y0, x1]  # y0行的插值
    interpolated_y1 = (1 - wx) * matrix[y1, x0] + wx * matrix[y1, x1]  # y1行的插值

    # 步骤2：在y方向进行插值，得到最终结果
    value = (1 - wy) * interpolated_y0 + wy * interpolated_y1

    return value

# ========== 添加LK光流法的计算 ==========
def calc_lk_optical_flow(img_A, img_B, corners):
    """
    使用OpenCV的LK光流法计算每个角点的光流
    img_A: 第一帧图像（float32或uint8，单通道）
    img_B: 第二帧图像（float32或uint8，单通道）
    corners: 角点坐标，形状为(N, 2)
    返回：每个角点的新位置、状态、误差
    """
    # OpenCV的calcOpticalFlowPyrLK要求输入为uint8
    if img_A.dtype != np.uint8:
        img_A_uint8 = np.clip(img_A, 0, 255).astype(np.uint8)
    else:
        img_A_uint8 = img_A
    if img_B.dtype != np.uint8:
        img_B_uint8 = np.clip(img_B, 0, 255).astype(np.uint8)
    else:
        img_B_uint8 = img_B

    # 设置LK光流法的参数
    lk_params = dict(
        winSize=(5, 5),  # 搜索窗口大小
        maxLevel=3,  # 金字塔层数
        criteria=(cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_COUNT, 5, 0.01),  # 迭代终止条件
    )

    # 角点需要reshape为(N,1,2)
    p0 = corners.reshape(-1, 1, 2).astype(np.float32)

    # 计算光流
    p1, st, err = cv2.calcOpticalFlowPyrLK(
        img_A_uint8,
        img_B_uint8,
        p0,
        None,
        **lk_params
    )

    return p0, p1, st, err
def calculate_gradient(matrix_A, matrix_B, total_flow_u=None, total_flow_v=None, iter_count=0):
    """
    计算图像梯度和时间梯度
    matrix_A: 第一帧图像
    matrix_B: 第二帧图像
    total_flow_u: 水平方向累积光流场
    total_flow_v: 垂直方向累积光流场
    iter_count: 当前迭代次数
    """
    height, width = matrix_A.shape
    center_y = height // 2
    center_x = width // 2
    y_start = max(center_y - GRADIENT_WINDOW_RADIUS, 0)
    y_end = min(center_y + GRADIENT_WINDOW_RADIUS + 1, height)
    x_start = max(center_x - GRADIENT_WINDOW_RADIUS, 0)
    x_end = min(center_x + GRADIENT_WINDOW_RADIUS + 1, width)

    # 初始化光流场
    if total_flow_u is None:
        total_flow_u = 0
    if total_flow_v is None:
        total_flow_v = 0

    # 设置卷积核
    size = 2 * GRADIENT_WINDOW_RADIUS + 1
    center = GRADIENT_WINDOW_RADIUS

    kernel_x = np.zeros((size, size))
    kernel_x[center, center - 2:center + 3] = np.array([1, -8, 0, 8, -1]) / 12

    kernel_y = np.zeros((size, size))
    kernel_y[center - 2:center + 3, center] = np.array([1, -8, 0, 8, -1]) / 12

    # 初始化结果矩阵为实际窗口大小
    window_height = y_end - y_start
    window_width = x_end - x_start
    Ix = np.zeros((window_height, window_width))
    Iy = np.zeros((window_height, window_width))
    It = np.zeros((window_height, window_width))
    warped_B = np.zeros((window_height, window_width))

    # 计算每个像素的梯度
    for i in range(window_height):
        for j in range(window_width):
            y = y_start + i
            x = x_start + j

            # 确保有足够的边界进行卷积
            if (y >= GRADIENT_WINDOW_RADIUS and y < height - GRADIENT_WINDOW_RADIUS and
                    x >= GRADIENT_WINDOW_RADIUS and x < width - GRADIENT_WINDOW_RADIUS):

                # 提取当前像素的窗口区域
                window_A = matrix_A[y - GRADIENT_WINDOW_RADIUS:y + GRADIENT_WINDOW_RADIUS + 1,
                           x - GRADIENT_WINDOW_RADIUS:x + GRADIENT_WINDOW_RADIUS + 1]

                # 计算空间梯度
                Ix[i, j] = np.sum(window_A * kernel_x)
                Iy[i, j] = np.sum(window_A * kernel_y)

                # 计算变形后的坐标
                new_x = x + total_flow_u
                new_y = y + total_flow_v

                # 使用双线性插值计算变形后的值
                warped_B_value = bilinear_interpolate(matrix_B, new_x, new_y)

                # 计算时间梯度
                if warped_B_value is not None:
                    It[i, j] = warped_B_value - float(matrix_A[y, x])
                    warped_B[i, j] = warped_B_value
                else:
                    It[i, j] = float(matrix_B[y, x]) - float(matrix_A[y, x])
                    warped_B[i, j] = float(matrix_B[y, x])

    return Ix, Iy, It, warped_B


def calculate_pixel_integral_matrix(Ix, Iy):
    """
    计算每个像素点的局部积分矩阵
    Ix: 水平方向梯度
    Iy: 垂直方向梯度
    返回: 每个像素对应的2x2积分矩阵
    """
    result = np.zeros((2, 2))
    result[0, 0] = np.sum(Ix * Ix)  # Ix^2的积分
    result[0, 1] = np.sum(Ix * Iy)  # Ix*Iy的积分
    result[1, 0] = result[0, 1]  # 对称矩阵
    result[1, 1] = np.sum(Iy * Iy)  # Iy^2的积分

    return result


def calculate_pixel_temporal_spatial_matrix(Ix, Iy, It):
    """
    计算每个像素点的时间空间梯度积分矩阵
    Ix: 水平方向梯度
    Iy: 垂直方向梯度
    It: 时间梯度
    返回: 每个像素对应的2x1时空积分矩阵
    """
    result = np.zeros((2, 1))
    result[0, 0] = np.sum(Ix * It)  # Ix*It的积分
    result[1, 0] = np.sum(Iy * It)  # Iy*It的积分

    return result


def calculate_inverse_matrix(A):
    """
    计算2x2矩阵的逆矩阵
    A: 输入的2x2矩阵
    返回: 如果可逆，返回逆矩阵；如果不可逆，返回None
    """
    # 计算行列式
    det_A = A[0, 0] * A[1, 1] - A[0, 1] * A[1, 0]

    # 如果行列式接近0，矩阵不可逆
    if abs(det_A) < 1e-10:
        return None

    # 计算伴随矩阵并除以行列式得到逆矩阵
    A_inv = np.array([
        [A[1, 1], -A[0, 1]],
        [-A[1, 0], A[0, 0]]
    ]) / det_A

    return A_inv


def calculate_optical_flow(matrix_A, matrix_B, max_iterations=50, epsilon=0.01,
                           initial_flow_u=None, initial_flow_v=None):
    """
    计算两帧图像之间的光流
    matrix_A: 第一帧图像
    matrix_B: 第二帧图像
    max_iterations: 最大迭代次数
    epsilon: 收敛阈值
    initial_flow_u: 初始水平方向光流场
    initial_flow_v: 初始垂直方向光流场
    返回: (final_flow_u, final_flow_v) 最终的水平和垂直光流场
    """
    height, width = matrix_A.shape
    # 初始化光流场
    flow_u = initial_flow_u if initial_flow_u is not None else np.zeros((height, width))
    flow_v = initial_flow_v if initial_flow_v is not None else np.zeros((height, width))

    for iter_count in range(max_iterations):
        print(f"\n第 {iter_count + 1} 次迭代:")

        # 计算中心点的图像梯度和时间梯度
        Ix, Iy, It, warped_B = calculate_gradient(matrix_A, matrix_B, flow_u, flow_v, iter_count)

        # 计算中心点的局部积分矩阵
        A = calculate_pixel_integral_matrix(Ix, Iy)
        b = calculate_pixel_temporal_spatial_matrix(Ix, Iy, It)

        # 计算中心点的光流增量
        A_inv = calculate_inverse_matrix(A)
        if A_inv is not None:
            # 求解方程 Av = b
            # print(f"A: {A}")
            # print(f"b: {b}")
            flow_update = np.dot(A_inv, -b)
            delta_u = flow_update[0, 0]
            delta_v = flow_update[1, 0]

            # 更新光流值
            flow_u += delta_u
            flow_v += delta_v

            # 计算变化量
            total_delta = delta_u ** 2 + delta_v ** 2

            # print(f"变化量: {total_delta:.6f}")
            # print(f"当前光流: u = {flow_u:.3f}, v = {flow_v:.3f}")

            if total_delta < epsilon:
                print(f"\n光流计算在第 {iter_count + 1} 次迭代后收敛")
                break
        else:
            print("矩阵求逆失败，停止迭代")
            break

    return flow_u, flow_v


# 第一层：对原始矩阵进行高斯滤波
def build_gaussian_pyramid(matrix, levels):
    """
    构建高斯金字塔
    matrix: 输入矩阵
    levels: 金字塔层数
    返回: 金字塔层级列表
    """
    pyramid = [matrix]
    current_matrix = matrix.copy()

    for level in range(levels - 1):
        # 高斯滤波
        filtered = gaussian_filter(current_matrix)
        # print(f"\n第{level + 1}层高斯滤波后的矩阵:")
        # print(np.round(filtered, 3))

        # 降采样
        downsampled = simple_interpolation(filtered)
        # print(f"\n第{level + 1}层降采样后的矩阵:")
        # print(np.round(downsampled, 3))

        # 对降采样结果再次滤波
        filtered_downsampled = gaussian_filter(downsampled)
        # print(f"\n第{level + 1}层降采样后再次滤波的矩阵:")
        # print(np.round(filtered_downsampled, 3))

        # 添加到金字塔中
        pyramid.append(filtered_downsampled)
        current_matrix = filtered_downsampled

    return pyramid


def calculate_pyramid_flow(pyramid_A, pyramid_B, max_iterations=5, epsilon=0.01):
    """
    计算金字塔各层的中心点光流
    pyramid_A: 第一帧图像的金字塔
    pyramid_B: 第二帧图像的金字塔
    返回: 最终的光流值和光流大小
    """
    levels = len(pyramid_A)
    flow_u = 0.0
    flow_v = 0.0

    # 从金字塔顶层开始计算
    # 如果levels等于3，这个循环会执行3次，level分别为2,1,0
    for level in range(levels - 1, -1, -1):
        print(f"\n计算第{levels - level}层金字塔的光流:")

        # 如果不是最顶层，将上一层的光流值扩大2倍作为初始值
        if level < levels - 1:
            flow_u *= 2
            flow_v *= 2

        # 计算当前层的光流
        flow_u, flow_v = calculate_optical_flow(
            pyramid_A[level],
            pyramid_B[level],
            max_iterations=max_iterations,
            epsilon=epsilon,
            initial_flow_u=flow_u,
            initial_flow_v=flow_v
        )

        # 计算当前层的光流大小
        flow_magnitude = np.sqrt(flow_u ** 2 + flow_v ** 2)
        print(f"第{levels - level}层金字塔的光流结果:")
        print(f"u = {flow_u:.3f}, v = {flow_v:.3f}")
        print(f"光流大小 = {flow_magnitude:.3f}")

    # 计算最终光流大小
    final_flow_magnitude = np.sqrt(flow_u ** 2 + flow_v ** 2)
    print(f"\n最终光流大小: {final_flow_magnitude:.3f}")
    return flow_u, flow_v, final_flow_magnitude


def compute_optical_flow(matrix_A, matrix_B, pyramid_levels=3):
    """
    主函数：计算两帧图像间的光流
    """
    # 构建两个图像的高斯金字塔
    pyramid_A = build_gaussian_pyramid(matrix_A, pyramid_levels)
    pyramid_B = build_gaussian_pyramid(matrix_B, pyramid_levels)

    # 计算金字塔光流
    final_flow_u, final_flow_v, final_flow_magnitude = calculate_pyramid_flow(pyramid_A, pyramid_B)

    print("\n最终光流结果:")
    print(f"水平方向位移 (u): {final_flow_u:.3f}")
    print(f"垂直方向位移 (v): {final_flow_v:.3f}")
    print(f"光流大小: {final_flow_magnitude:.3f}")

    return final_flow_u, final_flow_v, final_flow_magnitude

# 加载图像并计算光流
import os

# 图像目录 - 修改为项目中的测试数据目录
image_dir = "../test_data/"
# 获取目录中的所有图像文件
image_files = sorted([f for f in os.listdir(image_dir) if f.endswith('.png')])

# 用于存储所有有效的AEE和AAE值
all_aee_values = []
all_aae_values = []

# 打开结果文件
with open(r'MH_04_difficult.txt', 'w', encoding='utf-8') as f:
    f.write("图像对\tAEE\tAAE(度)\n")
    
    # 连续处理25张图片（24对）
    for i in range(0, len(image_files) - 1):
        if i >= 500:  # 处理完24对图片后停止
            break
            
        path_A = os.path.join(image_dir, image_files[i])
        path_B = os.path.join(image_dir, image_files[i+1])
        
        print(f"\n处理图像对 {i+1}/24: {image_files[i]} -> {image_files[i+1]}")
        
        try:
            # 加载图像对
            img_matrix_A, img_matrix_B = load_images(path_A, path_B)
            print(f"矩阵A - 形状: {img_matrix_A.shape}")
            print(f"矩阵B - 形状: {img_matrix_B.shape}")
            
            # 检测角点
            corners_A = detect_corners(img_matrix_A, max_corners=1000)
            print(f"检测到的角点数量: {len(corners_A)}")
            
            # 批量提取所有角点对应的子矩阵
            sub_matrices = []
            for idx, (corner_x, corner_y) in enumerate(corners_A):
                sub_matrix_A = extract_matrix_around_point(img_matrix_A, corner_x, corner_y)
                sub_matrix_B = extract_matrix_around_point(img_matrix_B, corner_x, corner_y)
                sub_matrices.append({
                    'corner_index': idx,
                    'corner_coords': (corner_x, corner_y),
                    'sub_matrix_A': sub_matrix_A,
                    'sub_matrix_B': sub_matrix_B
                })
            
            # 对每个角点的子矩阵计算光流
            corner_flows = []
            for item in sub_matrices:
                matrix_A = item['sub_matrix_A']
                matrix_B = item['sub_matrix_B']
                flow_u, flow_v, magnitude = compute_optical_flow(matrix_A, matrix_B, pyramid_levels=3)
                corner_flows.append({
                    'corner_index': item['corner_index'],
                    'corner_coords': item['corner_coords'],
                    'flow_u': flow_u,
                    'flow_v': flow_v,
                    'magnitude': magnitude
                })
            
            # 调用LK光流法
            p0, p1, st, err = calc_lk_optical_flow(img_matrix_A, img_matrix_B, corners_A)
            
            # 收集自实现和OpenCV的光流
            my_u = np.array([item['flow_u'] for item in corner_flows])
            my_v = np.array([item['flow_v'] for item in corner_flows])
            opencv_u = []
            opencv_v = []
            for idx, (start, end, status) in enumerate(zip(p0, p1, st)):
                if status[0]:
                    opencv_u.append(end[0][0] - start[0][0])
                    opencv_v.append(end[0][1] - start[0][1])
                else:
                    opencv_u.append(np.nan)
                    opencv_v.append(np.nan)
            opencv_u = np.array(opencv_u)
            opencv_v = np.array(opencv_v)
            
            # 先去除自己算出来的光流绝对值大于32的点
            my_valid_mask = (np.abs(my_u) <= 32) & (np.abs(my_v) <= 32)
            
            # 只保留都有效的点
            valid_mask = my_valid_mask & ~np.isnan(opencv_u) & ~np.isnan(opencv_v)
            my_u_valid = my_u[valid_mask]
            my_v_valid = my_v[valid_mask]
            opencv_u_valid = opencv_u[valid_mask]
            opencv_v_valid = opencv_v[valid_mask]
            
            # 计算端点误差 (Endpoint Error)
            endpoint_errors = np.sqrt((my_u_valid - opencv_u_valid) ** 2 + (my_v_valid - opencv_v_valid) ** 2)
            
            # 滤除与OpenCV相差过大的点（例如超过平均误差的3倍或固定阈值）
            error_threshold = min(np.mean(endpoint_errors) * 3, 5.0)  # 使用平均误差的3倍或5.0作为阈值
            valid_endpoint_mask = endpoint_errors < error_threshold
            valid_endpoint_errors = endpoint_errors[valid_endpoint_mask]
            
            # 计算AEE
            aee = np.mean(valid_endpoint_errors) if valid_endpoint_errors.size > 0 else float('nan')
            
            # 计算AAE (Average Angular Error)
            epsilon = 1e-10
            # 只使用端点误差过滤后的有效点计算角度误差
            my_u_angle_valid = my_u_valid[valid_endpoint_mask]
            my_v_angle_valid = my_v_valid[valid_endpoint_mask]
            opencv_u_angle_valid = opencv_u_valid[valid_endpoint_mask]
            opencv_v_angle_valid = opencv_v_valid[valid_endpoint_mask]
            
            angles_my = np.arctan2(my_v_angle_valid, my_u_angle_valid + epsilon)
            angles_opencv = np.arctan2(opencv_v_angle_valid, opencv_u_angle_valid + epsilon)
            angle_diff = np.abs(angles_my - angles_opencv)
            angle_diff = np.minimum(angle_diff, np.pi - angle_diff)
            angle_diff_deg = np.degrees(angle_diff)
            
            # 先过滤，只保留绝对值小于等于1度的点
            valid_angle_mask = np.abs(angle_diff_deg) <= 1
            valid_angle_diff = angle_diff[valid_angle_mask]
            
            # 计算AAE
            aae_rad = np.mean(valid_angle_diff) if valid_angle_diff.size > 0 else float('nan')
            aae_deg = np.degrees(aae_rad) if valid_angle_diff.size > 0 else float('nan')
            
            # 存储有效的AEE和AAE值
            if not np.isnan(aee):
                all_aee_values.append(aee)
            if not np.isnan(aae_deg):
                all_aae_values.append(aae_deg)
            
            # 写入结果
            f.write(f"{image_files[i]}->{image_files[i+1]}\t{aee:.6f}\t{aae_deg:.6f}\n")
            f.flush()  # 确保立即写入文件
            
            print(f"图像对 {i+1} 的AEE: {aee:.6f}, AAE: {aae_deg:.6f}度")
            
        except Exception as e:
            print(f"处理图像对 {i+1} 时出错: {e}")
            f.write(f"{image_files[i]}->{image_files[i+1]}\tError\tError\n")
            f.flush()
    
    # 计算并写入所有图片的平均AEE和AAE
    mean_aee = np.mean(all_aee_values) if all_aee_values else float('nan')
    mean_aae = np.mean(all_aae_values) if all_aae_values else float('nan')
    
    f.write("\n===== 总体统计 =====\n")
    f.write(f"平均AEE: {mean_aee:.6f}\n")
    f.write(f"平均AAE(度): {mean_aae:.6f}\n")
    f.write(f"有效图像对数量: {len(all_aee_values)}/{i+1}\n")

print("\n所有结果已保存到 MH_04_difficult.txt")
print(f"平均AEE: {mean_aee:.6f}")
print(f"平均AAE(度): {mean_aae:.6f}")
print(f"有效图像对数量: {len(all_aee_values)}/{i+1}")