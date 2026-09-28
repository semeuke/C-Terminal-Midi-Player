#ifndef PLAYER_H
#define PLAYER_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include "EventScheduler.h"
#include "MidiParser.h"
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

class Player {
public:
    Player();
    ~Player();

    // 返回 true 表示继续播放其他文件，false 表示退出程序
    bool Play(const std::vector<NoteEvent>& allEvents,
              uint64_t start_us,
              const std::string& filename);

    void Pause();
    void Resume();
    bool IsPaused() const { return m_paused; }
    void Stop();

private:
    void InitMidi();
    void CloseMidi();
    void SetChannelVolume(int channel, int volume);
    void NoteOn(int channel, int note, int velocity);
    void NoteOff(int channel, int note);
    void AllNotesOff();

    HMIDIOUT m_midiOut;
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_stopped{false};
};

#endif