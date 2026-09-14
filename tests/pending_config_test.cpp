#include "pendingconfigupdate.h"
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

Page1Config config(int outputs)
{
    Page1Config value;
    value.loadOutputs = outputs;
    return value;
}

int main()
{
    try {
        PendingConfigUpdate updates;
        require(!updates.tryStart(false), "empty queue started work");
        auto initial = config(1);
        updates.enqueue(initial);
        initial.loadOutputs = 99;
        require(!updates.tryStart(true) && updates.hasPending(), "capture blocker lost update");
        auto first = updates.tryStart(false);
        require(first && first->loadOutputs == 1 && updates.isRunning(), "snapshot ownership failed");
        updates.enqueue(config(2));
        require(!updates.tryStart(false), "second build overlapped first");
        updates.enqueue(config(3));
        require(!updates.tryStart(false) && updates.hasPending(), "busy attempt discarded latest settings");
        updates.complete();
        auto latest = updates.tryStart(false);
        require(latest && latest->loadOutputs == 3, "completion did not retain newest config");
        require(first->loadOutputs == 1, "pending update changed in-flight snapshot");
        // Failure uses the same completion transition and must allow a subsequent retry.
        updates.enqueue(config(4));
        updates.complete();
        require(!updates.tryStart(true) && !updates.isRunning(), "blocked retry marked running");
        auto retry = updates.tryStart(false);
        require(retry && retry->loadOutputs == 4, "failure completion stranded next request");
        updates.complete();
        require(!updates.hasPending() && !updates.tryStart(false), "completion fabricated more work");
        std::cout << "PASS: pending settings coalesce, preserve snapshots and survive busy/failure states\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
