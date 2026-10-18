#pragma once

namespace TransientDetail {
class Execution;
class Event;

// Throws on failure; the entrypoint always runs Execution::finish afterwards.
void runSearch(Execution &run, Event &event, bool retainSteadyReference);
} // namespace TransientDetail
