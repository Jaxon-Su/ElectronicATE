#pragma once

#include <QVector>
#include <QMap>
#include <QString>
#include <utility>
#include "resourcecleaner.h"
#include "page1config.h"
#include "page2config.h"

class ACSource;
class DCLoad;
class RelayBase;
class ICommunication;

class InstrumentCreator
{
  public:
    struct ACSourceResult {
        ACSourceResult() = default;
        ACSourceResult(ACSource *s, ICommunication *c, bool ok, QString error = {})
            : success(ok), errorMessage(std::move(error)), m_source(s), m_comm(c)
        {
        }
        ~ACSourceResult() { ResourceCleaner::cleanupACSource(m_source, m_comm); }
        ACSourceResult(const ACSourceResult &) = delete;
        ACSourceResult &operator=(const ACSourceResult &) = delete;
        ACSourceResult(ACSourceResult &&other) noexcept { swap(other); }
        ACSourceResult &operator=(ACSourceResult &&other) noexcept
        {
            ACSourceResult previous(std::move(other));
            swap(previous);
            return *this;
        }
        void swap(ACSourceResult &other) noexcept
        {
            std::swap(m_source, other.m_source);
            std::swap(m_comm, other.m_comm);
            std::swap(success, other.success);
            std::swap(errorMessage, other.errorMessage);
        }

        ACSource *source() const { return m_source; }
        ICommunication *comm() const { return m_comm; }
        bool success = false;
        QString errorMessage;

      private:
        friend class InstrumentCreator;
        ACSource *m_source = nullptr;
        ICommunication *m_comm = nullptr;
    };

    struct DCLoadResult {
        DCLoadResult() = default;
        ~DCLoadResult() { ResourceCleaner::cleanupDCLoads(m_dcLoads, m_commMap); }
        DCLoadResult(const DCLoadResult &) = delete;
        DCLoadResult &operator=(const DCLoadResult &) = delete;
        DCLoadResult(DCLoadResult &&other) noexcept { swap(other); }
        DCLoadResult &operator=(DCLoadResult &&other) noexcept
        {
            DCLoadResult previous(std::move(other));
            swap(previous);
            return *this;
        }
        void swap(DCLoadResult &other) noexcept
        {
            std::swap(m_dcLoads, other.m_dcLoads);
            std::swap(m_commMap, other.m_commMap);
            std::swap(success, other.success);
            std::swap(errorMessage, other.errorMessage);
        }

        const QVector<DCLoad *> &dcLoads() const { return m_dcLoads; }
        const QMap<QString, ICommunication *> &commMap() const { return m_commMap; }
        bool success = false;
        QString errorMessage;

      private:
        friend class InstrumentCreator;
        QVector<DCLoad *> m_dcLoads;
        QMap<QString, ICommunication *> m_commMap;
    };

    struct RelayResult {
        RelayResult() = default;
        ~RelayResult() { ResourceCleaner::cleanupRelays(m_relays, m_commMap); }
        RelayResult(const RelayResult &) = delete;
        RelayResult &operator=(const RelayResult &) = delete;
        RelayResult(RelayResult &&other) noexcept { swap(other); }
        RelayResult &operator=(RelayResult &&other) noexcept
        {
            RelayResult previous(std::move(other));
            swap(previous);
            return *this;
        }
        void swap(RelayResult &other) noexcept
        {
            std::swap(m_relays, other.m_relays);
            std::swap(m_configurations, other.m_configurations);
            std::swap(m_commMap, other.m_commMap);
            std::swap(success, other.success);
            std::swap(errorMessage, other.errorMessage);
        }

        const QVector<RelayBase *> &relays() const { return m_relays; }
        const QVector<InstrumentConfig> &configurations() const { return m_configurations; }
        const QMap<QString, ICommunication *> &commMap() const { return m_commMap; }
        bool success = false;
        QString errorMessage;

      private:
        friend class InstrumentCreator;
        QVector<RelayBase *> m_relays;
        QVector<InstrumentConfig> m_configurations; // Same order as m_relays, including partial creation.
        QMap<QString, ICommunication *> m_commMap;
    };

    static ACSourceResult createACSource(const Page1Config &config);

    static DCLoadResult createDCLoads(const Page1Config &config, TableKind kind);

    static RelayResult createRelays(const Page1Config &config);

  private:
    InstrumentCreator() = delete;
    ~InstrumentCreator() = delete;
    InstrumentCreator(const InstrumentCreator &) = delete;
    InstrumentCreator &operator=(const InstrumentCreator &) = delete;
};
