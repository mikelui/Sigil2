#include "EventManager.hpp"

namespace sgl
{

void EventManager::flushEvents()
{
	for (UInt i=0; i < used; ++i)
	{
		ev_buf[i].notify(ev_buf[i]);
	}
	used = 0;
}

void EventManager::finish()
{
	flushEvents();
	for( auto& cleanup : cleanup_observers )
	{
		cleanup();
	}
}

namespace
{
void notifyMemObservers(const EventManager::BufferedEvent& ev)
{
	for( auto& notify : *reinterpret_cast<Observers<SglMemEv>*>(ev.observers) )
	{
		notify(ev.mem_ev);
	}
}

void notifyCompObservers(const EventManager::BufferedEvent& ev)
{
	for( auto& notify : *reinterpret_cast<Observers<SglCompEv>*>(ev.observers) )
	{
		notify(ev.comp_ev);
	}
}

void notifySyncObservers(const EventManager::BufferedEvent& ev)
{
	for( auto& notify : *reinterpret_cast<Observers<SglSyncEv>*>(ev.observers) )
	{
		notify(ev.sync_ev);
	}
}

void notifyCxtObservers(const EventManager::BufferedEvent& ev)
{
	for( auto& notify : *reinterpret_cast<Observers<SglCxtEv>*>(ev.observers) )
	{
		notify(ev.cxt_ev);
	}
}
}; //end namespace

void EventManager::bufferEvent(SglMemEv ev)
{
	ev_buf[used].notify = notifyMemObservers;
	ev_buf[used].observers = reinterpret_cast<void*>(&mem_observers);
	ev_buf[used].mem_ev = ev;
	used++;
}

void EventManager::bufferEvent(SglCompEv ev) 
{
	ev_buf[used].notify = notifyCompObservers;
	ev_buf[used].observers = reinterpret_cast<void*>(&comp_observers);
	ev_buf[used].comp_ev = ev;
	used++;
}

void EventManager::bufferEvent(SglSyncEv ev)
{
	ev_buf[used].notify = notifySyncObservers;
	ev_buf[used].observers = reinterpret_cast<void*>(&sync_observers);
	ev_buf[used].sync_ev = ev;
	used++;
}

void EventManager::bufferEvent(SglCxtEv ev)
{
	ev_buf[used].notify = notifyCxtObservers;
	ev_buf[used].observers = reinterpret_cast<void*>(&cxt_observers);
	ev_buf[used].cxt_ev = ev;
	used++;
}

void EventManager::addObserver(std::function<void(SglMemEv)> obs)
{
	mem_observers.push_back(obs);
}

void EventManager::addObserver(std::function<void(SglCompEv)> obs)
{
	comp_observers.push_back(obs);
}

void EventManager::addObserver(std::function<void(SglSyncEv)> obs)
{
	sync_observers.push_back(obs);
}

void EventManager::addObserver(std::function<void(SglCxtEv)> obs)
{
	cxt_observers.push_back(obs);
}

void EventManager::addCleanup(std::function<void()> obs)
{
	cleanup_observers.push_back(obs);
}

}; //end namespace sgl
