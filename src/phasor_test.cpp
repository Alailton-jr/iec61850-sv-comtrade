#include <iostream>
#include <csignal>
#include "phasor_injection_test.h"

// Global reference for signal handler
static PhasorInjectionTest* g_testInstance = nullptr;

void signalHandler(int) {
    if (g_testInstance) {
        g_testInstance->stop();
    }
}

/**
 * @brief Main phasor injection test
 */
int main(int argc, char* argv[]) {
    // Create test instance
    PhasorInjectionTest test;
    
    // Register signal handler
    g_testInstance = &test;
    std::signal(SIGINT, signalHandler);
    
    std::cout << "=== IEC 61850 Sampled Value Injection Test ===" << std::endl;
    std::cout << std::endl;
    
    // Configure test
    PhasorInjectionConfig config;
    
    // Parse command line arguments
    if (argc > 1) {
        config.iface = argv[1];
    }
    
    // Get phasor values from user
    std::cout << "Enter phasor values (magnitude and angle in degrees):" << std::endl;
    std::cout << std::endl;
    
    const char* labels[] = {"IA", "IB", "IC", "IN", "VA", "VB", "VC", "VN"};
    
    for (int i = 0; i < 8; i++) {
        std::cout << labels[i] << " magnitude: ";
        std::cin >> config.phasors[i][0];
        std::cout << labels[i] << " angle (deg): ";
        std::cin >> config.phasors[i][1];
    }
    
    // Configure the test
    if (!test.configure(config)) {
        std::cerr << "Failed to configure test: " << test.getLastError() << std::endl;
        return 1;
    }
    
    // Run the test
    if (!test.run()) {
        std::cerr << "Failed to run test: " << test.getLastError() << std::endl;
        return 1;
    }
    
    g_testInstance = nullptr;
    return 0;
}
