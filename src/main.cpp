#include "MidiParser.h"
#include "Player.h"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <windows.h>
#include <conio.h>

namespace fs = std::filesystem;

static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT ||
        ctrlType == CTRL_BREAK_EVENT ||
        ctrlType == CTRL_CLOSE_EVENT) {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

        // 恢复光标显示
        CONSOLE_CURSOR_INFO cci;
        cci.dwSize = 25;
        cci.bVisible = TRUE;
        SetConsoleCursorInfo(h, &cci);

        // 清屏
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(h, &csbi)) {
            DWORD size = csbi.dwSize.X * csbi.dwSize.Y;
            DWORD written;
            COORD home = { 0, 0 };
            FillConsoleOutputCharacterA(h, ' ', size, home, &written);
            FillConsoleOutputAttribute(h, csbi.wAttributes, size, home, &written);
            SetConsoleCursorPosition(h, home);
        }
    }
    return FALSE;  // 返回 FALSE，让系统继续默认处理（终止进程）
}//针对ctrl+c中断后未清屏bug

int main() {
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    SetConsoleOutputCP(65001);

    std::string midi_dir = "MIDIS";
    if (!fs::exists(midi_dir) || !fs::is_directory(midi_dir)) {
        std::cerr << "找不到目录: " << midi_dir << "\n";
        return 1;
    }

    std::vector<std::string> midi_files;
    for (const auto& entry : fs::directory_iterator(midi_dir)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".mid" || ext == ".midi") {
            midi_files.push_back(entry.path().string());
        }
    }
    if (midi_files.empty()) {
        std::cerr << "MIDIS 目录下未找到 .mid 或 .midi 文件\n";
        return 1;
    }

    bool keepRunning = true;
    do {
        std::cout << "\n可用的MIDI文件:\n";
        for (size_t i = 0; i < midi_files.size(); ++i) {
            std::cout << "  [" << i << "] " << midi_files[i] << "\n";
        }

        std::cout << "请输入要播放的文件序号: ";
        size_t choice = 0;
        if (!(std::cin >> choice) || choice >= midi_files.size()) {
            std::cerr << "输入无效\n";
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            continue;
        }
        std::cin.ignore();

        std::string selected = midi_files[choice];
        std::cout << "你选择了: " << selected << "\n\n";

        MidiParser parser;
        std::vector<NoteEvent> events;
        try {
            events = parser.Parse(selected);
        } catch (const std::exception& e) {
            std::cerr << "解析错误: " << e.what() << "\n";
            continue;
        }

        std::cout << "解析到 " << events.size() << " 个音符事件\n";
        if (!events.empty()) {
            uint64_t total_us = events.back().time_us;
            std::cout << "总时长约 " << total_us / 1000000.0 << " 秒\n";
        }

        std::cout << "请输入起始时间（秒，0表示从头开始）: ";
        double start_sec = 0.0;
        std::cin >> start_sec;
        std::cin.ignore();
        uint64_t start_us = static_cast<uint64_t>(start_sec * 1000000.0);

        std::cout << "\n操作说明：\n";
        std::cout << "  P 暂停/继续   Q 退出\n";
        std::cout << "  +/- 时间缩放   [ ] 时间轴滚动   , . 音高移调\n";
        std::cout << "开始播放...\n\n";

        Player player;
        bool continuePlaying = player.Play(events, start_us, selected);
        if (!continuePlaying) {
            keepRunning = false;
        }

    } while (keepRunning);

    std::cout << "程序退出。\n";
    return 0;
}