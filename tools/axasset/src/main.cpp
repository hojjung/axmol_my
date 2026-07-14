#include "AssetPipeline.h"

#include <iostream>
#include <string>

int main(int argc, const char* const* argv)
{
    axasset::Options options;
    std::string error;
    switch (axasset::parseArguments(argc, argv, options, error))
    {
    case axasset::ParseResult::Help:
        axasset::printUsage();
        return 0;
    case axasset::ParseResult::Error:
        std::cerr << "axasset: " << error << "\n\n";
        axasset::printUsage();
        return 64;
    case axasset::ParseResult::Run:
        return axasset::runPipeline(options);
    }
    return 70;
}
