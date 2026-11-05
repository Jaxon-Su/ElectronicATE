#pragma once

namespace TransientDetail {
class Execution;
class Event;

enum class ReferenceResults { None, AutoOnly, WorstSteady };

// Throws on failure; the entrypoint always runs Execution::finish afterwards.
void runSearch(Execution &run, Event &event, ReferenceResults references);
} // namespace TransientDetail
