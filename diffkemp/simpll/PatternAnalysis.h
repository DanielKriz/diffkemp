//===-------------- PatternAnalysis.h - Code pattern finder ---------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Daniel Kriz, xkrizd03@vutbr.cz
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration functions used for analysis of patterns
/// for generation.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_PATTERN_ANALYSIS_H
#define DIFFKEMP_SIMPLL_PATTERN_ANALYSIS_H

#include <iostream>
#include <memory>
#include <string>

#include "Config.h"
#include "Output.h"
#include "PatternGenerator.h"
#include "Snapshot.h"
#include "Utils.h"

using namespace llvm;

/// Private (module scope) instance of the pattern generator.
///
/// NOTE: this is another flaw in the design, we have tried to remove it, but
/// there is some connection between its LLVM contexts and contexts we are using
/// in base patterns. Due to the time pressure during finalization we were not
/// able to resolve it.
static auto gPatternGen = std::make_unique<PatternGenerator>();

/// Reads pattern config and creates new patterns in accordance to this
/// configuration.
void readPatternConfig(std::string configPath);

#endif // DIFFKEMP_SIMPLL_PATTERN_ANALYSIS_H
