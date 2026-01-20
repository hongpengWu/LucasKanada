import cv2
import numpy as np
import os

def create_more_test_images():
    """创建更多的测试图像用于光流测试"""
    
    test_data_dir = "../test_data/"
    if not os.path.exists(test_data_dir):
        os.makedirs(test_data_dir)
    
    # 图像参数
    height, width = 388, 584
    num_frames = 25  # 生成25帧图像
    
    print(f"正在生成 {num_frames} 帧测试图像...")
    
    for frame_id in range(num_frames):
        # 创建基础图像：添加一些纹理模式
        img = np.zeros((height, width), dtype=np.uint8)
        
        # 添加正弦波纹理
        for i in range(height):
            for j in range(width):
                # 基础纹理
                base_pattern = 128 + 50 * np.sin(i * 0.1) * np.cos(j * 0.1)
                
                # 添加运动：水平和垂直方向的轻微移动
                motion_x = frame_id * 1.0  # 每帧水平移动1像素
                motion_y = frame_id * 0.5  # 每帧垂直移动0.5像素
                
                # 应用运动到纹理模式
                moved_i = i - motion_y
                moved_j = j - motion_x
                
                # 计算移动后的像素值
                if 0 <= moved_i < height and 0 <= moved_j < width:
                    pattern_value = 128 + 50 * np.sin(moved_i * 0.1) * np.cos(moved_j * 0.1)
                else:
                    pattern_value = base_pattern
                
                # 添加一些随机噪声
                noise = np.random.normal(0, 5)
                final_value = np.clip(pattern_value + noise, 0, 255)
                
                img[i, j] = int(final_value)
        
        # 添加一些几何形状作为特征点
        # 圆形特征
        center_x, center_y = width // 2 + int(10 * np.sin(frame_id * 0.2)), height // 2
        cv2.circle(img, (center_x, center_y), 20, 200, -1)
        
        # 矩形特征
        rect_x = 100 + frame_id * 2
        rect_y = 100
        cv2.rectangle(img, (rect_x, rect_y), (rect_x + 40, rect_y + 30), 180, -1)
        
        # 线条特征
        line_start = (50, 200 + frame_id)
        line_end = (150, 250 + frame_id)
        cv2.line(img, line_start, line_end, 220, 3)
        
        # 保存图像
        filename = f"frame{frame_id:02d}.png"
        filepath = os.path.join(test_data_dir, filename)
        cv2.imwrite(filepath, img)
        
        if frame_id % 5 == 0:
            print(f"已生成 {frame_id + 1}/{num_frames} 帧")
    
    print(f"成功生成 {num_frames} 帧测试图像到 {test_data_dir}")
    return num_frames

if __name__ == "__main__":
    create_more_test_images()
