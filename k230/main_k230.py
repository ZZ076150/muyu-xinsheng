import os
import sys
import ujson
import gc
import math
import time
import utime
import image
import random
import aicube
import network
import aidemo
import socket
import ulab.numpy as np
import nncase_runtime as nn
from machine import UART, FPIOA, Pin
from media.display import *
from media.media import *
from media.display import Display
from libs.PipeLine import PipeLine, ScopedTiming
from libs.AIBase import AIBase
from libs.AI2d import Ai2d
from libs.Utils import *
# ==================== WiFi 视频流配置 ====================
WIFI_SSID = "YOUR_WIFI_SSID"
WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"
SERVER_IP = "YOUR_SERVER_IP"
SERVER_PORT = 9002
STREAM_QUALITY = 30
STREAM_FRAME_SKIP = 3
# ==================== 配置参数 ====================
select_display = 3
fpioa = FPIOA()
fpioa.set_function(3, fpioa.UART1_TXD)
fpioa.set_function(4, fpioa.UART1_RXD)
fpioa.set_function(11, fpioa.UART2_TXD)
fpioa.set_function(12, fpioa.UART2_RXD)
uart_esp8266 = UART(1, 115200)
uart_mcu = UART(2,115200)
# ==================== GPIO配置（使用Pin类）====================
# 配置GPIO引脚功能
fpioa.set_function(5, fpioa.GPIO5)   # 光敏电阻输入引脚（物理引脚5）
fpioa.set_function(6, fpioa.GPIO6)   # LED灯控制引脚（物理引脚6）
# 初始化GPIO
light_sensor = Pin(5, Pin.IN, Pin.PULL_UP)  # 光敏电阻输入（上拉）
led_light = Pin(6, Pin.OUT)                  # LED灯输出
led_light.value(0)  # 初始关闭LED
print("[GPIO] 初始化成功")
print("[GPIO] 光敏电阻: GPIO5 (输入, 上拉)")
print("[GPIO] LED灯: GPIO6 (输出)")
flag_fall = 3
flag_person = 3
count_fall = 0
count_person = 0
frame_cnt = 0
target_face_name = "older"
face_track_enabled = True
send_face_interval = 5
face_lost_threshold = 10
face_lost_count = 0
last_send_time = 0
# 注视方向参数
gaze_send_interval = 1          # 每帧都检查
gaze_frame_count = 0
gaze_threshold_angle = 5        # 阈值5度
gaze_debug_print = True
gaze_detailed_debug = True
IMAGE_CENTER_X = 640
IMAGE_CENTER_Y = 360
# 夜间照明参数
night_light_enabled = True      # 夜间照明功能开关
light_check_interval = 5        # 每5帧检查一次光照条件
light_check_counter = 0
led_status = False              # LED当前状态
target_detected = False         # 是否检测到目标人脸
# ==================== 显示函数 ====================
def init_display(select_display_mode):
    if select_display_mode == 1:
        width, height = 640, 480
        Display.init(Display.LT9611, width=width, height=height, to_ide=True)
        print("初始化HDMI显示，分辨率：" + str(width) + "x" + str(height))
    elif select_display_mode == 2:
        width, height = 800, 480
        Display.init(Display.ST7701, width=width, height=height, to_ide=True)
        print("初始化LCD显示，分辨率：" + str(width) + "x" + str(height))
    elif select_display_mode == 3:
        width, height = 1280, 720
        Display.init(Display.VIRT, width=width, height=height, fps=100, to_ide=True)
        print("初始化IDE虚拟显示，分辨率：" + str(width) + "x" + str(height))
    else:
        raise ValueError("select_display参数错误，只能是1、2或3")
    return width, height
def deinit_display():
    Display.deinit()
    print("释放显示资源")
# ==================== 视频推流函数 ====================
def init_stream():
    """连接 WiFi 和服务器"""
    print("[视频流] 正在连接 WiFi: " + WIFI_SSID)
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    if not wlan.isconnected():
        wlan.connect(WIFI_SSID, WIFI_PASSWORD)
        timeout = 15
        while not wlan.isconnected() and timeout > 0:
            time.sleep_ms(500)
            timeout -= 1
            print(".", end="")
    if not wlan.isconnected():
        print("\n[视频流] ❌ WiFi 连接失败")
        return None
    print("\n[视频流] ✅ WiFi 连接成功, IP: " + wlan.ifconfig()[0])
    print("[视频流] 正在连接服务器 {}:{}...".format(SERVER_IP, SERVER_PORT))
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    try:
        sock.connect((SERVER_IP, SERVER_PORT))
        sock.setblocking(True)
        print("[视频流] ✅ 已连接到服务器")
        return sock
    except Exception as e:
        print("[视频流] ❌ 连接失败: " + str(e))
        sock.close()
        return None
# ==================== 夜间照明控制函数 ====================
def check_night_light():
    """
    检查是否需要开启夜间照明
    返回: True=需要开灯（天黑）, False=不需要开灯（天亮）
    """
    try:
        # 读取光敏电阻状态
        light_value = light_sensor.value()
        # 修正：光敏电阻输出1（高电平）= 天黑，0（低电平）= 天亮
        is_dark = (light_value == 1)
        if gaze_debug_print:
            print("[夜间照明] 光敏电阻值: {}, 是否天黑: {}".format(light_value, is_dark))
        return is_dark
    except Exception as e:
        print("[夜间照明] 读取光敏电阻失败: " + str(e))
        return False
