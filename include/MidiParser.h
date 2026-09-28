#ifndef MIDI_PARSER_H
#define MIDI_PARSER_H

#include <vector>
#include<cstdint>
#include<string>

struct NoteEvent
{
	uint64_t time_us;
	uint8_t note;
	uint8_t velocity;
	bool is_on;
	uint8_t channel;
	//time_us微秒精度绝对时间，note是音高，velocity是力度
};

class MidiParser {
public:
	MidiParser();
	std::vector<NoteEvent> Parse(const std::string& filepath);
private:
	uint16_t ReadBE16(const uint8_t* p);
	uint32_t ReadBE32(const uint8_t* p);
	uint32_t ReadVLQ(const std::vector<uint8_t>& data, size_t& pos);
	std::vector<uint8_t> m_data;
	double m_us_per_tick;
};
#endif