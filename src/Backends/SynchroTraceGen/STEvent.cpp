#include "STEvent.hpp"
#include "Sigil2/SigiLog.hpp"

#include <cassert>
#include <cstring>

/* XXX overflow builtin not in GCC <5.
 * This overflow check should only be used for
 * variables which increment by 1 each time. */
#if __GNUC__ >= 5
#define INCR_EID_OVERFLOW(var) __builtin_add_overflow(var, 1, &var)
#else
#define INCR_EID_OVERFLOW(var) (var == UINT_MAX ? true : (var += 1) && false)
#endif

namespace STGen
{

////////////////////////////////////////////////////////////
// SynchroTrace - Compute Event
////////////////////////////////////////////////////////////
STCompEvent::STCompEvent(TID &tid, EID &eid, const std::shared_ptr<spdlog::logger> &logger)
    : thread_id(tid)
    , event_id(eid)
    , logger(logger)
{
    reset();
}


void STCompEvent::flush()
{
    if (is_empty == false)
    {
        logmsg += std::to_string(event_id).append(",");
        logmsg += std::to_string(thread_id).append(",");
        logmsg += std::to_string(iop_cnt).append(",");
        logmsg += std::to_string(flop_cnt).append(",");
        logmsg += std::to_string(thread_local_load_cnt).append(",");
        logmsg += std::to_string(thread_local_store_cnt);

        char hexStr[sizeof(Addr)*2+3];

        /* log write addresses */
        for (auto &addr_pair : stores_unique.get())
        {
            assert(addr_pair.first <= addr_pair.second);
            logmsg += " $ ";
            logmsg += n2hexstr(hexStr, addr_pair.first);
            logmsg += " ";
            logmsg += n2hexstr(hexStr, addr_pair.second);
        }

        /* log read addresses */
        for (auto &addr_pair : loads_unique.get())
        {
            assert(addr_pair.first <= addr_pair.second);
            logmsg += " * ";
            logmsg += n2hexstr(hexStr, addr_pair.first);
            logmsg += " ";
            logmsg += n2hexstr(hexStr, addr_pair.second);
        }

        logger->info(logmsg);

        if(INCR_EID_OVERFLOW(event_id))
        {
            SigiLog::fatal("SynchroTraceGen Event ID overflow");
        }

        reset();
    }
}


void STCompEvent::updateWrites(const Addr begin, const Addr size)
{
    stores_unique.insert(std::make_pair(begin, begin + size - 1));
}


void STCompEvent::updateWrites(const SglMemEv &ev)
{
    stores_unique.insert(std::make_pair(ev.begin_addr, ev.begin_addr + ev.size - 1));
}


void STCompEvent::updateReads(const Addr begin, const Addr size)
{
    loads_unique.insert(std::make_pair(begin, begin + size - 1));
}


void STCompEvent::updateReads(const SglMemEv &ev)
{
    loads_unique.insert(std::make_pair(ev.begin_addr, ev.begin_addr + ev.size - 1));
}


void STCompEvent::incWrites()
{
    is_empty = false;
    ++thread_local_store_cnt;
    ++total_events;
}


void STCompEvent::incReads()
{
    is_empty = false;
    ++thread_local_load_cnt;
    ++total_events;
}


void STCompEvent::incIOP()
{
    is_empty = false;
    ++iop_cnt;
    ++total_events;
}


void STCompEvent::incFLOP()
{
    is_empty = false;
    ++flop_cnt;
    ++total_events;
}


inline void STCompEvent::reset()
{
    iop_cnt = 0;
    flop_cnt = 0;
    thread_local_store_cnt = 0;
    thread_local_load_cnt = 0;
    total_events = 0;
    stores_unique.clear();
    loads_unique.clear();

    logmsg.clear();
    is_empty = true;
}


////////////////////////////////////////////////////////////
// SynchroTrace - Communication Event
////////////////////////////////////////////////////////////
STCommEvent::STCommEvent(TID &tid, EID &eid, const std::shared_ptr<spdlog::logger> &logger)
    : thread_id(tid)
    , event_id(eid)
    , logger(logger)
{
    reset();
}


void STCommEvent::flush()
{
    if (is_empty == false)
    {
        logmsg += std::to_string(event_id).append(",");
        logmsg += std::to_string(thread_id);

        assert(comms.empty() == false);

        /* log comm edges between current and other threads */
        char hexStr[sizeof(Addr)*2+3];
        for (auto &edge : comms)
        {
            for (auto &addr_pair : std::get<2>(edge).get())
            {
                assert(addr_pair.first <= addr_pair.second);
                logmsg += " # ";
                logmsg += std::to_string(std::get<0>(edge)).append(" ");
                logmsg += std::to_string(std::get<1>(edge)).append(" ");
                logmsg += n2hexstr(hexStr, addr_pair.first);
                logmsg += " ";
                logmsg += n2hexstr(hexStr, addr_pair.second);
            }
        }

        logger->info(logmsg);

        if(INCR_EID_OVERFLOW(event_id))
            SigiLog::fatal("SynchroTraceGen Event ID overflow");

        reset();
    }
}


void STCommEvent::addEdge(const TID writer, const EID writer_event, const Addr addr)
{
    is_empty = false;

    if (comms.empty())
    {
        comms.push_back(std::make_tuple(writer, writer_event, AddrSet(std::make_pair(addr, addr))));
    }
    else
    {
        for (auto &edge : comms)
        {
            if (std::get<0>(edge) == writer && std::get<1>(edge) == writer_event)
            {
                std::get<2>(edge).insert(std::make_pair(addr, addr));
                return;
            }
        }

        comms.push_back(std::make_tuple(writer, writer_event, AddrSet(std::make_pair(addr, addr))));
    }
}


inline void STCommEvent::reset()
{
    comms.clear();
    logmsg.clear();
    is_empty = true;
}


////////////////////////////////////////////////////////////
// SynchroTrace - Context Event (Instruction)
////////////////////////////////////////////////////////////
STInstrEvent::STInstrEvent(const std::shared_ptr<spdlog::logger> &logger)
    : logger(logger)
{
    reset();
}


void STInstrEvent::append_instr(Addr addr)
{
    char hexStr[sizeof(Addr)*2+3];
    instrs += "! ";
    instrs += n2hexstr(hexStr, addr);
    instrs += " ";

    is_empty = false;
}


inline void STInstrEvent::flush()
{
    if (is_empty == false)
    {
        logger->info(instrs);
        reset();
    }
}


inline void STInstrEvent::reset()
{
    instrs.clear();
    is_empty = true;
}


////////////////////////////////////////////////////////////
// SynchroTrace - Synchronization Event
////////////////////////////////////////////////////////////
STSyncEvent::STSyncEvent(TID &tid, EID &eid, const std::shared_ptr<spdlog::logger> &logger)
    : thread_id(tid)
    , event_id(eid)
    , logger(logger) { }


void STSyncEvent::flush(const STSyncType type, const Addr sync_addr)
{
    char hexStr[sizeof(Addr)*2+3];
    logmsg += std::to_string(event_id).append(",");
    logmsg += std::to_string(thread_id).append(",pth_ty:");
    logmsg += std::to_string(type).append("^");
    logmsg += n2hexstr(hexStr, sync_addr);
    logger->info(logmsg);
    logmsg.clear();

    if(INCR_EID_OVERFLOW(event_id))
        SigiLog::fatal("SynchroTraceGen Event ID overflow");
}


////////////////////////////////////////////////////////////
// Unique Address Set
////////////////////////////////////////////////////////////
void AddrSet::insert(const AddrRange &range)
{
    /* TODO Someone please clean up flow control */

    assert(range.first <= range.second);

    /* insert if this is the first addr */
    if (ms.empty() == true)
    {
        ms.insert(range);
        return;
    }

    /* get the first addr pair that is not less than range */
    /* see http://en.cppreference.com/w/cpp/utility/pair/operator_cmp */
    auto it = ms.lower_bound(range);

    if (it != ms.cbegin())
    {
        if (it == ms.cend())
            /* if no address range starts at a higher address,
             * check the last element */
        {
            it = --ms.cend();
        }
        else
            /* check if the previous addr pair overlaps with range */
        {
            --it;

            if (range.first > it->second + 1)
            {
                ++it;
            }
        }
    }

    if (range.first == it->second + 1)
    {
        /* extend 'it' by 'range'; recheck, may overrun other addresses */
        auto tmp = std::make_pair(it->first, range.second);
        ms.erase(it);
        insert(tmp);
    }
    else if (range.second + 1 == it->first)
    {
        /* extend 'it' by 'range'; recheck, may overrun other addresses */
        auto tmp = std::make_pair(range.first, it->second);
        ms.erase(it);
        insert(tmp);
    }
    else if (range.first > it->second)
    {
        /* can't merge, just insert (at end) */
        ms.insert(range);
    }
    else if (range.first >= it->first)
    {
        if (range.second > it->second)
            /* extending 'it' to the end of 'range' */
        {
            /* merge, delete, and recheck, may overrun other addresses */
            auto tmp = std::make_pair(it->first, range.second);
            ms.erase(it);
            insert(tmp);
        }

        /* else do not insert; 'it' encompasses 'range' */
    }
    else /* if (range.first < it->first) */
    {
        if (range.second < it->first)
            /* no overlap */
        {
            /* nothing to merge */
            ms.insert(range);
        }
        else if (range.second <= it->second)
            /* begin address is extended */
        {
            /* merge, delete, and insert; no need to recheck */
            Addr second = it->second;
            ms.erase(it);
            ms.emplace(range.first, second);
        }
        else /* if(range.second > it->second) */
            /* 'range' encompasses 'it' */
        {
            /* delete old range and insert bigger range; recheck */
            ms.erase(it);
            insert(range);
        }
    }
}


void AddrSet::clear()
{
    ms.clear();
}

}; //end namespace STGen
