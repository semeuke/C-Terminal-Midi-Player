#ifndef PIANO_ROLL_RENDERER_H
#define PIANO_ROLL_RENDERER_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#include <string>
#include <cstdint>

// 正在播放或已经结束的一个音符块
struct ActiveNote {
    uint8_t  note;
    uint8_t  velocity;
    uint8_t  channel;
    uint64_t on_time_us;
    uint64_t off_time_us;   // 0 = 还没收到 Note Off
    bool     is_finished;
};

// ========== 颜色配置（用户可自行修改） ==========
// 白键行背景色
#define COLOR_WHITE_KEY_BG      (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
// 黑键行背景色
#define COLOR_BLACK_KEY_BG      (BACKGROUND_INTENSITY)
// 音符块力度颜色
#define COLOR_VEL_LOW           (BACKGROUND_BLUE)
#define COLOR_VEL_MID           (BACKGROUND_GREEN | BACKGROUND_BLUE)
#define COLOR_VEL_HIGH          (BACKGROUND_GREEN)
// 播放线颜色
#define COLOR_PLAYHEAD          (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
// 文本/标签颜色
#define COLOR_TEXT              (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
// 顶部/底部栏背景色（可选，默认无背景）
#define COLOR_BAR_BG            0
// ================================================

class PianoRollRenderer {
public:
    PianoRollRenderer(int width, int height);
    ~PianoRollRenderer();

    void SetPitchRange(int lowest_note, int num_semitones);
    void SetMicrosecondsPerColumn(uint64_t us);
    void AdjustMicrosecondsPerColumn(int direction);
    void ShiftPitch(int semitones);

    // 获取当前可见时间窗口的微秒数
    uint64_t GetWindowMicroseconds() const;

    // 每帧调用
    // view_offset_us: 时间轴偏移，用于暂停时查看历史
    void Render(const std::vector<ActiveNote>& notes,
                uint64_t current_us,
                uint64_t view_offset_us,
                uint64_t total_us,
                const std::string& filename,
                bool playing,
                const std::string& footer_msg);

private:
    void ClearBuffer();
    void PutChar(int x, int y, char c, WORD attr);
    void PutString(int x, int y, const std::string& s, WORD attr);
    void Flush();

    int m_width;
    int m_height;
    int m_lowestNote;
    int m_numSemitones;
    uint64_t m_usPerColumn;
    float m_playColRatio;

    std::vector<CHAR_INFO> m_buffer;
    HANDLE m_hConsole;
    CONSOLE_CURSOR_INFO m_oldCursor;
};

#endif