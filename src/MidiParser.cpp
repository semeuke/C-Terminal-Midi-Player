#include"MidiParser.h"
#include<fstream>
#include<stdexcept>
#include<iostream>
#include<cstring>
#include<algorithm>

MidiParser::MidiParser():m_us_per_tick(0.0){}
uint16_t MidiParser::ReadBE16(const uint8_t* p) {
	return (static_cast<uint16_t>(p[0]) << 8) | p[1];
}
uint32_t MidiParser::ReadBE32(const uint8_t* p) {
	return (static_cast<uint32_t>(p[0]) << 24) |
		(static_cast<uint32_t>(p[1])<<16) |
		(static_cast<uint32_t>(p[2])<<8) |
		static_cast<uint32_t>(p[3]);
}
uint32_t MidiParser::ReadVLQ(const std::vector<uint8_t>& data, size_t& pos) {
	uint32_t value = 0;
	uint8_t byte = 0;
	do {
		if (pos >= data.size())break;
		byte = data[pos++];
		value = (value << 7) | (byte & 0x7F);
	} 
	while (byte & 0x80);
	return value;
}
std::vector<NoteEvent>MidiParser::Parse(const std::string& filepath) {
	std::vector<NoteEvent>events;
	std::ifstream file(filepath, std::ios::binary);
	if (!file) {
		throw std::runtime_error("无法打开文件:" + filepath);
	}
	m_data.assign(std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>());
	//读取文件完毕
	if(m_data.size()<14){
		throw std::runtime_error("文件太小，不是有效MIDI");
	}
	if (std::memcmp(m_data.data(), "MThd", 4) != 0) {
		throw std::runtime_error("不是MIDI文件(缺少MThd)");
	}
	uint16_t format = ReadBE16(&m_data[8]);
	uint16_t num_tracks = ReadBE16(&m_data[10]);
	uint16_t division = ReadBE16(&m_data[12]);
	std::cout << "格式:" << format
		<< ",轨道数:" << num_tracks
		<< ",division:" << division << "\n";
	//Midi由HeaderChunk(MThd)+TrackChunk(MTrk)构成,以上验证头块完成。
	double ticks_per_quarter = division;
	m_us_per_tick = 500000.0 / ticks_per_quarter;
	//以上规定每tick多少微秒
	size_t pos = 14;
	for (uint16_t t = 0; t < num_tracks && pos + 8 <= m_data.size(); ++t) {
		if (std::memcmp(&m_data[pos], "MTrk", 4) != 0) {
			break;
		}
		uint32_t track_len = ReadBE32(&m_data[pos + 4]);
		pos += 8;
		size_t track_end = std::min(pos + track_len, m_data.size());
		uint32_t abs_tick = 0;
		uint8_t last_status = 0;
		while (pos < track_end)
		{
			uint32_t delta = ReadVLQ(m_data, pos);
			abs_tick += delta;
			if (pos >= track_end)break;
			uint8_t status = m_data[pos];
			//以上读取增量时间
			if (status < 0x80) {
				status = last_status;
			}
			else
			{
				last_status = status;
				pos++;
			}
			uint8_t type = status & 0xF0;
			uint8_t channel = status & 0x0F;
			if (type == 0x80 || type == 0x90) {
				if (pos + 2 > track_end)break;
				uint8_t note = m_data[pos];
				uint8_t velocity = m_data[pos + 1];
				pos += 2;
				NoteEvent ev;
				ev.time_us = static_cast<uint64_t>(abs_tick * m_us_per_tick);
				ev.note = note;
				ev.velocity = velocity;
				ev.channel = channel;
				ev.is_on = (type == 0x90 && velocity > 0);
				events.push_back(ev);
			}
			else if (type == 0xA0 || type == 0xB0 || type == 0xE0) {
				if (pos + 2 > track_end)break;
				pos += 2;
			}
			else if (type == 0xc0 || type == 0xD0) {
				if (pos + 1 > track_end)break;
				pos += 1;
			}

			else if (status == 0xFF) {
				if (pos >= track_end)break;
				uint8_t meta_type = m_data[pos++];
				uint32_t meta_len = ReadVLQ(m_data, pos);
				if (meta_type == 0x51 && meta_len == 3 && pos + 3 <= track_end) {
					uint32_t tempo = (m_data[pos] << 16) |
						(m_data[pos + 1] << 8) |
						m_data[pos + 2];
					m_us_per_tick = static_cast<double>(tempo) / ticks_per_quarter;
				}
				pos += meta_len;
			}
			else if (status == 0xF0 || status == 0xF7) {
				uint32_t len = ReadVLQ(m_data, pos);
				pos += len;
			}
			else {
				break;
			}
		}
		pos = track_end;
	}
	//以上完成meta事件
	std::sort(events.begin(), events.end(), [](const NoteEvent& a, const NoteEvent& b) {
		return a.time_us < b.time_us;
		});
	return events;
}