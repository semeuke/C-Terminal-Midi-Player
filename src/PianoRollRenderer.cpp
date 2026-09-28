#include "PianoRollRenderer.h"
#include <cstdio>
#include <algorithm>
#include <cstring>

static const char* NOTE_NAMES[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

static WORD VelocityToBg(int vel) {
    if (vel < 48)  return COLOR_VEL_LOW;
    if (vel < 96)  return COLOR_VEL_MID;
    return COLOR_VEL_HIGH;
}

PianoRollRenderer::PianoRollRenderer(int width, int height)
    : m_width(width), m_height(height),
      m_lowestNote(48), m_numSemitones(24),
      m_usPerColumn(50000), m_playColRatio(0.8f) {
    m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    m_buffer.resize(static_cast<size_t>(width) * height);

    GetConsoleCursorInfo(m_hConsole, &m_oldCursor);
    CONSOLE_CURSOR_INFO cci = m_oldCursor;
    cci.bVisible = FALSE;
    SetConsoleCursorInfo(m_hConsole, &cci);
}

PianoRollRenderer::~PianoRollRenderer() {
    SetConsoleCursorInfo(m_hConsole, &m_oldCursor);
}

void PianoRollRenderer::SetPitchRange(int lowest_note, int num_semitones) {
    m_lowestNote = lowest_note;
    m_numSemitones = num_semitones;
}

void PianoRollRenderer::SetMicrosecondsPerColumn(uint64_t us) {
    m_usPerColumn = us;
}

void PianoRollRenderer::AdjustMicrosecondsPerColumn(int direction) {
    static const uint64_t options[] = { 25000, 50000, 100000, 200000, 400000 };
    const int n = sizeof(options) / sizeof(options[0]);
    int idx = 1;
    for (int i = 0; i < n; ++i) {
        if (options[i] == m_usPerColumn) { idx = i; break; }
    }
    idx = std::max(0, std::min(n - 1, idx + direction));
    m_usPerColumn = options[idx];
}

void PianoRollRenderer::ShiftPitch(int semitones) {
    int v = m_lowestNote + semitones;
    v = std::max(0, std::min(127 - m_numSemitones, v));
    m_lowestNote = v;
}

uint64_t PianoRollRenderer::GetWindowMicroseconds() const {
    int rollWidth = m_width - 5;  // 左侧标签宽 5
    if (rollWidth < 1) rollWidth = 1;
    return static_cast<uint64_t>(rollWidth) * m_usPerColumn;
}

void PianoRollRenderer::ClearBuffer() {
    CHAR_INFO ci;
    ci.Char.AsciiChar = ' ';
    ci.Attributes = COLOR_WHITE_KEY_BG | COLOR_BAR_BG;
    std::fill(m_buffer.begin(), m_buffer.end(), ci);
}

void PianoRollRenderer::PutChar(int x, int y, char c, WORD attr) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;
    int idx = y * m_width + x;
    m_buffer[idx].Char.AsciiChar = c;
    m_buffer[idx].Attributes = attr;
}

void PianoRollRenderer::PutString(int x, int y, const std::string& s, WORD attr) {
    for (size_t i = 0; i < s.size(); ++i) {
        PutChar(x + static_cast<int>(i), y, s[i], attr);
    }
}

void PianoRollRenderer::Flush() {
    COORD bufferSize = { static_cast<SHORT>(m_width), static_cast<SHORT>(m_height) };
    COORD bufferCoord = { 0, 0 };
    SMALL_RECT region = { 0, 0,
                          static_cast<SHORT>(m_width - 1),
                          static_cast<SHORT>(m_height - 1) };
    WriteConsoleOutput(m_hConsole, m_buffer.data(), bufferSize, bufferCoord, &region);
}

