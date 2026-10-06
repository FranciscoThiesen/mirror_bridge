#pragma once

// Every shipped C++ library puts a visibility macro in front of every class:
// jsoncpp spells it JSON_API, V8 spells it V8_EXPORT, OpenCV CV_EXPORTS, Qt
// QT_WIDGETS_EXPORT, LLVM LLVM_ABI. This header is the fixture's version of
// that, and the reason class discovery cannot be a scan of the header text.
#if defined(_WIN32)
#  define EXPORTLIB_API __declspec(dllexport)
#else
#  define EXPORTLIB_API __attribute__((visibility("default")))
#endif
