/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#include "genmc/Verification/CATDecisionState.hpp"

#include "genmc/ADT/VectorClock.hpp"
#include "genmc/Execution/EventLabel.hpp"
#include "genmc/Execution/ExecutionGraph.hpp"
#include "genmc/Support/Cast.hpp"
#include "genmc/Support/Error.hpp"

namespace genmc::catcensus {

void DecisionState::record(Kind kind, Event subject, cat::StableEventKey alternative)
{
	if (const auto found = entries_.find(subject);
	    found != entries_.end() && found->second.kind == kind &&
		    found->second.alternative == alternative)
		return;
	entries_.insert_or_assign(subject,
				  Entry{kind, subject, std::move(alternative), nextOrdinal_++});
}

void DecisionState::recordRf(const ReadLabel &read)
{
	VERIFY(read.getRf(), "cannot record an unassigned RF decision");
	record(Kind::RF, read.getPos(), genmc::isa<InitLabel>(read.getRf())
					     ? cat::StableEventKey{read.getAddr()}
					     : cat::StableEventKey{read.getRf()->getPos()});
}

void DecisionState::recordCo(const WriteLabel &write)
{
	VERIFY(write.getParent(), "cannot record a graphless CO decision");
	VERIFY(write.isInCo(), "cannot record an unplaced CO decision");
	const auto *predecessor = write.getParent()->co_imm_pred(&write);
	record(Kind::CO, write.getPos(),
	       predecessor ? cat::StableEventKey{predecessor->getPos()}
			   : cat::StableEventKey{write.getAddr()});
}

void DecisionState::cut(const VectorClock &prefix)
{
	std::erase_if(entries_, [&](const auto &item) {
		const auto &entry = item.second;
		if (!prefix.contains(entry.subject))
			return true;
		const auto *event = std::get_if<Event>(&entry.alternative);
		return event && !prefix.contains(*event);
	});
}

auto DecisionState::find(Event subject) const -> const Entry *
{
	const auto found = entries_.find(subject);
	return found == entries_.end() ? nullptr : &found->second;
}

} /* namespace genmc::catcensus */
