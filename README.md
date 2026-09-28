# MIDI Player with Terminal Piano Roll

C++ 实现的 MIDI 文件解析与播放器，支持终端钢琴卷帘可视化。

## 功能

- MIDI 二进制解析（VLQ、running status、Tempo 元事件）
- 事件调度（`std::priority_queue` 按时间戳排序）
- Windows MIDI API 播放（Note On/Off、CC#123、CC#7）
- 终端钢琴卷帘可视化（30fps 双缓冲渲染、力度颜色映射）

## 操作

| 按键 | 功能 |
| :--- | :--- |
| `P` | 暂停/继续 |
| `Q` | 退出 |
| `+` / `-` | 时间轴缩放 |
| `[` / `]` | 时间轴滚动（暂停/结束后可用） |
| `,` / `.` | 音高移调 |

## 构建

```bash
mkdir build
cd build
cmake ..
cmake --build .

请将MIDIS文件夹放在build内
.\midi_player.exe  运行