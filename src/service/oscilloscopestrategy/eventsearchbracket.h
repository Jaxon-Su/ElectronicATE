#pragma once
#include "../measurement/triggerlevelgrid.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

// Search coordinates always point outward. The caller maps them to Rise/Fall levels.
// No instrument operations, timing or measurement accumulation belong in this class.
class EventSearchBracket
{
  public:
    enum class Progress { Continue, Converged, NoEvent };
    EventSearchBracket(double initial, double step) : m_candidate(initial) { reset(step); }
    double candidate() const { return m_candidate; }
    bool bounded() const { return m_haveHit && m_haveMiss; }
    double hit() const { return m_hit; }
    double miss() const { return m_miss; }
    void reset(double step)
    {
        if (!std::isfinite(step) || step <= 0)
            throw std::invalid_argument("Invalid search step");
        m_expansion = step;
        m_tolerance = step;
        m_candidate = TriggerLevelGrid(step).atOrAbove(m_candidate);
        m_haveHit = m_haveMiss = false;
        m_inwardAttempts = 0;
    }
    Progress baseline(double floor)
    {
        if (m_haveHit)
            throw std::runtime_error("Baseline changed inside event search bracket");
        const double next = TriggerLevelGrid(m_tolerance).above(floor);
        if (m_haveMiss && next >= m_miss)
            return Progress::NoEvent;
        m_candidate = next;
        return Progress::Continue;
    }
    Progress observe(bool captured, double observedPeak, double baselineFloor)
    {
        const TriggerLevelGrid grid(m_tolerance);
        const double previous = m_candidate;
        if (captured) {
            m_hit = m_candidate;
            m_haveHit = true;
            if (m_haveMiss && observedPeak >= m_miss)
                m_haveMiss = false;
        } else {
            m_miss = m_candidate;
            m_haveMiss = true;
        }
        if (bounded()) {
            if (m_miss <= m_hit)
                throw std::runtime_error("Inconsistent event trigger bracket");
            const double hitIndex = grid.ceilIndex(m_hit), missIndex = grid.ceilIndex(m_miss);
            if (missIndex - hitIndex <= 1)
                return Progress::Converged;
            m_candidate = grid.valueAt(std::floor(hitIndex + (missIndex - hitIndex) / 2));
        } else if (m_haveHit) {
            m_candidate = std::max(grid.atOrAbove(m_hit + m_expansion), grid.above(observedPeak));
            m_expansion *= 2.0;
        } else {
            if (++m_inwardAttempts >= 8)
                return Progress::NoEvent;
            const double inner = std::isfinite(baselineFloor) ? baselineFloor : 0.0;
            const double innerIndex = grid.floorIndex(inner), missIndex = grid.ceilIndex(m_miss);
            if (missIndex - innerIndex <= 1)
                return Progress::NoEvent;
            m_candidate = grid.valueAt(std::floor(innerIndex + (missIndex - innerIndex) / 2));
        }
        if (!std::isfinite(m_candidate) || std::abs(m_candidate) >= 1e30 || m_candidate == previous)
            throw std::runtime_error("Search threshold cannot advance");
        return Progress::Continue;
    }

  private:
    double m_candidate;
    double m_hit = 0;
    double m_miss = 0;
    double m_expansion = 0;
    double m_tolerance = 0;
    int m_inwardAttempts = 0;
    bool m_haveHit = false;
    bool m_haveMiss = false;
};
