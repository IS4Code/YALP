#include "timers.h"

#include <utility>
#include <chrono>
#include <list>
#include <unordered_set>

namespace timers
{
	unsigned int tick_count = 0;
	std::list<std::pair<unsigned int, std::function<void()>>> tick_handlers;
	std::list<std::pair<std::chrono::system_clock::time_point, std::function<void()>>> timer_handlers;

	template <class Ord, class Obj>
	typename std::list<std::pair<Ord, Obj>>::iterator insert_sorted(std::list<std::pair<Ord, Obj>> &list, const Ord &ord, Obj &&obj)
	{
		for(auto it = list.begin();; it++)
		{
			if(it == list.end() || it->first > ord)
			{
				return list.insert(it, std::make_pair(ord, std::forward<Obj>(obj)));
			}
		}
	}

	void register_tick(unsigned int ticks, std::function<void()> &&handler)
	{
		unsigned int time = tick_count + ticks;
		insert_sorted(tick_handlers, time, std::move(handler));
	}

	void register_timer(unsigned int interval, std::function<void()> &&handler)
	{
		auto time = std::chrono::system_clock::now() + std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::milliseconds(interval));
		insert_sorted(timer_handlers, time, std::move(handler));
	}

	void tick()
	{
		tick_count++;
		{
			auto it = tick_handlers.begin();
			while(it != tick_handlers.end())
			{
				auto &pair = *it;

				if(pair.first <= tick_count)
				{
					auto handler = std::move(pair.second);
					it = tick_handlers.erase(it);
					handler();
				}else{
					break;
				}
			}
		}
		if(tick_handlers.empty())
		{
			tick_count = 0;
		}

		auto now = std::chrono::system_clock::now();
		{
			auto it = timer_handlers.begin();
			while(it != timer_handlers.end())
			{
				auto &pair = *it;

				if(now >= pair.first)
				{
					auto handler = std::move(pair.second);
					it = timer_handlers.erase(it);
					handler();
				}else{
					it++;
				}
			}
		}
	}

	void clear()
	{
		tick_handlers.clear();
		timer_handlers.clear();
	}
}
