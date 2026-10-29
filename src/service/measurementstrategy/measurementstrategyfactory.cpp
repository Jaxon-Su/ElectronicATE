#include "measurementstrategyfactory.h"
#include "scopemeasurementstrategy.h"
#include "../brownout/brownoutstrategy.h"
#include <type_traits>
#include <utility>

MeasurementStrategyFactory makeMeasurementStrategyFactory(ScopeAlgorithmFactory scopeAlgorithms)
{
    return [algorithms = std::move(scopeAlgorithms)](const MeasurementStrategyRequest &request) {
        return std::visit([&](const auto &typed) -> std::unique_ptr<IMeasurementStrategy> {
            using Request = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<Request, ScopeMeasurementRequest>) {
                if (!algorithms) return {};
                auto algorithm = algorithms(typed.taskName, &typed.context);
                if (!algorithm) return {};
                if (typed.capture) algorithm->setCaptureObserver(typed.capture);
                algorithm->setLogObserver(typed.context.log);
                return std::make_unique<ScopeMeasurementStrategy>(std::move(algorithm), typed.scope);
            } else {
                return std::make_unique<BrownOutStrategy>(typed.settings, typed.scope, typed.createIo, typed.capture, typed.log);
            }
        }, request);
    };
}
