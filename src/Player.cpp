#include "Player.h"
#include "PianoRollRenderer.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <cmath>
#include <conio.h>
#include <algorithm>

Player::Player() : m_midiOut(nullptr) {
    InitMidi();
}

Player::~Player() {
    CloseMidi();
}

void Player::InitMidi() {
    UINT devCount = midiOutGetNumDevs();
    std::cout << "系统检测到 " << devCount << " 个 MIDI 输出设备\n";
    if (devCount == 0) {
        std::cerr << "错误：系统没有可用的 MIDI 输出设备。\n";
        m_midiOut = nullptr;
        return;
    }

    std::cout << "可用的 MIDI 输出设备:\n";
    for (UINT i = 0; i < devCount; ++i) {
        MIDIOUTCAPS caps;
        if (midiOutGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            std::cout << "  [" << i << "] " << caps.szPname << "\n";
        }
    }

    UINT selectedId = 0;
    std::cout << "请选择要使用的 MIDI 输出设备编号: ";
    if (!(std::cin >> selectedId) || selectedId >= devCount) {
        std::cerr << "输入无效，将使用默认设备。\n";
        selectedId = 0;
        std::cin.clear();
        std::cin.ignore(10000, '\n');
    } else {
        std::cin.ignore();
    }

    MMRESULT result = midiOutOpen(&m_midiOut, selectedId, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "打开 MIDI 设备失败，错误码: " << result << "\n";
        m_midiOut = nullptr;
    } else {
        std::cout << "成功打开 MIDI 设备: " << selectedId << "\n";
    }
}

void Player::SetChannelVolume(int channel, int volume) {
    if (!m_midiOut) return;
    DWORD msg = 0xB0 | (channel & 0x0F) | (7 << 8) | ((volume & 0x7F) << 16);
    midiOutShortMsg(m_midiOut, msg);
}

void Player::CloseMidi() {
    if (m_midiOut) {
        AllNotesOff();
        midiOutClose(m_midiOut);
        m_midiOut = nullptr;
    }
}

void Player::NoteOn(int channel, int note, int velocity) {
    if (!m_midiOut) return;
    DWORD msg = 0x90 | (channel & 0x0F)
              | ((note & 0x7F) << 8)
              | ((velocity & 0x7F) << 16);
    midiOutShortMsg(m_midiOut, msg);
}

void Player::NoteOff(int channel, int note) {
    if (!m_midiOut) return;
    DWORD msg = 0x80 | (channel & 0x0F) | ((note & 0x7F) << 8);
    midiOutShortMsg(m_midiOut, msg);
}

void Player::AllNotesOff() {
    if (!m_midiOut) return;
    for (int ch = 0; ch < 16; ++ch) {
        DWORD msg = 0xB0 | (ch & 0x0F) | (123 << 8) | (0 << 16);
        midiOutShortMsg(m_midiOut, msg);
    }
}

void Player::Pause() { m_paused = true; AllNotesOff(); }
void Player::Resume() { m_paused = false; }
void Player::Stop() { m_stopped = true; AllNotesOff(); }