void PianoRollRenderer::Render(const std::vector<ActiveNote>& notes,
                               uint64_t current_us,
                               uint64_t view_offset_us,
                               uint64_t total_us,
                               const std::string& filename,
                               bool playing,
                               const std::string& footer_msg) {
    ClearBuffer();

    const int labelWidth = 5;
    const int topBar = 1;
    const int bottomBar = 1;
    const int rollHeight = m_height - topBar - bottomBar;
    const int rollWidth = m_width - labelWidth;
    if (rollHeight <= 0 || rollWidth <= 0) { Flush(); return; }

    // 实际用于渲染的时间点
    uint64_t render_us = current_us + view_offset_us;
    if (render_us > total_us) render_us = total_us;
    if (render_us < 0) render_us = 0;

    // 播放线位置
    const int playCol = labelWidth + static_cast<int>(rollWidth * m_playColRatio);

    // ---- 顶部栏 ----
    char topBuf[256];
    double cur_s = render_us / 1e6;
    double tot_s = total_us / 1e6;
    std::snprintf(topBuf, sizeof(topBuf),
                  " %s  |  %7.3fs / %7.3fs  |  %s",
                  filename.c_str(), cur_s, tot_s,
                  playing ? "PLAYING" : "PAUSED");
    PutString(0, 0, topBuf, COLOR_TEXT | COLOR_BAR_BG);

    // ---- 音高行 ----
    for (int row = 0; row < m_numSemitones && row < rollHeight; ++row) {
        int note = m_lowestNote + (m_numSemitones - 1 - row);
        int y = topBar + row;

        int ni = note % 12;
        int octave = note / 12 - 1;
        char label[8];
        std::snprintf(label, sizeof(label), "%-3s%d", NOTE_NAMES[ni], octave);

        bool isBlack = (ni == 1 || ni == 3 || ni == 6 || ni == 8 || ni == 10);
        WORD labelAttr = isBlack ? (COLOR_TEXT | COLOR_BLACK_KEY_BG)
                                 : (COLOR_TEXT | COLOR_WHITE_KEY_BG);
        PutString(0, y, label, labelAttr);

        // 分隔线
        PutChar(labelWidth - 1, y, '|', COLOR_TEXT);

        // 黑键行背景
        if (isBlack) {
            for (int x = labelWidth; x < m_width; ++x) {
                int idx = y * m_width + x;
                m_buffer[idx].Char.AsciiChar = ' ';
                m_buffer[idx].Attributes = COLOR_BLACK_KEY_BG;
            }
        }
    }

    // ---- 音符块 ----
    for (const auto& n : notes) {
        int right_col, left_col;
        if (!n.is_finished) {
            // 视图时间还没到音符开始 → 不渲染
            if (render_us < n.on_time_us) continue;
            right_col = playCol;
            double dx = (render_us - n.on_time_us) / static_cast<double>(m_usPerColumn);
            left_col = playCol - static_cast<int>(dx);
        } else {
            // 已结束音符：整体向左移动
            double dx_r = (render_us - n.off_time_us) / static_cast<double>(m_usPerColumn);
            double dx_l = (render_us - n.on_time_us)  / static_cast<double>(m_usPerColumn);
            right_col = playCol - static_cast<int>(dx_r);
            left_col  = playCol - static_cast<int>(dx_l);
        }
        if (right_col <= labelWidth) continue;
        if (left_col >= m_width) continue;

        left_col  = std::max(left_col, labelWidth);
        right_col = std::min(right_col, m_width);

        int row = (m_numSemitones - 1) - (n.note - m_lowestNote);
        if (row < 0 || row >= rollHeight) continue;
        int y = topBar + row;

        WORD bg = VelocityToBg(n.velocity);

        for (int x = left_col; x < right_col; ++x) {
            int idx = y * m_width + x;
            m_buffer[idx].Char.AsciiChar = ' ';
            m_buffer[idx].Attributes = bg;
        }

        // 块开头显示音名
        int width = right_col - left_col;
        if (width >= 3 && left_col + 3 <= m_width) {
            char name[8];
            std::snprintf(name, sizeof(name), "%s%d",
                          NOTE_NAMES[n.note % 12], n.note / 12 - 1);
            WORD fg = COLOR_TEXT;
            for (int i = 0; name[i] && left_col + i < right_col; ++i) {
                int idx = y * m_width + (left_col + i);
                m_buffer[idx].Char.AsciiChar = name[i];
                m_buffer[idx].Attributes = fg | bg;
            }
        }
    }

    // ---- 播放线 ----
    for (int row = 0; row < rollHeight; ++row) {
        int y = topBar + row;
        int idx = y * m_width + playCol;
        m_buffer[idx].Char.AsciiChar = '|';
        m_buffer[idx].Attributes = COLOR_PLAYHEAD;
    }

    // ---- 底部栏 ----
    PutString(0, m_height - 1, footer_msg, COLOR_TEXT | COLOR_BAR_BG);

    Flush();
}