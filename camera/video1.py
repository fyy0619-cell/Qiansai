#设备节点video2（第二区间）的录音录像程序，不会显示当前摄像头画面

import face_recognition
import cv2
import os
import numpy as np
import subprocess
from threading import Thread
from queue import Queue
import datetime

# 创建视频保存目录
os.makedirs("video", exist_ok=True)

# 加载已知人脸
known_face_encodings = []
known_face_names = []

for filename in os.listdir("/home/j/opendoor/known_faces"):
    image = face_recognition.load_image_file(f"/home/j/opendoor/known_faces/{filename}")
    encodings = face_recognition.face_encodings(image)
    if encodings:
        known_face_encodings.append(encodings[0])
        known_face_names.append(os.path.splitext(filename)[0])

#录像的
class VideoWriterThread(Thread):
    def __init__(self):
        super().__init__()
        self.queue = Queue(maxsize=30)
        self.running = True

        # 设置摄像头参数
        temp_cap = cv2.VideoCapture(0)
        self.frame_size = (640, 480)
        self.fps =  30
        temp_cap.release()

        # 创建视频写入器
        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"/home/j/opendoor/video/video1_{timestamp}.avi"
        self.video_path = filename
        fourcc = cv2.VideoWriter_fourcc(*'MJPG')
        self.out = cv2.VideoWriter(filename, fourcc, self.fps, self.frame_size)

    def run(self):
        while self.running or not self.queue.empty():
            try:
                frame = self.queue.get(timeout=1)
                self.out.write(frame)
            except:
                continue
        self.out.release()

    def get_video_path(self):
        return self.video_path

    def stop(self):
        self.running = False


# 打开摄像头video2
video_capture = cv2.VideoCapture(2)
cpp_exe = False

# 启动视频写入线程
writer_thread = VideoWriterThread()
writer_thread.start()

while True:
    result = None
    ret, frame = video_capture.read()
    if not ret:
        break

    # 在帧上显示当前时间
    cv2.putText(frame, datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"), (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 255), 1)

    # 添加原始帧到写入队列
    if writer_thread.queue.qsize() < 30:  # 防止队列堆积
        writer_thread.queue.put(frame.copy())

    # 缩小图像加快处理速度（BGR）
    small_frame = cv2.resize(frame, (0, 0), fx=0.25, fy=0.25)

    # BGR 转 RGB（face_recognition 需要 RGB）
    rgb_small_frame = cv2.cvtColor(small_frame, cv2.COLOR_BGR2RGB)

    # 检测人脸位置
    face_locations = face_recognition.face_locations(rgb_small_frame)
    # 编码（注意：必须使用 RGB 图像）
    face_encodings = face_recognition.face_encodings(rgb_small_frame, face_locations)

    face_detected = False  # 标记是否检测到人脸

    for (top, right, bottom, left), face_encoding in zip(face_locations, face_encodings):
        name = "Unknown"
        face_distances = face_recognition.face_distance(known_face_encodings, face_encoding)
        if len(face_distances) > 0:
            best_match_index = np.argmin(face_distances)
            if face_distances[best_match_index] < 0.3:  #识别到人脸

                name = known_face_names[best_match_index]
                video_path = writer_thread.get_video_path()
                face_detected = True
                #执行C++
                try:
                    result = subprocess.run(["/home/j/opendoor/open_door"], capture_output=True, text=True) #读取C程序的返回值
                    print("C++ 程序返回码:", result.returncode)
                    print("C++ 程序输出:", result.stdout)
                    if result.returncode == 0:
                        try:
                            video_capture.release()   #释放写入视频进程，防止占用文件无法删除 可有可无研究一下用处
                            writer_thread.stop()
                            writer_thread.join()
                            os.remove(video_path)  #识别到车主，进行录像删除
                        except Exception as e:
                            print(f"删除程序出错: {e}")

                    cpp_executed = True

                except Exception as e:
                    print(f"执行C++程序出错: {e}")


    # 如果检测到人脸，打印1到标准输出(主程序会捕获这个输出)
    if face_detected:
        print("1", flush=True)  # flush确保立即输出

    if result is not None and result.returncode == 0:
        break

video_capture.release()
writer_thread.stop()
writer_thread.join()
