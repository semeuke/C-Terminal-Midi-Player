#include"EventScheduler.h"
EventScheduler::EventScheduler(){}
void EventScheduler::LoadEvents(const std::vector<NoteEvent>& events) {
	for (const auto& ev : events) {
		m_queue.push(ev);
	}
}
bool EventScheduler::HasNext()const {
	return !m_queue.empty();
}
NoteEvent EventScheduler::Next() {
	NoteEvent ev = m_queue.top();
	m_queue.pop();
	return ev;
}