def control_night_light(enable):
    """
    控制LED灯开关
    enable: True=开灯, False=关灯
    """
    global led_status
    try:
        if enable and not led_status:
            led_light.value(1)  # 高电平点亮LED
            led_status = True
            print("[夜间照明] 💡 LED灯已开启")
        elif not enable and led_status:
            led_light.value(0)  # 低电平关闭LED
            led_status = False
            print("[夜间照明] LED灯已关闭")
    except Exception as e:
        print("[夜间照明] 控制LED失败: " + str(e))
# ==================== 人脸检测类 ====================
class FaceDetApp(AIBase):
    def __init__(self, kmodel_path, model_input_size, anchors, confidence_threshold=0.25,
                 nms_threshold=0.3, rgb888p_size=[1920,1080], display_size=[1920,1080], debug_mode=0):
        super().__init__(kmodel_path, model_input_size, rgb888p_size, debug_mode)
        self.kmodel_path = kmodel_path
        self.model_input_size = model_input_size
        self.confidence_threshold = confidence_threshold
        self.nms_threshold = nms_threshold
        self.anchors = anchors
        self.rgb888p_size = [ALIGN_UP(rgb888p_size[0], 16), rgb888p_size[1]]
        self.display_size = [ALIGN_UP(display_size[0], 16), display_size[1]]
        self.debug_mode = debug_mode
        self.ai2d = Ai2d(debug_mode)
        self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT, nn.ai2d_format.NCHW_FMT, np.uint8, np.uint8)
    def config_preprocess(self, input_image_size=None):
        with ScopedTiming("set preprocess config", self.debug_mode > 0):
            ai2d_input_size = input_image_size if input_image_size else self.rgb888p_size
            top, bottom, left, right, _ = letterbox_pad_param(self.rgb888p_size, self.model_input_size)
            self.ai2d.pad([0, 0, 0, 0, top, bottom, left, right], 0, [104, 117, 123])
            self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
            self.ai2d.build([1, 3, ai2d_input_size[1], ai2d_input_size[0]],
                           [1, 3, self.model_input_size[1], self.model_input_size[0]])
    def postprocess(self, results):
        with ScopedTiming("postprocess", self.debug_mode > 0):
            res = aidemo.face_det_post_process(self.confidence_threshold, self.nms_threshold,
                                               self.model_input_size[0], self.anchors,
                                               self.rgb888p_size, results)
            if len(res) == 0:
                return res, res
            else:
                return res[0], res[1]
