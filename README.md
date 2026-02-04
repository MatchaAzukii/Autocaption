# Autocaption

**This is a product of vibe coding. Use at your own risk!**

实时系统音频转录桌面应用程序。捕获系统音频输出，执行低延迟语音识别，并通过 WebSocket 提供字幕。

## 功能特性

- 🎤 **系统音频捕获**: 跨平台支持 (Windows WASAPI, macOS CoreAudio, Linux PulseAudio)
- 🧠 **本地语音识别**: 基于 whisper.cpp，完全本地运行，无需云端
- 🌐 **WebSocket 实时传输**: 字幕通过 WebSocket 实时推送到 Web UI
- 🎨 **双模式界面**: 
  - **Web 浏览器模式**: 简单快捷，适合测试
  - **Tauri 桌面应用**: 真正的悬浮窗、系统托盘、全局快捷键
- ⚡ **低延迟**: 滑动窗口设计，边说边出字幕
- 🔒 **隐私安全**: 所有处理本地完成，数据不离开您的设备

## 系统要求

- **Windows**: Windows 10/11, Visual Studio 2019+ 或 MinGW
- **macOS**: macOS 10.15+, Xcode Command Line Tools, BlackHole 2ch
- **Linux**: Ubuntu 20.04+, PulseAudio, GCC 10+
- **通用**: CMake 3.20+, Rust (用于 Tauri), 4GB+ RAM

## 快速开始

### 1. 克隆仓库

```bash
git clone --recursive https://github.com/yourusername/autocaption.git
cd autocaption
```

### 2. 下载 Whisper 模型

```bash
./scripts/download_models.sh base
```

### 3. 构建 C++ 后端

```bash
./scripts/build.sh
```

### 4. 运行 (选择一种方式)

#### 方式 A - 浏览器 (简单)

```bash
# 终端 1: 启动后端
./build/autocaption -m models/ggml-base.bin

# 终端 2: 启动 Web 服务器
cd webui && python3 -m http.server 8080

# 浏览器打开 http://localhost:8080
```

#### 方式 B - Tauri 桌面应用 (推荐)

```bash
# 安装 Rust (如果尚未安装)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

# 启动后端
./build/autocaption -m models/ggml-base.bin

# 启动 Tauri (开发模式，有控制台输出)
. "$HOME/.cargo/env" && cd src-tauri && cargo run

# 或使用脚本
./scripts/start-tauri.sh

# 或运行已构建的应用
open src-tauri/target/debug/bundle/macos/Autocaption.app
```

## 使用方法

### 命令行选项

```bash
./build/autocaption -m models/ggml-base.bin [options]

Options:
  -m, --model <path>       Whisper 模型路径 (必需)
  -l, --language <lang>    语言代码 (默认: auto)
  -t, --threads <n>        线程数 (默认: 4)
  -p, --port <port>        WebSocket 端口 (默认: 8765)
  --stream-window <ms>     流式窗口 (默认: 3000ms)
  --stdout                 输出到控制台
  --file <path>            保存到文件
  --log <path>             日志文件路径
  --gpu                    启用 GPU 加速
  -h, --help               显示帮助
```

### 示例

```bash
# 基本用法
./build/autocaption -m models/ggml-base.bin

# 中文识别
./build/autocaption -m models/ggml-base.bin -l zh

# 输出到控制台
./build/autocaption -m models/ggml-base.bin --stdout

# 调整流式窗口（越小越实时）
./build/autocaption -m models/ggml-base.bin --stdout --stream-window 2000
```

## Tauri 桌面应用功能

| 功能 | 浏览器 | Tauri |
|------|--------|-------|
| 实时字幕 | ✅ | ✅ |
| 悬浮窗 (Always-on-top) | ❌ | ✅ |
| 穿透点击 | ❌ | ✅ |
| 系统托盘 | ❌ | ✅ |
| 全局快捷键 (F9/F10) | ❌ | ✅ |
| 独立应用 | ❌ | ✅ |

### Tauri 快捷键

| 快捷键 | 功能 |
|--------|------|
| `F9` | 切换悬浮窗模式 |
| `F10` | 显示/隐藏主窗口 |

### 系统托盘

点击托盘图标可快速显示/隐藏应用窗口。

