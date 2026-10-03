#pragma once
enum class OutputState
{
    ConfirmedOff,
    ConfirmedOn,
    Unknown
};
inline bool outputMayBeOn(OutputState state) { return state != OutputState::ConfirmedOff; }
inline OutputState confirmedOutputState(bool on)
{
    return on ? OutputState::ConfirmedOn : OutputState::ConfirmedOff;
}