# ==================== 人脸识别类 ====================
class FaceRegistrationApp(AIBase):
    def __init__(self, kmodel_path, model_input_size, rgb888p_size=[1920,1080],
                 display_size=[1920,1080], debug_mode=0):
        super().__init__(kmodel_path, model_input_size, rgb888p_size, debug_mode)
        self.kmodel_path = kmodel_path
        self.model_input_size = model_input_size
        self.rgb888p_size = [ALIGN_UP(rgb888p_size[0], 16), rgb888p_size[1]]
        self.display_size = [ALIGN_UP(display_size[0], 16), display_size[1]]
        self.debug_mode = debug_mode
        self.umeyama_args_112 = [38.2946, 51.6963, 73.5318, 51.5014, 56.0252, 71.7366,
                                 41.5493, 92.3655, 70.7299, 92.2041]
        self.ai2d = Ai2d(debug_mode)
        self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT, nn.ai2d_format.NCHW_FMT, np.uint8, np.uint8)
    def config_preprocess(self, landm, input_image_size=None):
        with ScopedTiming("set preprocess config", self.debug_mode > 0):
            ai2d_input_size = input_image_size if input_image_size else self.rgb888p_size
            affine_matrix = self.get_affine_matrix(landm)
            self.ai2d.affine(nn.interp_method.cv2_bilinear, 0, 0, 127, 1, affine_matrix)
            self.ai2d.build([1, 3, ai2d_input_size[1], ai2d_input_size[0]],
                           [1, 3, self.model_input_size[1], self.model_input_size[0]])
    def postprocess(self, results):
        with ScopedTiming("postprocess", self.debug_mode > 0):
            return results[0][0]
    def svd22(self, a):
        s = [0.0, 0.0]
        u = [0.0, 0.0, 0.0, 0.0]
        v = [0.0, 0.0, 0.0, 0.0]
        s[0] = (math.sqrt((a[0] - a[3]) ** 2 + (a[1] + a[2]) ** 2) +
                math.sqrt((a[0] + a[3]) ** 2 + (a[1] - a[2]) ** 2)) / 2
        s[1] = abs(s[0] - math.sqrt((a[0] - a[3]) ** 2 + (a[1] + a[2]) ** 2))
        v[2] = math.sin((math.atan2(2 * (a[0] * a[1] + a[2] * a[3]),
                                    a[0] ** 2 - a[1] ** 2 + a[2] ** 2 - a[3] ** 2)) / 2) if s[0] > s[1] else 0
        v[0] = math.sqrt(1 - v[2] ** 2)
        v[1] = -v[2]
        v[3] = v[0]
        u[0] = -(a[0] * v[0] + a[1] * v[2]) / s[0] if s[0] != 0 else 1
        u[2] = -(a[2] * v[0] + a[3] * v[2]) / s[0] if s[0] != 0 else 0
        u[1] = (a[0] * v[1] + a[1] * v[3]) / s[1] if s[1] != 0 else -u[2]
        u[3] = (a[2] * v[1] + a[3] * v[3]) / s[1] if s[1] != 0 else u[0]
        v[0] = -v[0]
        v[2] = -v[2]
        return u, s, v
    def image_umeyama_112(self, src):
        SRC_NUM = 5
        SRC_DIM = 2
        src_mean = [0.0, 0.0]
        dst_mean = [0.0, 0.0]
        for i in range(0, SRC_NUM * 2, 2):
            src_mean[0] += src[i]
            src_mean[1] += src[i + 1]
            dst_mean[0] += self.umeyama_args_112[i]
            dst_mean[1] += self.umeyama_args_112[i + 1]
        src_mean[0] /= SRC_NUM
        src_mean[1] /= SRC_NUM
        dst_mean[0] /= SRC_NUM
        dst_mean[1] /= SRC_NUM
        src_demean = [[0.0, 0.0] for _ in range(SRC_NUM)]
        dst_demean = [[0.0, 0.0] for _ in range(SRC_NUM)]
        for i in range(SRC_NUM):
            src_demean[i][0] = src[2 * i] - src_mean[0]
            src_demean[i][1] = src[2 * i + 1] - src_mean[1]
            dst_demean[i][0] = self.umeyama_args_112[2 * i] - dst_mean[0]
            dst_demean[i][1] = self.umeyama_args_112[2 * i + 1] - dst_mean[1]
        A = [[0.0, 0.0], [0.0, 0.0]]
        for i in range(SRC_DIM):
            for k in range(SRC_DIM):
                for j in range(SRC_NUM):
                    A[i][k] += dst_demean[j][i] * src_demean[j][k]
                A[i][k] /= SRC_NUM
        T = [[1, 0, 0], [0, 1, 0], [0, 0, 1]]
        U, S, V = self.svd22([A[0][0], A[0][1], A[1][0], A[1][1]])
        T[0][0] = U[0] * V[0] + U[1] * V[2]
        T[0][1] = U[0] * V[1] + U[1] * V[3]
        T[1][0] = U[2] * V[0] + U[3] * V[2]
        T[1][1] = U[2] * V[1] + U[3] * V[3]
        scale = 1.0
        src_demean_mean = [0.0, 0.0]
        src_demean_var = [0.0, 0.0]
        for i in range(SRC_NUM):
            src_demean_mean[0] += src_demean[i][0]
            src_demean_mean[1] += src_demean[i][1]
        src_demean_mean[0] /= SRC_NUM
        src_demean_mean[1] /= SRC_NUM
        for i in range(SRC_NUM):
            src_demean_var[0] += (src_demean_mean[0] - src_demean[i][0]) * (src_demean_mean[0] - src_demean[i][0])
            src_demean_var[1] += (src_demean_mean[1] - src_demean[i][1]) * (src_demean_mean[1] - src_demean[i][1])
        src_demean_var[0] /= SRC_NUM
        src_demean_var[1] /= SRC_NUM
        scale = 1.0 / (src_demean_var[0] + src_demean_var[1]) * (S[0] + S[1])
        T[0][2] = dst_mean[0] - scale * (T[0][0] * src_mean[0] + T[0][1] * src_mean[1])
        T[1][2] = dst_mean[1] - scale * (T[1][0] * src_mean[0] + T[1][1] * src_mean[1])
        T[0][0] *= scale
        T[0][1] *= scale
        T[1][0] *= scale
        T[1][1] *= scale
        return T
    def get_affine_matrix(self, sparse_points):
        with ScopedTiming("get_affine_matrix", self.debug_mode > 1):
            matrix_dst = self.image_umeyama_112(sparse_points)
            matrix_dst = [matrix_dst[0][0], matrix_dst[0][1], matrix_dst[0][2],
                          matrix_dst[1][0], matrix_dst[1][1], matrix_dst[1][2]]
            return matrix_dst
