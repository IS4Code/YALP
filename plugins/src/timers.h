#ifndef TIMERS_H_INCLUDED
#define TIMERS_H_INCLUDED

#include <functional>

namespace timers
{
	void register_tick(unsigned int ticks, std::function<void()> &&handler);
	void register_timer(unsigned int interval, std::function<void()> &&handler);
	void clear();

	void tick();
}

#endif