## 平台特定说明

### Windows

使用 WASAPI Loopback 模式捕获系统音频。无需额外配置。

### macOS

macOS 没有原生的系统音频环回功能，需要设置虚拟音频设备：

1. 安装 [BlackHole](https://github.com/ExistentialAudio/BlackHole):
   ```bash
   brew install blackhole-2ch
   ```

2. 创建多输出设备:
   - 打开 "音频 MIDI 设置"
   - 点击左下角的 "+", 选择 "创建多输出设备"
   - 勾选 "BlackHole 2ch" 和您的扬声器

3. 设置默认设备:
   - **输出**: 选择多输出设备
   - **输入**: 选择 BlackHole 2ch

### Linux

使用 PulseAudio 的 monitor 源捕获系统音频：

```bash
# 查看可用的 monitor 源
pactl list | grep Name | grep monitor

# 设置默认源（如果需要）
pactl set-default-source <monitor_source_name>
```

## 架构

```
系统音频 (OS 输出)
        ↓
┌─────────────────────────────┐
│  平台特定音频捕获 (C++)      │
│  - WASAPI Loopback           │
│  - CoreAudio HAL             │
│  - PulseAudio Monitor        │
└─────────────────────────────┘
        ↓
┌─────────────────────────────┐
│  音频处理                     │
│  - 立体声 → 单声道            │
│  - 重采样至 16kHz            │
│  - Float32 PCM 格式          │
└─────────────────────────────┘
        ↓
┌─────────────────────────────┐
│  VAD 语音活动检测            │
└─────────────────────────────┘
        ↓
┌─────────────────────────────┐
│  Whisper ASR (滑动窗口)      │
│  - 最少 1.5s 音频            │
│  - 每 3s 输出一次            │
│  - 最大 10s 上下文           │
└─────────────────────────────┘
        ↓
┌─────────────────────────────┐
│  IPC 输出                    │
│  - WebSocket (端口 8765)     │
│  - Stdout JSON Lines         │
│  - 文件日志                  │
└─────────────────────────────┘
        ↓
┌─────────────────────────────┐
│  Web UI / Tauri 应用         │
│  - 实时字幕显示              │
│  - 悬浮窗模式                │
│  - 系统托盘                  │
└─────────────────────────────┘
```

## 开发

### 项目结构

```
autocaption/
├── CMakeLists.txt          # C++ 构建配置
├── include/                # C++ 头文件
├── src/                    # C++ 源代码
│   ├── audio/             # 音频捕获和处理
│   ├── asr/               # 语音识别 (Whisper)
│   └── ipc/               # 进程间通信
├── webui/                 # Web UI (HTML/CSS/JS)
├── src-tauri/             # Tauri 桌面应用
│   ├── src/main.rs        # Rust 主程序
│   ├── Cargo.toml         # Rust 依赖
│   └── icons/             # 应用图标
└── scripts/               # 辅助脚本
```

### 构建选项

```bash
# 调试构建
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 启用测试
cmake .. -DBUILD_TESTS=ON

# 启用 CUDA (需要 NVIDIA GPU)
cmake .. -DUSE_CUDA=ON
```

## 故障排查

### WebSocket 连接失败

1. 检查后端是否运行：`nc -z localhost 8765`
2. 检查防火墙设置
3. 查看浏览器/Tauri 控制台错误

### Tauri 无法启动

```bash
# 诊断问题
./scripts/diagnose-tauri.sh

# 确保 Rust 环境
source "$HOME/.cargo/env"

# 重新构建
cd src-tauri && cargo tauri build --debug
```

### macOS 音频问题

1. 确认 BlackHole 2ch 已安装
2. 确认 BlackHole 是默认输入设备
3. 确认多输出设备包含 BlackHole 和扬声器

## 许可

MIT License - 详见 LICENSE 文件

## 致谢

- [whisper.cpp](https://github.com/ggerganov/whisper.cpp) - 高效的语音识别引擎
- [Tauri](https://tauri.app) - 轻量级桌面应用框架
- [Kimi Code](https://www.kimi.com/code) - 作者(?)
- [抹茶紅豆](https://www.matchaazukii.com/) - 把作品上傳的人類
