#pragma once
#include <QString>
#include <QStringList>
#include "src/core/runtime/boardprofile.h"

// Static analysis over sketch source text -- pin-range/PWM/ISR/etc. checks
// and compiler-error humanization. No Qt-widget dependency: everything here
// is a pure function of the source text (and, for checkSource, the target
// board), so it doesn't need a MainWindow to run.
namespace SketchLinter {

// Pin range, PWM, ISR, and similar warnings for the given board.
QStringList checkSource(const QString& source, const BoardProfile& board);

// True if a #define/const name looks like a pin definition (LED_PIN, BTN, etc).
bool isPinName(const std::string& name);

// Names of #defines, functions, and variables in the source, for autocomplete.
QStringList scanSymbols(const QString& source);

// Rewrites cryptic GCC error messages to plain English.
QString humanizeErrors(const QString& raw);

} // namespace SketchLinter
