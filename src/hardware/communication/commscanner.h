#pragma once

// Resource enumeration runs separately so a blocked VISA call cannot lock the UI.
int runCommScanner();