bool Player::Play(const std::vector<NoteEvent>& allEvents,
                  uint64_t start_us,
                  const std::string& filename) {
    if (!m_midiOut) {
        std::cerr << "MIDI设备未初始化，无法播放。\n";
        return false;
    }

    m_stopped = false;
    m_paused  = false;

    // ---- 控制台尺寸 ----
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    int consoleW = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    int consoleH = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    if (consoleW < 40) consoleW = 40;
    if (consoleH < 10) consoleH = 10;

    PianoRollRenderer renderer(consoleW, consoleH);
    renderer.SetPitchRange(48, 24);
    renderer.SetMicrosecondsPerColumn(50000);

    std::vector<ActiveNote> activeNotes;

    // ---- 预渲染：构建 start_us 之前已经触发的音符 ----
    size_t cursor = 0;
    while (cursor < allEvents.size() && allEvents[cursor].time_us <= start_us) {
        const auto& ev = allEvents[cursor];
        if (ev.is_on) {
            activeNotes.push_back({ ev.note, ev.velocity, ev.channel, ev.time_us, 0, false });
        } else {
            for (auto& an : activeNotes) {
                if (!an.is_finished && an.note == ev.note && an.channel == ev.channel) {
                    an.off_time_us = ev.time_us;
                    an.is_finished = true;
                    break;
                }
            }
        }
        cursor++;
    }

    // // 清理已经移出屏幕左侧的已结束音符
    // uint64_t window_us = renderer.GetWindowMicroseconds();
    // activeNotes.erase(std::remove_if(activeNotes.begin(), activeNotes.end(),
    //     [&](const ActiveNote& n) {
    //         return n.is_finished && (start_us - n.off_time_us) > window_us;
    //     }), activeNotes.end());
	uint64_t window_us = renderer.GetWindowMicroseconds();

    uint64_t total_us = allEvents.empty() ? 0 : allEvents.back().time_us;

    // ---- 清屏 + 预热渲染 ----
    system("cls");

    // 先渲染一帧历史画面
    renderer.Render(activeNotes, start_us, 0, total_us, filename, false,
                    " P pause  Q quit  +/- zoom  [ ] history  , . pitch");

    // 等待 300ms，让合成器/终端稳定
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 发送在 start_us 时仍活跃的音符的 Note On
    for (const auto& an : activeNotes) {
        if (!an.is_finished) {
            NoteOn(an.channel, an.note, an.velocity);
        }
    }

    uint64_t current_us = start_us;
    int64_t  view_offset_us = 0;   // 负数表示向历史回退

    auto lastTick  = std::chrono::steady_clock::now();
    auto lastFrame = std::chrono::steady_clock::now() - std::chrono::milliseconds(33);

    bool quitRequested = false;
    bool waitingForAnswer = false;

    while (!quitRequested) {
        auto now = std::chrono::steady_clock::now();
        auto delta_us = std::chrono::duration_cast<std::chrono::microseconds>(now - lastTick).count();
        lastTick = now;

        if (!m_paused && !waitingForAnswer) {
            current_us += delta_us;
        }

        // ---- 推进事件 ----
        while (cursor < allEvents.size() && allEvents[cursor].time_us <= current_us) {
            const auto& ev = allEvents[cursor];
            if (ev.is_on) {
                NoteOn(ev.channel, ev.note, ev.velocity);
                activeNotes.push_back({ ev.note, ev.velocity, ev.channel, ev.time_us, 0, false });
            } else {
                NoteOff(ev.channel, ev.note);
                for (auto& an : activeNotes) {
                    if (!an.is_finished && an.note == ev.note && an.channel == ev.channel) {
                        an.off_time_us = ev.time_us;
                        an.is_finished = true;
                        break;
                    }
                }
            }
            cursor++;
        }

        // // ---- 清理已移出屏幕的已结束音符 ----
        // if (!activeNotes.empty()) {
        //     uint64_t cur_render = current_us + view_offset_us;
        //     activeNotes.erase(std::remove_if(activeNotes.begin(), activeNotes.end(),
        //         [&](const ActiveNote& n) {
        //             return n.is_finished && (cur_render - n.off_time_us) > window_us;
        //         }), activeNotes.end());
        // }

        // ---- 30fps 渲染 ----
        if (now - lastFrame >= std::chrono::milliseconds(33)) {
            std::string footer;
            if (waitingForAnswer) {
                footer = " Finished. Continue to another file? (y/n)  [ ] history  , . pitch";
            } else {
                footer = " P pause  Q quit  +/- zoom  [ ] history  , . pitch";
            }
            renderer.Render(activeNotes, current_us, view_offset_us, total_us,
                            filename, !m_paused && !waitingForAnswer, footer);
            lastFrame = now;
        }

        // ---- 键盘 ----
        if (_kbhit()) {
            int ch = _getch();

            bool pausedOrFinished = m_paused || waitingForAnswer;

            if (waitingForAnswer) {
                // 结束状态
                if (ch == 'y' || ch == 'Y') {
                    system("cls");
                    while (_kbhit()) _getch();
                    return true;
                } else if (ch == 'n' || ch == 'N') {
                    system("cls");
                    while (_kbhit()) _getch();
                    return false;
                } else if (ch == '+' || ch == '=') {
                    renderer.AdjustMicrosecondsPerColumn(+1);
                } else if (ch == '-' || ch == '_') {
                    renderer.AdjustMicrosecondsPerColumn(-1);
                } else if (ch == ',') {
                    renderer.ShiftPitch(-12);
                } else if (ch == '.') {
                    renderer.ShiftPitch(+12);
                } else if (ch == '[') {
                    view_offset_us -= 200000;
                    int64_t min_off = -static_cast<int64_t>(current_us);
                    if (view_offset_us < min_off) view_offset_us = min_off;
                } else if (ch == ']') {
                    view_offset_us += 200000;
                    int64_t max_off = static_cast<int64_t>(total_us) - static_cast<int64_t>(current_us);
                    if (max_off < 0) max_off = 0;
                    if (view_offset_us > max_off) view_offset_us = max_off;
                }
            } else {
                // 播放中或暂停中
                // 1. 状态键
                if (ch == 'q' || ch == 'Q') {
                    quitRequested = true;
                    m_stopped = true;
                } else if (ch == 'p' || ch == 'P') {
                    if (!m_paused) {
                        // 暂停
                        m_paused = true;
                        AllNotesOff();
                    } else {
                        // 恢复前：如果浏览过历史/未来，先把播放位置移动到视图位置
                        if (view_offset_us != 0) {
                            int64_t new_us = static_cast<int64_t>(current_us) + view_offset_us;
                            if (new_us < 0) new_us = 0;
                            if (new_us > static_cast<int64_t>(total_us)) new_us = total_us;
                            current_us = static_cast<uint64_t>(new_us);
                            view_offset_us = 0;
                
                            // 重建事件游标与活跃音符列表
                            activeNotes.clear();
                            cursor = 0;
                            while (cursor < allEvents.size() && allEvents[cursor].time_us <= current_us) {
                                const auto& ev = allEvents[cursor];
                                if (ev.is_on) {
                                    activeNotes.push_back({ ev.note, ev.velocity, ev.channel,
                                                            ev.time_us, 0, false });
                                } else {
                                    for (auto& an : activeNotes) {
                                        if (!an.is_finished && an.note == ev.note && an.channel == ev.channel) {
                                            an.off_time_us = ev.time_us;
                                            an.is_finished = true;
                                            break;
                                        }
                                    }
                                }
                                cursor++;
                            }

                            // 重新触发当前仍在响的音符
                            for (const auto& an : activeNotes) {
                                if (!an.is_finished) {
                                    NoteOn(an.channel, an.note, an.velocity);
                                }
                            }
                        }
                        m_paused = false;
                    }
                }
                // 2. 渲染参数键：任何状态都可用
                else if (ch == '+' || ch == '=') {
                    renderer.AdjustMicrosecondsPerColumn(+1);
                } else if (ch == '-' || ch == '_') {
                    renderer.AdjustMicrosecondsPerColumn(-1);
                } else if (ch == ',') {
                    renderer.ShiftPitch(-12);
                } else if (ch == '.') {
                    renderer.ShiftPitch(+12);
                }
                // 3. 时间轴键：只在暂停中可用
                else if (pausedOrFinished) {
                    if (ch == '[') {
                        view_offset_us -= 200000;
                        int64_t min_off = -static_cast<int64_t>(current_us);
                        if (view_offset_us < min_off) view_offset_us = min_off;
                    } else if (ch == ']') {
                        view_offset_us += 200000;
                        int64_t max_off = static_cast<int64_t>(total_us) - static_cast<int64_t>(current_us);
                        if (max_off < 0) max_off = 0;
                        if (view_offset_us > max_off) view_offset_us = max_off;
                    }
                }
            }
        }

        // ---- 播放自然结束 ----
        if (!waitingForAnswer && !m_paused && current_us > total_us) {
            current_us = total_us;
            waitingForAnswer = true;
            m_paused = true;
            AllNotesOff();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    AllNotesOff();
    while (_kbhit()) _getch();
    return false;
}