# Tauri 桌面应用使用指南

## 快速开始

### 1. 确保后端运行
```bash
# 终端 1：启动 C++ 后端（带滑动窗口 3 秒）
./build/autocaption -m models/ggml-base.bin
```

### 2. 启动 Tauri（开发模式）
```bash
# 终端 2：启动 Tauri（带控制台输出，便于调试）
. "$HOME/.cargo/env" && cd src-tauri && cargo run
```

或者使用脚本：
```bash
./scripts/run-tauri-debug.sh
```

### 3. 使用已构建的应用
```bash
open src-tauri/target/debug/bundle/macos/Autocaption.app
```

---

## 故障排查

### 问题 1: "command not found: cargo"
**解决**: 加载 Rust 环境
```bash
source "$HOME/.cargo/env"
```

### 问题 2: Tauri 窗口白屏/无法连接 WebSocket
**解决**: 确保后端已启动并检查 CSP 配置
```bash
# 检查后端是否运行
curl http://localhost:8765  # 应该返回错误或连接成功

# 检查 WebSocket 连接
echo "测试 WebSocket..."
nc -z localhost 8765 && echo "✅ 端口开放" || echo "❌ 端口未开放"
```

### 问题 3: "input too short" 错误
**已修复**: 滑动窗口现在确保至少 1.5 秒音频才处理
```
# 之前的错误
Processing speech segment: 8000 samples (0.500000s)  ← 太短
input is too short - 490 ms < 1000 ms

# 现在
Processing speech segment: 48000 samples (3.000s)  ← 正常
```

---

## 功能说明

| 快捷键 | 功能 |
|--------|------|
| `F9` | 切换悬浮窗模式 |
| `F10` | 显示/隐藏主窗口 |

| 特性 | 状态 |
|------|------|
| 系统托盘 | ✅ 点击图标显示/隐藏 |
| 悬浮窗 | ✅ 无边框、始终置顶 |
| 穿透点击 | ✅ 鼠标可穿过字幕区域 |
| WebSocket | ✅ 自动连接 localhost:8765 |

---

## 更新后的滑动窗口参数

```cpp
MIN_PROCESS_MS = 1500    // 最少处理 1.5 秒音频
STEP_MS = 3000           // 每 3 秒输出一次
MAX_BUFFER_MS = 10000    // 最大保留 10 秒上下文
```

这确保了：
1. Whisper 不会收到太短的数据 (< 1s)
2. 每 3 秒有新的字幕输出
3. 保留 10 秒上下文提高准确度