# ==================== 注视估计类（方案2：后处理坐标转换）====================
class EyeGazeApp(AIBase):
    def __init__(self, kmodel_path, model_input_size, rgb888p_size=[1920,1080],
                 display_size=[1920,1080], debug_mode=0):
        super().__init__(kmodel_path, model_input_size, rgb888p_size, debug_mode)
        self.kmodel_path = kmodel_path
        self.model_input_size = model_input_size
        self.rgb888p_size = [ALIGN_UP(rgb888p_size[0], 16), rgb888p_size[1]]
        self.display_size = [ALIGN_UP(display_size[0], 16), display_size[1]]
        self.debug_mode = debug_mode
        self.ai2d = Ai2d(debug_mode)
        self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT, nn.ai2d_format.NCHW_FMT, np.uint8, np.uint8)
    def config_preprocess(self, det, input_image_size=None):
        with ScopedTiming("set preprocess config", self.debug_mode > 0):
            ai2d_input_size = input_image_size if input_image_size else self.rgb888p_size
            x, y, w, h = map(lambda x: int(round(x, 0)), det[:4])
            if gaze_detailed_debug:
                print("[注视预处理] crop区域: x={}, y={}, w={}, h={}".format(x, y, w, h))
            self.ai2d.crop(x, y, w, h)
            self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
            self.ai2d.build([1, 3, ai2d_input_size[1], ai2d_input_size[0]],
                           [1, 3, self.model_input_size[1], self.model_input_size[0]])
    def postprocess(self, results):
        with ScopedTiming("postprocess", self.debug_mode > 0):
            if gaze_detailed_debug:
                print("[注视后处理] 模型输出结果数量: {}".format(len(results)))
                for idx, r in enumerate(results):
                    if hasattr(r, 'shape'):
                        print("[注视后处理]   输出[{}] shape: {}".format(idx, r.shape))
                    else:
                        print("[注视后处理]   输出[{}] type: {}, len: {}".format(idx, type(r), len(r) if hasattr(r, '__len__') else 'N/A'))
            post_ret = aidemo.eye_gaze_post_process(results)
            pitch_raw = post_ret[0]
            yaw_raw = post_ret[1]
            pitch_converted = -yaw_raw
            yaw_converted = pitch_raw
            if gaze_detailed_debug:
                print("[注视后处理] ===== 坐标转换 =====")
                print("[注视后处理] 原始pitch(rad): {:.6f}, 原始yaw(rad): {:.6f}".format(pitch_raw, yaw_raw))
                print("[注视后处理] 转换后pitch(rad): {:.6f}, 转换后yaw(rad): {:.6f}".format(
                    pitch_converted, yaw_converted))
            return pitch_converted, yaw_converted
