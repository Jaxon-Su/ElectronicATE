#pragma once
#include <QString>
#include <array>

namespace Page5TaskKind
{
enum class Kind {
    Unknown,
    Static,
    Dynamic,
    DynamicGroup,
    StaticGroup,
    TurnOnGroup,
    TurnOffGroup,
    OnShortGroup,
    ShortOnGroup,
    TurnOn,
    TurnOff,
    OnShort,
    ShortOn,
    Relay,
    WriteScope,
    Capture,
    Delay
};
struct Descriptor {
    Kind kind;
    const char *name;
    const char *group;
    bool measurement;
    Kind single = Kind::Unknown;
};
inline constexpr std::array<Descriptor, 16> descriptors{
    {{Kind::Static, "Static Test", "static", true},
     {Kind::Dynamic, "Dynamic Test", "dynamic", true},
     {Kind::StaticGroup, "Static Group Test", "staticGroup", true, Kind::Static},
     {Kind::DynamicGroup, "Dynamic Group Test", "dynamicGroup", true, Kind::Dynamic},
     {Kind::TurnOnGroup, "Turn on Group Test", "turnOnGroup", true, Kind::TurnOn},
     {Kind::TurnOffGroup, "Turn off Group Test", "turnOffGroup", true, Kind::TurnOff},
     {Kind::ShortOnGroup, "Short then turn on Group Test", "shortOnGroup", true, Kind::ShortOn},
     {Kind::OnShortGroup, "Turn on then short Group Test", "onShortGroup", true, Kind::OnShort},
     {Kind::TurnOn, "Turn on", "turnOn", true},
     {Kind::TurnOff, "Turn off", "turnOff", true},
     {Kind::OnShort, "Turn on then short", "onShort", true},
     {Kind::ShortOn, "Short then turn on", "shortOn", true},
     {Kind::Relay, "Relay", "relay", false},
     {Kind::WriteScope, "Write Oscilloscope", "osc", false},
     {Kind::Capture, "Capture", "capture", false},
     {Kind::Delay, "Delay", "delay", false}}};
inline const Descriptor *find(const QString &name)
{
    for (const auto &descriptor : descriptors)
        if (name == QLatin1String(descriptor.name))
            return &descriptor;
    return nullptr;
}
inline Kind fromName(const QString &name)
{
    const auto *descriptor = find(name);
    return descriptor ? descriptor->kind : Kind::Unknown;
}
inline Kind fromSettingsGroup(const QString &group)
{
    for (const auto &descriptor : descriptors)
        if (group == QLatin1String(descriptor.group))
            return descriptor.kind;
    return Kind::Unknown;
}
inline Kind singleKind(Kind kind)
{
    for (const auto &descriptor : descriptors)
        if (descriptor.kind == kind && descriptor.single != Kind::Unknown)
            return descriptor.single;
    return kind;
}
inline bool isGroup(Kind kind) { return singleKind(kind) != kind; }
inline bool isSteady(Kind kind)
{
    return singleKind(kind) == Kind::Static || singleKind(kind) == Kind::Dynamic;
}
inline bool isTransient(Kind kind)
{
    const auto single = singleKind(kind);
    return single == Kind::TurnOn || single == Kind::TurnOff || single == Kind::OnShort || single == Kind::ShortOn;
}
inline QString name(Kind kind)
{
    for (const auto &descriptor : descriptors)
        if (descriptor.kind == kind)
            return QString::fromLatin1(descriptor.name);
    return {};
}
inline QString loadKey(Kind kind) { return singleKind(kind) == Kind::Dynamic ? "dyload" : "load"; }
inline QString settingsGroup(const QString &name)
{
    const auto *descriptor = find(name);
    return descriptor ? QString::fromLatin1(descriptor->group) : QString{};
}
inline bool isMeasurement(const QString &name)
{
    const auto *descriptor = find(name);
    return descriptor && descriptor->measurement;
}
} // namespace Page5TaskKind
