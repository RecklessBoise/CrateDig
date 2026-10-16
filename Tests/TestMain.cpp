#include <juce_core/juce_core.h>

int main (int argc, char* argv[])
{
    juce::ignoreUnused (argc, argv);

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("CrateDig");

    int failures = 0;

    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        auto* result = runner.getResult (i);
        failures += result->failures;

        // JUCE logs to the debugger on Windows, so print failure details to the console too.
        if (result->failures > 0)
        {
            std::cout << "FAILED: " << result->unitTestName << " / " << result->subcategoryName << std::endl;

            for (auto& message : result->messages)
                std::cout << "  " << message << std::endl;
        }
    }

    std::cout << (failures == 0 ? "All tests passed" : juce::String (failures) + " test failure(s)") << std::endl;
    return failures == 0 ? 0 : 1;
}