# ==================== 人脸识别任务类（集成注视估计）====================
class FaceRecognition:
    def __init__(self, face_det_kmodel, face_reg_kmodel, eye_gaze_kmodel,
                 det_input_size, reg_input_size, eye_gaze_input_size,
                 database_dir, anchors, confidence_threshold=0.25, nms_threshold=0.3,
                 face_recognition_threshold=0.75, rgb888p_size=[1280,720],
                 display_size=[1920,1080], debug_mode=0):
        self.face_det_kmodel = face_det_kmodel
        self.face_reg_kmodel = face_reg_kmodel
        self.eye_gaze_kmodel = eye_gaze_kmodel
        self.det_input_size = det_input_size
        self.reg_input_size = reg_input_size
        self.eye_gaze_input_size = eye_gaze_input_size
        self.database_dir = database_dir
        self.anchors = anchors
        self.confidence_threshold = confidence_threshold
        self.nms_threshold = nms_threshold
        self.face_recognition_threshold = face_recognition_threshold
        self.rgb888p_size = [ALIGN_UP(rgb888p_size[0], 16), rgb888p_size[1]]
        self.display_size = [ALIGN_UP(display_size[0], 16), display_size[1]]
        self.debug_mode = debug_mode
        self.max_register_face = 100
        self.feature_num = 128
        self.valid_register_face = 0
        self.db_name = []
        self.db_data = []
        self.gaze_counter = 0
        self.gaze_pitch_sum = 0
        self.gaze_yaw_sum = 0
        self.face_det = FaceDetApp(self.face_det_kmodel, model_input_size=self.det_input_size,
                                   anchors=self.anchors, confidence_threshold=self.confidence_threshold,
                                   nms_threshold=self.nms_threshold, rgb888p_size=self.rgb888p_size,
                                   display_size=self.display_size, debug_mode=0)
        self.face_reg = FaceRegistrationApp(self.face_reg_kmodel, model_input_size=self.reg_input_size,
                                            rgb888p_size=self.rgb888p_size, display_size=self.display_size)
        self.eye_gaze = EyeGazeApp(self.eye_gaze_kmodel, model_input_size=self.eye_gaze_input_size,
                                    rgb888p_size=self.rgb888p_size, display_size=self.display_size)
        self.face_det.config_preprocess()
        self.database_init()
        print("[人脸库] 已加载 " + str(self.valid_register_face) + " 个人脸:")
        for i, name in enumerate(self.db_name):
            print("  " + str(i+1) + ". " + name)
    def run(self, input_np, target_name=None):
        det_boxes, landms = self.face_det.run(input_np)
        recg_res = []
        gaze_res = []
        for landm in landms:
            self.face_reg.config_preprocess(landm)
            feature = self.face_reg.run(input_np)
            res = self.database_search(feature)
            recg_res.append(res)
        for i in range(len(det_boxes)):
            if target_name is None or recg_res[i]['name'] == target_name:
                print("\n" + "=" * 60)
                print("[注视调试-帧{}] 对人脸 '{}' 进行注视估计".format(
                    self.gaze_counter, recg_res[i]['name']))
                print("[注视调试] 人脸框: x={:.1f}, y={:.1f}, w={:.1f}, h={:.1f}".format(
                    det_boxes[i][0], det_boxes[i][1], det_boxes[i][2], det_boxes[i][3]))
                self.eye_gaze.config_preprocess(det_boxes[i])
                pitch, yaw = self.eye_gaze.run(input_np)
                pitch_deg = math.degrees(pitch)
                yaw_deg = math.degrees(yaw)
                print("[注视调试] ===== 转换后输出 =====")
                print("[注视调试] pitch(rad): {:.6f}".format(pitch))
                print("[注视调试] yaw(rad):   {:.6f}".format(yaw))
                print("[注视调试] pitch(deg): {:.2f}°".format(pitch_deg))
                print("[注视调试] yaw(deg):   {:.2f}°".format(yaw_deg))
                self.gaze_counter += 1
                self.gaze_pitch_sum += pitch
                self.gaze_yaw_sum += yaw
                if self.gaze_counter > 0:
                    avg_pitch = self.gaze_pitch_sum / self.gaze_counter
                    avg_yaw = self.gaze_yaw_sum / self.gaze_counter
                    print("[注视调试] ===== 累计统计 =====")
                    print("[注视调试] 累计帧数: {}".format(self.gaze_counter))
                    print("[注视调试] 平均pitch(deg): {:.2f}°".format(math.degrees(avg_pitch)))
                    print("[注视调试] 平均yaw(deg):   {:.2f}°".format(math.degrees(avg_yaw)))
                direction = get_gaze_direction(pitch, yaw, gaze_threshold_angle)
                print("[注视调试] ===== 方向判断 =====")
                print("[注视调试] 阈值: ±{}°".format(gaze_threshold_angle))
                print("[注视调试] yaw_deg={:.2f}, 判断结果: {}".format(yaw_deg, direction))
                if abs(yaw) < 0.0001:
                    print("[注视调试] ⚠️ WARNING: yaw值接近0")
                print("=" * 60 + "\n")
                gaze_res.append((pitch, yaw))
            else:
                if gaze_detailed_debug:
                    print("[注视调试] 跳过人脸 '{}' (不是目标 '{}')".format(
                        recg_res[i]['name'], target_name))
                gaze_res.append((0.0, 0.0))
        return det_boxes, recg_res, gaze_res
    def database_init(self):
        with ScopedTiming("database_init", self.debug_mode > 1):
            db_file_list = os.listdir(self.database_dir)
            for db_file in db_file_list:
                if not db_file.endswith('.bin'):
                    continue
                if self.valid_register_face >= self.max_register_face:
                    break
                full_db_file = self.database_dir + db_file
                with open(full_db_file, 'rb') as f:
                    data = f.read()
                feature = np.frombuffer(data, dtype=np.float)
                self.db_data.append(feature)
                name = db_file.split('.')[0]
                self.db_name.append(name)
                self.valid_register_face += 1
    def database_search(self, feature):
        with ScopedTiming("database_search", self.debug_mode > 1):
            v_id = -1
            v_score_max = 0.0
            feature /= np.linalg.norm(feature)
            for i in range(self.valid_register_face):
                db_feature = self.db_data[i]
                db_feature /= np.linalg.norm(db_feature)
                v_score = np.dot(feature, db_feature) / 2 + 0.5
                if v_score > v_score_max:
                    v_score_max = v_score
                    v_id = i
            if v_id == -1:
                return {'name': 'unknown', 'score': 0.0}
            elif v_score_max < self.face_recognition_threshold:
                return {'name': 'unknown', 'score': v_score_max}
            else:
                return {'name': self.db_name[v_id], 'score': v_score_max}
    def draw_result(self, pl, dets, recg_results, gaze_results):
        pl.osd_img.clear()
        if dets:
            for i, det in enumerate(dets):
                x1, y1, w, h = map(lambda x: int(round(x, 0)), det[:4])
                x1 = x1 * self.display_size[0] // self.rgb888p_size[0]
                y1 = y1 * self.display_size[1] // self.rgb888p_size[1]
                w = w * self.display_size[0] // self.rgb888p_size[0]
                h = h * self.display_size[1] // self.rgb888p_size[1]
                if recg_results[i]['name'] != 'unknown':
                    if recg_results[i]['name'] == target_face_name:
                        pl.osd_img.draw_rectangle(x1, y1, w, h, color=(255, 0, 255, 0), thickness=4)
                    else:
                        pl.osd_img.draw_rectangle(x1, y1, w, h, color=(255, 255, 0, 0), thickness=2)
                    recg_text = recg_results[i]['name'] + " " + str(round(recg_results[i]['score'], 2))
                    pl.osd_img.draw_string_advanced(x1, y1 - 50, 24, recg_text, color=(255, 0, 255, 0))
                else:
                    pl.osd_img.draw_rectangle(x1, y1, w, h, color=(255, 0, 0, 255), thickness=4)
                    recg_text = "unknown"
                    pl.osd_img.draw_string_advanced(x1, y1 - 50, 24, recg_text, color=(255, 255, 0, 0))
                if gaze_results and len(gaze_results) > i:
                    pitch, yaw = gaze_results[i]
                    if pitch != 0.0 or yaw != 0.0:
                        center_x = x1 + w // 2
                        center_y = y1 + h // 2
                        length = min(w, h) // 2
                        dx = -length * math.sin(pitch) * math.cos(yaw)
                        dy = -length * math.sin(yaw)
                        target_x = int(center_x + dx)
                        target_y = int(center_y + dy)
                        pl.osd_img.draw_arrow(int(center_x), int(center_y),
                                             target_x, target_y,
                                             color=(255, 255, 255, 0),
                                             size=20, thickness=2)
                        direction = get_gaze_direction(pitch, yaw, gaze_threshold_angle)
                        gaze_text = "{} P:{:.0f} Y:{:.0f}".format(
                            direction, math.degrees(pitch), math.degrees(yaw))
                        pl.osd_img.draw_string_advanced(x1, y1 + h + 5, 20, gaze_text, color=(255, 255, 255, 0))
                        if recg_results[i]['name'] == target_face_name:
                            pl.osd_img.draw_string_advanced(x1, y1 - 30, 20,
                                "[TARGET] " + direction, color=(255, 255, 255, 0))
