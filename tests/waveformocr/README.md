# WaveformOcr regression tests

Tests build the production OCR targets and WaveformOcr view without NI-VISA, hardware or a running Ollama. All input is synthetic and temporary; no user waveform data is included. A loopback HTTP stub exercises the actual asynchronous client and ViewModel.

The complete local gate runs this suite as well as the main suite; use the [shared runner](../README.md). It checks the fixed inventory and QtTest XML, including method-level skips. Standalone commands below are for OCR-only diagnosis and do not establish a complete project PASS. Python 3.10+ and CMake/CTest 3.21+ are required by the test policy/reporting in addition to Qt 6 and a compatible C++17 compiler. For example, with Ninja on PATH:

```powershell
$env:PATH = "C:/Qt/6.12.0/mingw_64/bin;C:/Qt/Tools/mingw1310_64/bin;C:/Qt/Tools/Ninja;$env:PATH"
cmake -S tests/waveformocr -B build/waveform-ocr-tests -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/mingw_64 -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe
cmake --build build/waveform-ocr-tests --target parser_regression waveform_ocr_regression
ctest --test-dir build/waveform-ocr-tests --output-on-failure --no-tests=error
```

Coverage: measurement cards, GLM JSON transcription, units, CH1/CH2/CH3 separation, missing/ambiguous sources, statistics tables, conflicting values, numeric edits and optional metadata, unchanged edits and review state, editor Enter/Escape, original evidence, asynchronous batch/stop/restart, model queries, cancelled scan/export, Excel contents and filename collisions. Configuration also runs the production architecture guard. These tests verify software behavior, not a vision model's accuracy or real instrument control.
