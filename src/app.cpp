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

void try_load_comtrade(const std::string& path) {
    // Create parser instance
    ComtradeParser parser;
    
    // Load the COMTRADE file
    if (!parser.load(path)) {
        std::cerr << "Error loading COMTRADE file: " << parser.getLastError() << std::endl;
        return;
    }
    
    std::cout << "✓ Successfully loaded COMTRADE file!" << std::endl;
    
    // Get configuration
    const auto& config = parser.getConfig();
    
    // Display configuration information
    std::cout << "\n--- Configuration ---" << std::endl;
    std::cout << "Station Name: " << config.stationName << std::endl;
    std::cout << "Device ID: " << config.recDeviceId << std::endl;
    std::cout << "Revision Year: " << config.revisionYear << std::endl;
    std::cout << "Line Frequency: " << config.lineFreq << " Hz" << std::endl;
    std::cout << "Total Channels: " << config.totalChannels << std::endl;
    std::cout << "  Analog: " << config.numAnalogChannels << std::endl;
    std::cout << "  Digital: " << config.numDigitalChannels << std::endl;
    std::cout << "Total Samples: " << config.totalSamples << std::endl;
    
    // Display sample rate information
    std::cout << "\n--- Sample Rates ---" << std::endl;
    for (size_t i = 0; i < config.sampleRates.size(); i++) {
        const auto& sr = config.sampleRates[i];
        std::cout << "Rate " << (i + 1) << ": " << sr.rate << " Hz (up to sample " 
                  << sr.endSample << ")" << std::endl;
    }
    
    // Display analog channels
    std::cout << "\n--- Analog Channels ---" << std::endl;
    for (const auto& ch : config.analogChannels) {
        std::cout << "[" << (ch.index + 1) << "] " << ch.name;
        if (!ch.phase.empty()) {
            std::cout << " (Phase " << ch.phase << ")";
        }
        std::cout << " - " << ch.units;
        std::cout << " [Scaling: " << ch.a << "*x + " << ch.b << "]";
        std::cout << std::endl;
    }
    
    // Display digital channels if any
    if (config.numDigitalChannels > 0) {
        std::cout << "\n--- Digital Channels ---" << std::endl;
        for (const auto& ch : config.digitalChannels) {
            std::cout << "[" << (ch.index + 1) << "] " << ch.name 
                      << " (Normal state: " << ch.normalState << ")" << std::endl;
        }
    }
    
    // Display first few samples
    std::cout << "\n--- Sample Data (first 5 samples) ---" << std::endl;
    int samplesToShow = std::min(5, config.totalSamples);
    for (int i = 0; i < samplesToShow; i++) {
        ComtradeSample sample;
        if (parser.getSample(i, sample)) {
            std::cout << "Sample " << sample.sampleNumber 
                      << " @ " << (sample.timestamp / 1000.0) << " ms" << std::endl;
            
            // Show first 3 analog values
            for (size_t j = 0; j < std::min(size_t(3), sample.analogValues.size()); j++) {
                std::cout << "  " << config.analogChannels[j].name << ": " 
                          << sample.analogValues[j] << " " << config.analogChannels[j].units 
                          << std::endl;
            }
            
            // Show digital values if any
            if (!sample.digitalValues.empty() && i == 0) {
                std::cout << "  Digital states: ";
                for (size_t j = 0; j < std::min(size_t(8), sample.digitalValues.size()); j++) {
                    std::cout << sample.digitalValues[j];
                }
                std::cout << std::endl;
            }
        }
    }
    
    std::cout << "\n✓ COMTRADE parsing complete!" << std::endl;
    std::cout << "Ready for SV packet replay (implementation pending)" << std::endl;
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

