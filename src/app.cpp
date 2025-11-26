#include "app.h"
#include <iostream>
#include <csignal>
#include "phasor_injection_test.h"
#include "comtrade_replay_test.h"
#include "scd_parser.h"

// Global references for signal handlers
static PhasorInjectionTest* g_phasorTestInstance = nullptr;
static ComtradeReplayTest* g_comtradeTestInstance = nullptr;

void signalHandler(int) {
    if (g_phasorTestInstance) {
        g_phasorTestInstance->stop();
    }
    if (g_comtradeTestInstance) {
        g_comtradeTestInstance->stop();
    }
}

App::App() {
    // Constructor
}

App::~App() {
    // Destructor
}

int testPhasorInjection(PhasorInjectionConfig config) {
    // Create test instance
    PhasorInjectionTest test;
    
    // Register signal handler
    g_phasorTestInstance = &test;
    std::signal(SIGINT, signalHandler);
    
    // Optional: Set callbacks
    test.setGooseCallback([](const std::string& gocbRef, uint32_t stNum, uint32_t sqNum) {
        std::cout << "[Callback] GOOSE: " << gocbRef 
                  << " (stNum=" << stNum << ", sqNum=" << sqNum << ")" << std::endl;
    });
    
    // Progress callback is handled by the class when verboseOutput is true
    test.setProgressCallback([](uint32_t, double) {
        // Custom progress handling if needed
    });
    
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
    
    // Get and display statistics
    auto stats = test.getStatistics();
    std::cout << "\nFinal Statistics:" << std::endl;
    std::cout << "  Packets sent: " << stats.packetsSent << std::endl;
    std::cout << "  Packets failed: " << stats.packetsFailed << std::endl;
    std::cout << "  Average rate: " << stats.getAverageRate() << " packets/sec" << std::endl;
    
    g_phasorTestInstance = nullptr;
    return 0;
}

int testComtradeReplay(ComtradeReplayConfig config) {
    
    // Create test instance
    ComtradeReplayTest test;
    
    // Register signal handler
    g_comtradeTestInstance = &test;
    std::signal(SIGINT, signalHandler);
    
    
    // Configure the test
    if (!test.configure(config)) {
        std::cerr << "Failed to configure test: " << test.getLastError() << std::endl;
        g_comtradeTestInstance = nullptr;
        return 1;
    }
    
    // Run the test
    if (!test.run()) {
        std::cerr << "Failed to run test: " << test.getLastError() << std::endl;
        g_comtradeTestInstance = nullptr;
        return 1;
    }
    
    g_comtradeTestInstance = nullptr;
    return 0;
}

int run_phasor_injection(){
    PhasorInjectionConfig config;
    config.iface = "eth0";
    config.dstMac = "01:0C:CD:01:00:00";
    config.vlanId = 4;
    config.vlanPriority = 4;
    config.appId = 0x4000;
    config.svId = "TestSV01";
    config.sampleRate = 4800;
    config.stopGooseRef = "STOP";
    config.enableGooseMonitoring = false;
    config.verboseOutput = true;
    config.progressInterval = 1000;
    
    // Set phasors: [magnitude, phase_degrees]
    config.phasors[0][0] = 100.0;    config.phasors[0][1] = 0.0;      // IA
    config.phasors[1][0] = 100.0;    config.phasors[1][1] = -120.0;   // IB
    config.phasors[2][0] = 100.0;    config.phasors[2][1] = 120.0;    // IC
    config.phasors[3][0] = 0.0;      config.phasors[3][1] = 0.0;      // IN
    config.phasors[4][0] = 69500.0;  config.phasors[4][1] = 0.0;      // VA
    config.phasors[5][0] = 69500.0;  config.phasors[5][1] = -120.0;   // VB
    config.phasors[6][0] = 69500.0;  config.phasors[6][1] = 120.0;    // VC
    config.phasors[7][0] = 0.0;      config.phasors[7][1] = 0.0;      // VN
    
    return testPhasorInjection(config);
}

int run_comtrade_replay(){
    ComtradeReplayConfig config;
    
    // COMTRADE file paths
    config.cfgFilePath = "FRA00030.cfg";
    config.datFilePath = "";  // Auto-detected
    
    // Network configuration
    config.iface = "en0";
    config.dstMac = "01:0C:CD:01:00:00";
    config.srcMac = "";  // Auto-detected
    
    // VLAN configuration
    config.vlanId = 4;
    config.vlanPriority = 4;
    
    // SV configuration
    config.appId = 0x4000;
    config.svId = "ComtradeReplay";
    config.sampleRate = 4800;
    
    // Channel mapping
    config.channelMapping = {
        {"3TCC9:I A", 0},
        {"3TCC9:I B", 1},
        {"3TCC9:I C", 2},
        {"3TCC9:IN", 3},
        {"3TPM3:V A", 4},
        {"3TPM3:V B", 5},
        {"3TPM3:V C", 6},
    };
    
    // GOOSE stop configuration
    config.stopGooseRef = "STOP";
    config.enableGooseMonitoring = false;
    
    // Replay control
    config.loopPlayback = false;
    config.startTimeOffset = 0.0;
    config.endTimeOffset = 0.0;
    
    // Display configuration
    config.verboseOutput = true;
    config.progressInterval = 1000;
    
    return testComtradeReplay(config);
}

int save_scd_file(const std::string& path) {
    std::cout << "\n=== SCD File Generation ===\n" << std::endl;
    
    // Create SV control configuration - modify these values as needed
    SampledValueControl config;
    config.name = "MSVCB1";                      // Control block name
    config.svID = "SV_Phasors_1";               // SV identifier
    config.dataSet = "PhsCurrs";                // Dataset name
    config.multicast = true;                     // Multicast mode
    config.smpMod = "SmpPerPeriod";             // Sampling mode
    config.smpRate = 80;                         // Samples per period (80 or 256 typical)
    config.noASDU = 1;                           // Number of ASDUs per frame
    config.confRev = 1;                          // Configuration revision
    config.macAddress = "01-0C-CD-04-00-01";    // Multicast MAC address
    config.appId = 0x4000;                       // Application ID
    config.vlanId = 0;                           // VLAN ID (0 = no VLAN)
    config.vlanPriority = 4;                     // VLAN priority (0-7)
    
    // Display configuration summary
    std::cout << "\n--- Configuration Summary ---" << std::endl;
    std::cout << "SV ID:         " << config.svID << std::endl;
    std::cout << "Control Name:  " << config.name << std::endl;
    std::cout << "DataSet:       " << config.dataSet << std::endl;
    std::cout << "MAC Address:   " << config.macAddress << std::endl;
    std::cout << "APPID:         0x" << std::hex << config.appId << std::dec << std::endl;
    std::cout << "Sample Rate:   " << config.smpRate << std::endl;
    std::cout << "VLAN ID:       " << config.vlanId << std::endl;
    std::cout << "VLAN Priority: " << config.vlanPriority << std::endl;
    std::cout << "noASDU:        " << config.noASDU << std::endl;
    
    // Generate SCD file
    std::cout << "\nGenerating SCD file: " << path << std::endl;
    
    if (ScdParser::generateSCD(config, path)) {
        std::cout << "✓ SCD file generated successfully: " << path << std::endl;
    } else {
        std::cerr << "✗ Failed to generate SCD file" << std::endl;
        return 1;
    }
    
    std::cout << "\n=== SCD Generation Complete ===\n" << std::endl;
    return 0;
}

int App::run(int, char**) {
    run_phasor_injection();
    // run_comtrade_replay();
    // save_scd_file("generated_scd.scd");
    return 0;
}

