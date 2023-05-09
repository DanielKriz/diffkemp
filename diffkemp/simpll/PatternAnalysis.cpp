//===-------------- PatternAnalysis.h - Code pattern finder ---------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Daniel Kriz, xkrizd03@vutbr.cz
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the definitions functions used for analysis of patterns
/// for generation.
///
//===----------------------------------------------------------------------===//

#include "PatternAnalysis.h"

void readPatternConfig(std::string configPath) {
    using llvm::yaml::Input;
    std::vector<PatternInfo> patterns;
    auto buf = MemoryBuffer::getFile(configPath);
    Input yin(**buf);
    yin >> patterns;

    for (const auto &pattern : patterns) {
        bool isGenerationSuccess = true;
        for (const auto &c : pattern.candidates) {
            /// It is possible to define a function alias
            auto FunPair = c.function.split(',');
            FunPair.second =
                    FunPair.second.empty() ? FunPair.first : FunPair.second;

            /// Read the old and new file for the candiadate difference
            auto OldSnapshotBuf = MemoryBuffer::getFile(c.oldSnapshotPath);
            auto NewSnapshotBuf = MemoryBuffer::getFile(c.newSnapshotPath);
            Input oldYin(**OldSnapshotBuf);
            Input newYin(**NewSnapshotBuf);

            std::vector<Snapshot> OldSnapshots;
            std::vector<Snapshot> NewSnapshots;

            newYin >> NewSnapshots;
            oldYin >> OldSnapshots;

            std::string OldFunPath;
            std::string NewFunPath;

            /// Assign paths the LLVM IR files of seached functions
            for (auto const &s : OldSnapshots) {
                for (auto const &f : s.Functions) {
                    if (f.Name == FunPair.first) {
                        OldFunPath = s.SrcDir + "/" + f.PathToLLVM;
                    }
                }
            }
            for (auto const &s : NewSnapshots) {
                for (auto const &f : s.Functions) {
                    if (f.Name == FunPair.second) {
                        NewFunPath = s.SrcDir + "/" + f.PathToLLVM;
                    }
                }
            }

            /// Add functions to the pattern identified by name provided in
            /// the configuration file.
            if (!gPatternGen->addFunctionPairToPattern(
                        std::make_pair(OldFunPath, NewFunPath),
                        std::make_pair(FunPair.first.str(),
                                       FunPair.second.str()),
                        pattern.name)) {
                isGenerationSuccess = false;
                break;
            }
        }
        if (isGenerationSuccess) {
            std::cout << Color::makeGreen("Generation of pattern succeeded: ")
                      << pattern.name << "\n";
            auto pat = (*gPatternGen)[pattern.name];
            /// Rename sides of the pattern to 'side', otherwise each side
            /// could have different name and then it would not work.
            pat->renameSides("side");
            /// Determine pattern ranges for the base pattern, as it should
            /// also be appliable.
            gPatternGen->determinePatternRange(pat, *pat->mod.get());
            std::cout << *pat << std::endl;

            /// Generate variants of the base pattern
            int i = 0;
            for (auto varIter = pat->variants.begin();
                 varIter != pat->variants.end();
                 ++varIter) {
                auto var = pat->generateVariant({*varIter, *(++varIter)},
                                                "-var-" + std::to_string(i));
                if (var.first) {
                    gPatternGen->determinePatternRange(pat, *var.second);
                    outs() << Color::makeYellow("Generated Variant no. "
                                                + std::to_string(i) + ": ")
                           << var.second->getName().str() << "\n";
                    var.second->print(llvm::outs(), nullptr);
                    ++i;
                }
            }
        } else {
            errs() << Color::makeRed("Generation of pattern failed: ")
                   << "pattern '" << pattern.name << "' could not be generated"
                   << "\n";
        }
    }
}
