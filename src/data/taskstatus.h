#pragma once

#include <QMetaType>

// Execution state shared by workers and presentation; no widget dependency.
enum class TaskStatus { Idle, Running, Pass, Fail };
Q_DECLARE_METATYPE(TaskStatus)
