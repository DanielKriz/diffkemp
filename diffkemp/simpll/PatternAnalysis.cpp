#include "PatternAnalysis.h"

void readPatternConfig(std::string configPath) {
    std::vector<PatternInfo> patterns;
    auto buf = llvm::MemoryBuffer::getFile(configPath);
    llvm::yaml::Input yin(**buf);
    yin >> patterns;

    for (const auto &pattern : patterns) {
        bool isGenerationSuccess = true;
        for (const auto &c : pattern.candidates) {
            auto FunPair = c.function.split(',');
            FunPair.second =
                    FunPair.second.empty() ? FunPair.first : FunPair.second;

            auto OldSnapshotBuf =
                    llvm::MemoryBuffer::getFile(c.oldSnapshotPath);
            auto NewSnapshotBuf =
                    llvm::MemoryBuffer::getFile(c.newSnapshotPath);
            llvm::yaml::Input oldYin(**OldSnapshotBuf);
            llvm::yaml::Input newYin(**NewSnapshotBuf);

            std::vector<Snapshot> OldSnapshots;
            std::vector<Snapshot> NewSnapshots;

            newYin >> NewSnapshots;
            oldYin >> OldSnapshots;

            std::string OldFunPath;
            std::string NewFunPath;
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
            std::cout << Color::makeGreen("Successfule generation of pattern: ")
                      << pattern.name << std::endl;
            std::cout << *(*gPatternGen)[pattern.name] << std::endl;
            auto pat = (*gPatternGen)[pattern.name];
            int i = 0;
            for (auto varIter = pat->variants.begin();
                 varIter != pat->variants.end();
                 ++varIter) {
                auto varMod = pat->generateVariant({*varIter, *(++varIter)},
                                                   "-var-" + std::to_string(i));
                gPatternGen->determinePatternRange(pat, *varMod);
                if (!varMod->empty()) {
                    llvm::outs() << Color::makeYellow(
                            "Generated Variant no. " + std::to_string(i) + ": ")
                                 << varMod->getName().str() << "\n";
                    varMod->print(llvm::outs(), nullptr);
                    ++i;
                }
            }
        } else {
            llvm::outs() << Color::makeRed("Generation of pattern ")
                         << Color::makeRed(" failed: ") << "pattern '"
                         << pattern.name << "' could not be generated"
                         << "\n";
        }
    }
}
