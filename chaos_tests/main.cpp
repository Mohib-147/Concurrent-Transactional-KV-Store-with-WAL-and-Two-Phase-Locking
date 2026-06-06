#include <iostream>

static bool crash_test_started = false;

bool runScenarioCounter();
bool runScenarioRMW();
void runScenario3();
bool runScenarioIsolation();
bool runScenarioCrash();
bool verifyScenarioCrash();
bool runScenarioDeadlock();

void runAll()
{
    std::cout << "\n[CHAOS] Running full test suite...\n\n";

    runScenarioCounter();
    runScenarioRMW();
    runScenario3();
    runScenarioIsolation();
    runScenarioCrash();

    std::cout << "\n⚠️ Restart server manually now...\n";
    std::cout << "Then choose VERIFY option.\n";

    runScenarioDeadlock();

    std::cout << "\n[CHAOS] ALL TESTS COMPLETED\n";
}

int main()
{
    while (true)
    {
        std::cout << "\n==== CHAOS TEST DRIVER ====\n";
        std::cout << "1. Scenario 1 (Counter)\n";
        std::cout << "2. Scenario 2 (RMW)\n";
        std::cout << "3. Scenario 3 (Reader-Writer)\n";
        std::cout << "4. Scenario 4 (Isolation)\n";
        std::cout << "5. Scenario 5 - RUN CRASH TEST\n";
        std::cout << "6. Scenario 5 - VERIFY AFTER RESTART\n";
        std::cout << "7. Scenario 6 (Deadlock)\n";
        std::cout << "8. Run ALL\n";
        std::cout << "0. Exit\n";

        std::cout << "Choice: ";

        int choice;
        std::cin >> choice;

        switch (choice)
        {
        case 1:
            runScenarioCounter();
            break;

        case 2:
            runScenarioRMW();
            break;

        case 3:
            runScenario3();
            break;

        case 4:
            runScenarioIsolation();
            break;

        case 5:
            crash_test_started = true;
            runScenarioCrash();
            break;

        case 6:
            if (!crash_test_started)
                std::cout << "[ERROR] Run crash test first\n";
            else
                verifyScenarioCrash();
            break;

        case 7:
            runScenarioDeadlock();
            break;

        case 8:
            runAll();
            break;

        case 0:
            return 0;

        default:
            std::cout << "Invalid choice\n";
        }
    }
}