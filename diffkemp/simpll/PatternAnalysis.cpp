#include "PatternAnalysis.h"

void generatePattern(std::string pattern,
                     std::string function,
                     std::string fileName) {
    gPatternGen->addFileForInference(pattern, function, fileName);
}

// TODO: This should probably return a string, not just simply print it
void reportPatternImpl() {
    std::cout << "This is an evergoing problem" << std::endl;
    // llvm::yaml::Output output(llvm::outs());
    // output << *gPatternGen;
}
