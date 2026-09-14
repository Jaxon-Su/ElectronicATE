#pragma once

class Page1ViewModel;
class Page2ViewModel;
class Page3ViewModel;
class Page5ViewModel;

// Presentation composition: wire page dependencies after constructing the VMs.
// Connections use receiver lifetimes; this coordinator owns no page state.
class PageConnectionCoordinator final {
public:
    static void setupPageConnections(Page1ViewModel* vm1, Page2ViewModel* vm2,
                                     Page3ViewModel* vm3, Page5ViewModel* vm5);

private:
    PageConnectionCoordinator() = delete;
    static void connectPage1ToPage2(Page1ViewModel* vm1, Page2ViewModel* vm2);
    static void connectPage1ToPage3(Page1ViewModel* vm1, Page3ViewModel* vm3);
    static void connectPage2ToPage3(Page2ViewModel* vm2, Page3ViewModel* vm3);
    static void connectPage1ToPage5(Page1ViewModel* vm1, Page5ViewModel* vm5);
    static void connectPage2ToPage5(Page2ViewModel* vm2, Page5ViewModel* vm5);
};
