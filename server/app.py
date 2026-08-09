# -*- coding: utf-8 -*-
"""
本地温湿度接收服务器（纯 Python 标准库实现，无需安装任何第三方包）
- STM32(ESP8266) 通过 HTTP POST 把 {"t":温度,"h":湿度} 发到 /api/data
- 浏览器打开 http://<本机IP>:8000/ 查看实时曲线看板
运行：python app.py   （按 Ctrl+C 停止）

说明：本文件只用 Python 自带模块（http/server、socket、csv、json），
不依赖 Flask 等任何第三方库，不向磁盘写入任何新东西。
"""
import json
import csv
import os
import socket
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HOST = "0.0.0.0"
PORT = 8000
MAX_POINTS = 720  # 内存中保留最近 720 个点（约 1 小时，按 5 秒/次）

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_FILE = os.path.join(BASE_DIR, "data.csv")

history = []


def get_lan_ips():
    """获取本机对外的局域网 IP（通过 UDP 探测，不真正发包）。"""
    ips = []
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ips.append(s.getsockname()[0])
        s.close()
    except Exception:
        pass
    return ips


def write_csv(point):
    """把每个数据点追加写入 data.csv（断电重启后仍可保留历史）。"""
    file_exists = os.path.exists(DATA_FILE)
    try:
        with open(DATA_FILE, "a", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            if not file_exists:
                w.writerow(["ts", "temperature", "humidity"])
            w.writerow([point["ts"], point["t"], point["h"]])
    except Exception as e:
        print("[warn] csv 写入失败:", e)


class Handler(BaseHTTPRequestHandler):
    server_version = "EnvSensorServer/1.0"

    def _send(self, code=200, ctype="application/json", body=b""):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Access-Control-Allow-Origin", "*")  # 允许浏览器跨域拉取
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if body:
            self.wfile.write(body)

    def _serve_file(self, name, ctype):
        path = os.path.join(BASE_DIR, name)
        try:
            with open(path, "rb") as f:
                self._send(200, ctype, f.read())
        except FileNotFoundError:
            self._send(404, "text/plain; charset=utf-8", b"not found")

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            self._serve_file("index.html", "text/html; charset=utf-8")
        elif self.path == "/api/history":
            # 返回全部保留点，前端绘图用
            self._send(200, "application/json",
                       json.dumps(history, ensure_ascii=False).encode("utf-8"))
        elif self.path == "/api/latest":
            self._send(200, "application/json",
                       json.dumps(history[-1] if history else {}, ensure_ascii=False).encode("utf-8"))
        else:
            self._send(404, "application/json", b'{"error":"not found"}')

    def do_POST(self):
        if self.path == "/api/data":
            try:
                length = int(self.headers.get("Content-Length", 0))
                raw = self.rfile.read(length) if length else b"{}"
                data = json.loads(raw.decode("utf-8"))
                t = data.get("t")
                h = data.get("h")
                if t is None or h is None:
                    self._send(400, "application/json", b'{"error":"missing t or h"}')
                    return
                point = {
                    "t": int(t),
                    "h": int(h),
                    "ts": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                }
                history.append(point)
                if len(history) > MAX_POINTS:
                    del history[0: len(history) - MAX_POINTS]
                write_csv(point)
                print(f"[recv] t={t} h={h}  已存 {len(history)} 点")
                self._send(200, "application/json", b'{"ok":true}')
            except Exception as e:
                self._send(400, "application/json",
                           json.dumps({"error": str(e)}, ensure_ascii=False).encode("utf-8"))
        else:
            self._send(404, "application/json", b'{"error":"not found"}')

    def log_message(self, *args):
        pass  # 静默默认访问日志，避免刷屏


if __name__ == "__main__":
    ips = get_lan_ips()
    print("=" * 52)
    print(" 温湿度本地服务器已启动")
    print(f" 监听端口: {PORT}")
    if ips:
        print(" 本机局域网 IP（填到 STM32 的 SERVER_IP）：")
        for ip in ips:
            print(f"   -> {ip}")
    else:
        print(" 未能自动探测 IP，请用 ipconfig 自行查看 IPv4 地址")
    print(f" 浏览器看板: http://localhost:{PORT}/")
    print(f" STM32 上传地址: http://<本机IP>:{PORT}/api/data")
    print("=" * 52)
    try:
        ThreadingHTTPServer((HOST, PORT), Handler).serve_forever()
    except KeyboardInterrupt:
        print("\n服务器已停止")
