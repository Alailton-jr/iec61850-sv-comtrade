#include "app.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include "comtrade_parser.h"
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

int run_phasor_injection(){
    PhasorInjectionConfig config = {
        .interface = "en0",
        .dstMac = "01:0C:CD:01:00:00",
        .vlanId = 4,
        .vlanPriority = 4,
        .appId = 0x4000,
        .svId = "TestSV01",
        .sampleRate = 4800,
        .stopGooseRef = "STOP",
        .enableGooseMonitoring = false,
        .verboseOutput = true,
        .progressInterval = 1000
    };
    return testPhasorInjection(config);
}

int run_comtrade_replay(){
    ComtradeReplayConfig config = {
        .cfgFilePath = "FRA00030.cfg",
        .interface = "en0",
        .sampleRate = 4800,
        .verboseOutput = true,
        .progressInterval = 1000,
        .loopPlayback = false,
        .enableGooseMonitoring = false,
        .channelMapping = {
            {"3TCC9:I A", 0},
            {"3TCC9:I B", 1},
            {"3TCC9:I C", 2},
            {"3TCC9:IN", 3},
            {"3TPM3:V A", 4},
            {"3TPM3:V B", 5},
            {"3TPM3:V C", 6},
        }
    };
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

    // run_phasor_injection();
    // run_comtrade_replay();
    save_scd_file("generated_scd.scd");
    return 0;
}

