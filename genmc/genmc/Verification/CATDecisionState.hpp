/*
 * GenMC -- Generic Model Checking.
 *
 * This project is dual-licensed under the Apache License 2.0 and the MIT License.
 */

#ifndef GENMC_CAT_DECISION_STATE_HPP
#define GENMC_CAT_DECISION_STATE_HPP

#include "genmc/CAT/StableGraphAdapter.hpp"
#include "genmc/Execution/Event.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>

class ReadLabel;
class VectorClock;
class WriteLabel;

namespace genmc::catcensus {

/** Exact RF/CO choices active in one real ExecutionGraph prefix. */
class DecisionState {
public:
	enum class Kind : std::uint8_t { RF, CO };

	struct Entry {
		Kind kind{};
		Event subject;
		cat::StableEventKey alternative;
		std::uint64_t ordinal{};

		auto operator==(const Entry &) const -> bool = default;
	};

	/** Record one genuine multi-alternative decision, replacing the old subject choice. */
	void recordRf(const ReadLabel &read);
	void recordCo(const WriteLabel &write);
	/** Remove choices whose subject or real-event alternative is outside @p prefix. */
	void cut(const VectorClock &prefix);

	[[nodiscard]] auto find(Event subject) const -> const Entry *;
	[[nodiscard]] auto size() const -> std::size_t { return entries_.size(); }
	[[nodiscard]] auto nextOrdinal() const -> std::uint64_t { return nextOrdinal_; }

private:
	void record(Kind kind, Event subject, cat::StableEventKey alternative);

	std::map<Event, Entry> entries_;
	std::uint64_t nextOrdinal_{1};
};

} /* namespace genmc::catcensus */

#endif /* GENMC_CAT_DECISION_STATE_HPP */
