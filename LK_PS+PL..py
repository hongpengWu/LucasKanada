import os
import numpy as np
from pynq import Overlay, allocate, MMIO

base_dir = "/home/xilinx/jupyter_notebooks/lk"
bit_path = os.path.join(base_dir, "design_1.bit")
hwh_path = os.path.join(base_dir, "design_1.hwh")
img1_path = os.path.join(base_dir, "test_data/frame00.png")
img2_path = os.path.join(base_dir, "test_data/frame01.png")
result_dir = os.path.join(base_dir, "result")

print("bit exists:", os.path.exists(bit_path), bit_path)
print("hwh exists:", os.path.exists(hwh_path), hwh_path)
print("img1 exists:", os.path.exists(img1_path), img1_path)
print("img2 exists:", os.path.exists(img2_path), img2_path)

os.makedirs(result_dir, exist_ok=True)
print("result dir:", result_dir)

use_cv2 = True
try:
    import cv2

    def load_gray(path):
        return cv2.imread(path, cv2.IMREAD_GRAYSCALE)

    def save_gray(path, img):
        cv2.imwrite(path, img)
except Exception:
    use_cv2 = False
    from PIL import Image

    def load_gray(path):
        return np.array(Image.open(path).convert("L"))

    def save_gray(path, img):
        Image.fromarray(img).save(path)

img1 = load_gray(img1_path)
img2 = load_gray(img2_path)
if img1 is None or img2 is None:
    raise RuntimeError("image load failed")

max_h, max_w = 398, 594
h, w = img1.shape
if h > max_h or w > max_w:
    nh = min(h, max_h)
    nw = min(w, max_w)
    img1 = img1[:nh, :nw]
    img2 = img2[:nh, :nw]
    h, w = img1.shape

ol = Overlay(bit_path)
ip_name = None
for k in ol.ip_dict:
    if "hls_LK" in k or "lk0" in k:
        ip_name = k
        break
if ip_name is None:
    ip_name = list(ol.ip_dict.keys())[0]
print("ip name:", ip_name)

base_addr = ol.ip_dict[ip_name]["phys_addr"]
mmio = MMIO(base_addr, 0x1000)

buf_size = max_h * max_w
in1 = allocate(shape=(buf_size,), dtype=np.uint16)
in2 = allocate(shape=(buf_size,), dtype=np.uint16)
vx = allocate(shape=(buf_size,), dtype=np.int16)
vy = allocate(shape=(buf_size,), dtype=np.int16)

in1[:] = 0
in2[:] = 0
vx[:] = 0
vy[:] = 0

for row in range(h):
    row_base = row * max_w
    in1[row_base:row_base + w] = img1[row, :w].astype(np.uint16)
    in2[row_base:row_base + w] = img2[row, :w].astype(np.uint16)

in1.flush()
in2.flush()
vx.flush()
vy.flush()

def write64(offset, value):
    mmio.write(offset, value & 0xFFFFFFFF)
    mmio.write(offset + 4, (value >> 32) & 0xFFFFFFFF)

write64(0x18, in1.physical_address)
write64(0x24, in2.physical_address)
write64(0x30, vx.physical_address)
write64(0x3C, vy.physical_address)
mmio.write(0x48, h)
mmio.write(0x50, w)

mmio.write(0x00, 0x01)
while (mmio.read(0x00) & 0x2) == 0:
    pass

vx.invalidate()
vy.invalidate()

vx_img = vx.reshape((max_h, max_w))[:h, :w].astype(np.float32) / (1 << 6)
vy_img = vy.reshape((max_h, max_w))[:h, :w].astype(np.float32) / (1 << 6)
mag = np.sqrt(vx_img ** 2 + vy_img ** 2)
mag_norm = (255 * (mag / (mag.max() + 1e-6))).astype(np.uint8)

vx_display = np.clip(vx_img * 0.1 + 128, 0, 255).astype(np.uint8)
vy_display = np.clip(vy_img * 0.1 + 128, 0, 255).astype(np.uint8)

mag_path = os.path.join(result_dir, "lk_mag.png")
vx_path = os.path.join(result_dir, "lk_vx.png")
vy_path = os.path.join(result_dir, "lk_vy.png")
flow_path = os.path.join(result_dir, "output_optical_flow.jpg")

save_gray(mag_path, mag_norm)
save_gray(vx_path, vx_display)
save_gray(vy_path, vy_display)

if use_cv2:
    flow_vis = img1.copy()
    if len(flow_vis.shape) == 2:
        flow_vis = cv2.cvtColor(flow_vis, cv2.COLOR_GRAY2BGR)
    for i in range(0, h, 8):
        for j in range(0, w, 8):
            fx = vx_img[i, j]
            fy = vy_img[i, j]
            if abs(fx) > 0.1 or abs(fy) > 0.1:
                pt1 = (int(j), int(i))
                pt2 = (int(j + fx), int(i + fy))
                cv2.arrowedLine(flow_vis, pt1, pt2, (0, 255, 0), 1, 8, 0, 0.3)
    cv2.imwrite(flow_path, flow_vis)
    print("saved:", flow_path)

print("saved:", mag_path)
print("saved:", vx_path)
print("saved:", vy_path)
print("vx stats:", float(vx_img.min()), float(vx_img.max()), float(vx_img.mean()))
print("vy stats:", float(vy_img.min()), float(vy_img.max()), float(vy_img.mean()))
print("mag stats:", float(mag.min()), float(mag.max()), float(mag.mean()))