# ==================== 跌倒检测类 ====================
class FallDetectionApp(AIBase):
    def __init__(self, kmodel_path, model_input_size, labels, anchors,
                 confidence_threshold=0.2, nms_threshold=0.5, nms_option=False,
                 strides=[8,16,32], rgb888p_size=[224,224], display_size=[1920,1080],
                 debug_mode=0):
        super().__init__(kmodel_path, model_input_size, rgb888p_size, debug_mode)
        self.kmodel_path = kmodel_path
        self.model_input_size = model_input_size
        self.labels = labels
        self.anchors = anchors
        self.strides = strides
        self.confidence_threshold = confidence_threshold
        self.nms_threshold = nms_threshold
        self.nms_option = nms_option
        self.rgb888p_size = [ALIGN_UP(rgb888p_size[0], 16), rgb888p_size[1]]
        self.display_size = [ALIGN_UP(display_size[0], 16), display_size[1]]
        self.debug_mode = debug_mode
        self.color = [(255, 0, 0, 255), (255, 0, 255, 0), (255, 255, 0, 0), (255, 255, 0, 255)]
        self.ai2d = Ai2d(debug_mode)
        self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT, nn.ai2d_format.NCHW_FMT, np.uint8, np.uint8)
    def config_preprocess(self, input_image_size=None):
        with ScopedTiming("set preprocess config", self.debug_mode > 0):
            ai2d_input_size = input_image_size if input_image_size else self.rgb888p_size
            top, bottom, left, right = self.get_padding_param()
            self.ai2d.pad([0, 0, 0, 0, top, bottom, left, right], 0, [0, 0, 0])
            self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
            self.ai2d.build([1, 3, ai2d_input_size[1], ai2d_input_size[0]],
                           [1, 3, self.model_input_size[1], self.model_input_size[0]])
    def postprocess(self, results):
        with ScopedTiming("postprocess", self.debug_mode > 0):
            dets = aicube.anchorbasedet_post_process(
                results[0], results[1], results[2], self.model_input_size,
                self.rgb888p_size, self.strides, len(self.labels),
                self.confidence_threshold, self.nms_threshold, self.anchors, self.nms_option
            )
            return dets
    def draw_result(self, pl, dets):
        with ScopedTiming("display_draw", self.debug_mode > 0):
            if dets:
                for det_box in dets:
                    x1, y1, x2, y2 = det_box[2], det_box[3], det_box[4], det_box[5]
                    w = (x2 - x1) * self.display_size[0] // self.rgb888p_size[0]
                    h = (y2 - y1) * self.display_size[1] // self.rgb888p_size[1]
                    x1 = int(x1 * self.display_size[0] // self.rgb888p_size[0])
                    y1 = int(y1 * self.display_size[1] // self.rgb888p_size[1])
                    pl.osd_img.draw_rectangle(x1, y1, int(w), int(h),
                                             color=self.color[det_box[0]], thickness=2)
                    pl.osd_img.draw_string_advanced(x1, y1 - 50, 32,
                        " " + self.labels[det_box[0]] + " " + str(round(det_box[1], 2)),
                        color=self.color[det_box[0]])
    def get_padding_param(self):
        dst_w = self.model_input_size[0]
        dst_h = self.model_input_size[1]
        input_width = self.rgb888p_size[0]
        input_high = self.rgb888p_size[1]
        ratio_w = dst_w / input_width
        ratio_h = dst_h / input_high
        ratio = ratio_w if ratio_w < ratio_h else ratio_h
        new_w = int(ratio * input_width)
        new_h = int(ratio * input_high)
        dw = (dst_w - new_w) / 2
        dh = (dst_h - new_h) / 2
        top = int(round(dh - 0.1))
        bottom = int(round(dh + 0.1))
        left = int(round(dw - 0.1))
        right = int(round(dw - 0.1))
        return top, bottom, left, right
# ==================== 人脸位置计算函数 ====================
def calculate_face_position(det_boxes, recg_results, image_width, image_height, target_name):
    target_index = -1
    for i, result in enumerate(recg_results):
        if result['name'] == target_name:
            target_index = i
            break
    if target_index == -1:
        return False, 0, 0, 0, 0, 0, 0
    det = det_boxes[target_index]
    x1, y1, w, h = det[:4]
    face_center_x = int(x1 + w / 2)
    face_center_y = int(y1 + h / 2)
    offset_x = face_center_x - image_width // 2
    offset_y = face_center_y - image_height // 2
    return True, face_center_x, face_center_y, int(w), int(h), offset_x, offset_y
def send_face_position_to_esp8266(uart, face_center_x, face_center_y, face_width, face_height, offset_x, offset_y):
    try:
        data_str = "FACE,{},{},{},{},{},{}\r\n".format(
            face_center_x, face_center_y, face_width, face_height, offset_x, offset_y
        )
        uart.write(data_str.encode())
        return True
    except Exception as e:
        return False
# ==================== 注视方向计算和发送函数 ====================
def get_gaze_direction(pitch, yaw, threshold_angle=5):
    """
    根据yaw角度判断注视方向（只看左右）
    yaw < -threshold: left
    yaw > +threshold: right
    其他: center
    """
    yaw_deg = math.degrees(yaw)
    if yaw_deg < -threshold_angle:
        return "left"
    elif yaw_deg > threshold_angle:
        return "right"
    else:
        return "center"
def send_gaze_to_esp8266(uart, direction, pitch, yaw):
    """
    通过串口发送注视方向到ESP8266
    格式: left 或 right 或 center
    """
    try:
        data_str = "{}\r\n".format(direction)
        uart.write(data_str.encode())
        print("[ESP8266] 发送注视方向: " + data_str.strip())
        return True
    except Exception as e:
        print("[ESP8266] 发送失败: " + str(e))
        return False
# ==================== 主程序 ====================
if __name__ == "__main__":
    # 初始化显示
    display_output_width, display_output_height = init_display(select_display)
    actual_display_size = [ALIGN_UP(display_output_width, 16), display_output_height]
    # 人脸识别配置
    rgb888p_size_face = [1920, 1080]
    face_det_kmodel_path = "/sdcard/examples/kmodel/face_detection_320.kmodel"
    face_reg_kmodel_path = "/sdcard/examples/kmodel/face_recognition.kmodel"
    eye_gaze_kmodel_path = "/sdcard/examples/kmodel/eye_gaze.kmodel"
    anchors_path = "/sdcard/examples/utils/prior_data_320.bin"
    database_dir = "/sdcard/examples/utils/db/"
    face_det_input_size = [320, 320]
    face_reg_input_size = [112, 112]
    eye_gaze_input_size = [448, 448]
    confidence_threshold = 0.5
    nms_threshold = 0.2
    anchor_len = 4200
    det_dim = 4
    anchors = np.fromfile(anchors_path, dtype=np.float)
    anchors = anchors.reshape((anchor_len, det_dim))
    face_recognition_threshold = 0.6
    # 跌倒检测配置
    rgb888p_size_fall = [1920, 1080]
    kmodel_path_fall = "/sdcard/examples/kmodel/yolov5n-falldown.kmodel"
    confidence_threshold_fall = 0.3
    nms_threshold_fall = 0.45
    labels_fall = ["Fall", "NoFall"]
    anchors_fall = [10, 13, 16, 30, 33, 23, 30, 61, 62, 45, 59, 119, 116, 90, 156, 198, 373, 326]
    # 初始化PipeLine
    pl = PipeLine(rgb888p_size=rgb888p_size_fall, display_size=actual_display_size)
    pl.create()
    stream_sensor = pl.sensor
    # 初始化人脸识别
    fr = FaceRecognition(
        face_det_kmodel_path, face_reg_kmodel_path, eye_gaze_kmodel_path,
        det_input_size=face_det_input_size, reg_input_size=face_reg_input_size,
        eye_gaze_input_size=eye_gaze_input_size,
        database_dir=database_dir, anchors=anchors,
        confidence_threshold=confidence_threshold, nms_threshold=nms_threshold,
        face_recognition_threshold=face_recognition_threshold,
        rgb888p_size=rgb888p_size_face, display_size=actual_display_size
    )
    # 初始化跌倒检测
    fall_det = FallDetectionApp(
        kmodel_path_fall, model_input_size=[640, 640],
        labels=labels_fall, anchors=anchors_fall,
        confidence_threshold=confidence_threshold_fall,
        nms_threshold=nms_threshold_fall, nms_option=False,
        strides=[8, 16, 32], rgb888p_size=rgb888p_size_fall,
        display_size=actual_display_size, debug_mode=0
    )
    fall_det.config_preprocess()
    # 初始化视频推流
    stream_sock = init_stream()
    print("=" * 60)
    print("程序启动 - 注视方向发送到ESP8266")
    print("目标人脸: " + target_face_name)
    print("ESP8266 UART: uart_esp8266 (UART1)")
    print("MCU UART（GD32 主控）:  uart_mcu (UART2)")
    print("注视阈值: ±{}°".format(gaze_threshold_angle))
    print("注视估计使用后处理坐标转换")
    print("夜间照明功能: 已启用")
    print("  - 光敏电阻: GPIO5 (输入)")
    print("  - LED灯: GPIO6 (输出)")
    print("  - 天黑=高电平(1), 天亮=低电平(0)")
    print("=" * 60)
    debug_frame_count = 0
    stream_frame_count = 0
    stream_success = 0
    stream_drop = 0
    last_gaze_direction = "center"
    try:
        while True:
            os.exitpoint()
            with ScopedTiming("total", 1):
                img = pl.get_frame()
                debug_frame_count += 1
                # ========== 视频推流 ==========
                if stream_sock and stream_sensor:
                    stream_frame_count += 1
                    if stream_frame_count % STREAM_FRAME_SKIP == 0:
                        try:
                            raw_img = stream_sensor.snapshot()
                            if raw_img:
                                jpeg = raw_img.to_jpeg(quality=STREAM_QUALITY)
                                header = b'\xAA\xBB\xCC\xDD'
                                length = len(jpeg).to_bytes(4, 'big')
                                data = header + length + jpeg
                                try:
                                    stream_sock.sendall(data)
                                    stream_success += 1
                                except OSError:
                                    stream_drop += 1
                            else:
                                stream_drop += 1
                        except Exception as e:
                            stream_drop += 1
                            print("[视频流] 异常:", e)
                # ---- 人脸识别 + 注视估计 ----
                det_boxes, recg_res, gaze_res = fr.run(img, target_name=target_face_name)
                fr.draw_result(pl, det_boxes, recg_res, gaze_res)
                # ---- 夜间照明控制 ----
                if night_light_enabled:
                    light_check_counter += 1
                    if light_check_counter >= light_check_interval:
                        light_check_counter = 0
                        # 检查是否检测到目标人脸
                        target_detected = False
                        for result in recg_res:
                            if result['name'] == target_face_name:
                                target_detected = True
                                break
                        if target_detected:
                            # 检测到目标人脸，检查光照条件
                            is_dark = check_night_light()
                            control_night_light(is_dark)
                        else:
                            # 没有检测到目标人脸，关闭LED
                            control_night_light(False)
                # ---- 注视方向发送到ESP8266 ----
                if len(det_boxes) > 0:
                    target_index = -1
                    for i, result in enumerate(recg_res):
                        if result['name'] == target_face_name:
                            target_index = i
                            break
                    if target_index != -1:
                        pitch, yaw = gaze_res[target_index]
                        direction = get_gaze_direction(pitch, yaw, gaze_threshold_angle)
                        if direction != last_gaze_direction:
                            send_gaze_to_esp8266(uart_esp8266, direction, pitch, yaw)
                            last_gaze_direction = direction
                # ---- 人脸追踪 ----
                if face_track_enabled and len(det_boxes) > 0:
                    found, center_x, center_y, fw, fh, off_x, off_y = calculate_face_position(
                        det_boxes, recg_res, rgb888p_size_face[0], rgb888p_size_face[1], target_face_name
                    )
                    if found:
                        face_lost_count = 0
                        frame_cnt += 1
                        if frame_cnt % send_face_interval == 0:
                            send_face_position_to_esp8266(
                                uart_mcu, center_x, center_y, fw, fh, off_x, off_y
                            )
                    else:
                        face_lost_count += 1
                        if face_lost_count >= face_lost_threshold:
                            uart_esp8266.write(b"FACE_LOST\r\n")
                # ---- 跌倒检测（修改：fall任何人都触发；person仅识别到老人才发送）----
                res = fall_det.run(img)
                has_person = False
                is_fall = False
                # 判断当前帧是否识别到目标老人older
                has_target_older = any(rec['name'] == target_face_name for rec in recg_res)

                if res:
                    has_person = True
                    for box in res:
                        if box[0] == 0:
                            is_fall = True
                            break
                # fall：任何人摔倒都会上报
                if is_fall:
                    count_fall += 1
                else:
                    count_fall = 0
                if count_fall >= flag_fall:
                    print("[报警] 跌倒检测触发")
                    uart_esp8266.write(b"fall\r\n")
                    count_fall = 0

                # person：只有识别到老人才计数发送
                if has_target_older:
                    if has_person:
                        count_person += 1
                    else:
                        count_person = 0
                    if count_person >= flag_person:
                        print("[通知] 识别到老人")
                        uart_esp8266.write(b"person\r\n")
                        count_person = 0
                else:
                    # 没有识别到老人，清空person计数，不发送person
                    count_person = 0

                fall_det.draw_result(pl, res)
                pl.show_image()
                gc.collect()
    except KeyboardInterrupt:
        print("程序被用户中断")
    except Exception as e:
        print("程序异常: " + str(e))
        sys.print_exception(e)
    finally:
        # 关闭LED
        control_night_light(False)
        if stream_sock:
            try:
                stream_sock.close()
            except:
                pass
        print("[视频流] 已关闭 (发送:{} 丢弃:{})".format(stream_success, stream_drop))
        fr.face_det.deinit()
        fr.face_reg.deinit()
        fr.eye_gaze.deinit()
        fall_det.deinit()
        pl.destroy()
        deinit_display()
        print("程序结束")
