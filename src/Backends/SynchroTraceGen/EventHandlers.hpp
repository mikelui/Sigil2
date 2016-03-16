#ifndef STGEN_EVENTHANDLERS_H
#define STGEN_EVENTHANDLERS_H

#include <unistd.h>
#include <memory>
#include <fstream>
#include "spdlog.h"
#include "zfstream.h"
#include "ShadowMemory.hpp"
#include "STEvent.hpp"

namespace STGen
{

class EventHandlers
{
	/* Default address params */
	ShadowMemory shad_mem;

	/* Per-thread event count. Logged to SynchroTrace event trace.
	 * Each derived SynchroTrace event tracks the same event id.  */
	std::unordered_map<TId, EId> event_ids;
	TId curr_thread_id;
	EId curr_event_id;

	/* Vector of spawner, address of spawnee thread_t
	 * All addresses associated with the same spawner are in
	 * the order they were inserted */
	std::vector<std::pair<TId, Addr>> thread_spawns;

	/* Each spawned thread's ID, in the order it first seen ('created') */
	std::vector<TId> thread_creates;

	/* Vector of barrier_t addr, participating threads.
	 * Order matters for SynchroTraceSim */
	std::vector<std::pair<Addr, std::set<TId>>> barrier_participants;

	/* Output directly to a *.gz stream to save space */
	/* Keep these ostreams open until deconstruction */
	std::vector<std::shared_ptr<gzofstream>> gz_streams;
	std::map<std::string, std::shared_ptr<spdlog::logger>> loggers;
	std::shared_ptr<spdlog::logger> curr_logger;

	/* Compatibility with SynchroTraceSim parser */
	const char filebase[32] = "sigil.events.out-";
	std::string output_directory;

	std::string filename(TId tid)
	{
		return output_directory + "/" + filebase +
			std::to_string(tid) + ".gz";
	}

public:
	EventHandlers();
	EventHandlers(const EventHandlers&) = delete;
	EventHandlers& operator=(const EventHandlers&) = delete;
	~EventHandlers();

	/* interface to Sigil2 */
	void onSyncEv(SglSyncEv ev);
	void onCompEv(SglCompEv ev);
	void onMemEv(SglMemEv ev);
	void onCxtEv(SglCxtEv ev);
	void cleanup();
	void parseArgs(std::vector<std::string> args);

	/* SynchroTraceGen makes use of 3 SynchroTrace events,
	 * i.e. Computation, Communication, and Synchronization.
	 *
	 * One of each event is populated and flushed as Sigil
	 * primitives are processed. Because there might be trillions
	 * or more of SynchroTrace events, dynamic heap allocation of
	 * consecutive SynchroTrace events is avoided
	 *
	 * An additional pseudo-event, an instruction event, represents
	 * instruction addresses. Whenever any other event is flushed,
	 * this instruction event should also be flushed. */
	STInstrEvent st_cxt_ev;
	STCompEvent st_comp_ev;
	STCommEvent st_comm_ev;
	STSyncEvent st_sync_ev;
	
private:
	void onLoad(const SglMemEv& ev_data);
	void onStore(const SglMemEv& ev_data);
	void setThread(TId tid);
	void initThreadLog(TId tid);
	void switchThreadLog(TId tid);
};

}; //end namespace STGen

#endif