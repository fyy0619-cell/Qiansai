#通过文件进行通信，分别为carmer_receive和qt_receive文件。
#摄像头开启受到qt程序控制，qt向carmer_receive写入数据后开启摄像头，摄像头识别到人脸后向qt_receive写入数据
#qt读取文件接收到数据后通过串口发送true进行开门。

import serial
import subprocess
import time
import os
import signal
from threading import Thread, Lock
import fcntl

# 文件通信配置
COMMAND_FILE = '/home/j/opendoor/carmer_receive'  # 命令文件路径
RESPONSE_FILE = '/home/j/opendoor/qt_receive'  # Python→QT 的响应文件

# 程序配置
PROGRAMS = {
    1: {'path': '/home/j/opendoor/video0.py', 'venv': '/home/j/myenv/bin/python3', 'process': None, 'active': False},
    2: {'path': '/home/j/opendoor/video1.py', 'venv': '/home/j/myenv/bin/python3', 'process': None, 'active': False},
    3: {'path': '/home/j/opendoor/video2.py', 'venv': '/home/j/myenv/bin/python3', 'process': None, 'active': False},
    4: {'path': '/home/j/opendoor/video3.py', 'venv': '/home/j/myenv/bin/python3', 'process': None, 'active': False}
}

# 线程锁
lock = Lock()

def write_face_detection(program_num, detected):
    """写入人脸检测结果到响应文件"""
    try:
        with open(RESPONSE_FILE, 'w') as f:
            fcntl.flock(f, fcntl.LOCK_EX)
            f.write('1' if detected else '0')
            f.flush()
            fcntl.flock(f, fcntl.LOCK_UN)
    except Exception as e:
        print(f"写入响应文件出错: {e}")


def start_program(program_num):
    """启动指定的程序"""
    with lock:
        if program_num in PROGRAMS:
            program = PROGRAMS[program_num]
            if program['process'] is None or program['process'].poll() is not None:
                # 使用虚拟环境中的Python解释器执行程序
                cmd = [program['venv'], program['path']]
                program['process'] = subprocess.Popen(
                    cmd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    universal_newlines=True
                )
                program['active'] = True
                print(f"程序 {program_num} 已启动")

                # 启动线程监控程序输出
                output_thread = Thread(
                    target=monitor_program_output,
                    args=(program_num,),
                    daemon=True
                )
                output_thread.start()
            else:
                print(f"程序 {program_num} 已经在运行")
        else:
            print(f"无效的程序编号: {program_num}")


def monitor_program_output(program_num):
    """监控程序输出并处理"""
    program = PROGRAMS[program_num]
    while program['process'] is not None and program['process'].poll() is None:
        output = program['process'].stdout.readline()
        if output:
            output = output.strip()
            print(f"程序 {program_num} 输出: {output}")
            if output == "1":  # 检测到人脸
                write_face_detection(program_num, True)
            else:  # 未检测到人脸
                write_face_detection(program_num, False)


def stop_program(program_num):
    """停止指定的程序"""
    with lock:
        if program_num in PROGRAMS:
            program = PROGRAMS[program_num]
            if program['process'] is not None and program['process'].poll() is None:
                # 发送SIGTERM信号，让程序有机会清理资源
                program['process'].terminate()
                try:
                    # 等待5秒让程序正常退出
                    program['process'].wait(timeout=5)
                except subprocess.TimeoutExpired:
                    # 如果程序没有响应，强制终止
                    program['process'].kill()
                    print(f"程序 {program_num} 被强制终止")
                program['process'] = None
                program['active'] = False
                print(f"程序 {program_num} 已停止")
            else:
                print(f"程序 {program_num} 未在运行")
        else:
            print(f"无效的程序编号: {program_num}")


def command_handler():
    """命令处理线程函数，读取文件内容，进行摄像头控制"""
    try:
        # 确保命令文件存在
        if not os.path.exists(COMMAND_FILE):
            open(COMMAND_FILE, 'w').close()

        while True:
            # 读取文件内容作为字节数据
            with open(COMMAND_FILE, 'rb') as f:
                fcntl.flock(f, fcntl.LOCK_SH)
                data = f.read(1)  # 只读取一个字节
                fcntl.flock(f, fcntl.LOCK_UN)

            #打印判断是否接收正确
            if data:
                print(f"接收到字节: {hex(ord(data))}")

                try:
                    byte_value = ord(data)

                    # 处理低四位 (启动命令)
                    start_bits = byte_value & 0x0F
                    for i in range(4):
                        if start_bits & (1 << i):
                            start_program(i + 1)

                    # 处理高四位 (停止命令)
                    stop_bits = (byte_value >> 4) & 0x0F
                    for i in range(4):
                        if stop_bits & (1 << i):
                            stop_program(i + 1)

                    # 清空命令文件
                    with open(COMMAND_FILE, 'wb') as f:
                        fcntl.flock(f, fcntl.LOCK_EX)
                        f.truncate(0)
                        fcntl.flock(f, fcntl.LOCK_UN)

                except Exception as e:
                    print(f"处理命令时出错: {e}")

            time.sleep(0.1)  # 避免CPU占用过高

    except Exception as e:
        print(f"命令处理出错: {e}")


def cleanup():
    """清理所有运行中的程序"""
    print("正在清理所有运行中的程序...")
    for program_num in PROGRAMS:
        stop_program(program_num)
    print("清理完成")


def main():
    # 创建并启动命令处理线程
    command_thread = Thread(target=command_handler, daemon=True)
    command_thread.start()

    try:
        # 主线程保持运行
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n接收到中断信号，准备退出...")
    finally:
        cleanup()
        print("程序退出")


if __name__ == "__main__":
    main()