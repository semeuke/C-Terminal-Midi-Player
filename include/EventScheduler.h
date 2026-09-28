#ifndef EVENT_SCHEDULER_H
#define EVENT_SCHEDULER_H
#include "MidiParser.h"
#include <queue>
#include <vector>

class EventScheduler {
public:
	EventScheduler();
	void LoadEvents(const std::vector<NoteEvent>& events);
	bool HasNext() const;
	NoteEvent Next();

private:
	struct CompareEvent {
		bool operator()(const NoteEvent& a, const NoteEvent& b) {
			return a.time_us > b.time_us;
		}
	};
	std::priority_queue<NoteEvent, std::vector<NoteEvent>, CompareEvent> m_queue;
};
#endif