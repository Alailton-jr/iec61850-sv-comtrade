#ifndef PHASOR_INJECTION_TEST_H
#define PHASOR_INJECTION_TEST_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <functional>

// Forward declarations
class RawSocket;
class Ethernet;
class Virtual_LAN;
class SampledValue;

/**
 * @brief Configuration for Phasor Injection Test
 */
struct PhasorInjectionConfig {
    // Network configuration
    std::string interface = "en0";
    std::string dstMac = "01:0C:CD:01:00:00";  // SV multicast
    std::string srcMac;  // Auto-detected from interface
    
    // VLAN configuration
    uint16_t vlanId = 4;
    uint8_t vlanPriority = 4;
    
    // SV configuration
    uint16_t appId = 0x4000;
    std::string svId = "TestSV01";
    uint16_t sampleRate = 4800;  // samples/sec
    
    // GOOSE stop configuration
    std::string stopGooseRef = "STOP";
    bool enableGooseMonitoring = true;
    
    // Phasor values [magnitude, angle_degrees]
    // IA, IB, IC, IN, VA, VB, VC, VN
    double phasors[8][2] = {
        {100.0, 0.0},    // IA
        {100.0, -120.0}, // IB
        {100.0, 120.0},  // IC
        {0.0, 0.0},      // IN
        {69500.0, 0.0},  // VA
        {69500.0, -120.0}, // VB
        {69500.0, 120.0},  // VC
        {0.0, 0.0}       // VN
    };
    
    // Display configuration
    bool verboseOutput = true;
    uint32_t progressInterval = 1000;  // Print progress every N packets
};

/**
 * @brief Statistics from the phasor injection test
 */
struct PhasorInjectionStats {
    uint32_t packetsSent = 0;
    uint32_t packetsFailed = 0;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;
    bool stoppedByGoose = false;
    std::string gooseStopReason;
    
    double getElapsedSeconds() const {
        auto duration = endTime - startTime;
        return std::chrono::duration_cast<std::chrono::duration<double>>(duration).count();
    }
    
    double getAverageRate() const {
        double elapsed = getElapsedSeconds();
        return elapsed > 0 ? packetsSent / elapsed : 0.0;
    }
};

/**
 * @brief IEC 61850-9-2 Phasor Injection Test
 * 
 * This class manages the complete lifecycle of a phasor injection test:
 * - Opens raw socket for SV transmission
 * - Monitors network for GOOSE stop messages
 * - Injects SV packets with user-defined phasor values
 * - Tracks statistics and performance
 * 
 * Example usage:
 * @code
 * PhasorInjectionTest test;
 * PhasorInjectionConfig config;
 * config.interface = "en0";
 * config.phasors[0][0] = 100.0;  // IA magnitude
 * config.phasors[0][1] = 0.0;    // IA angle
 * 
 * if (test.configure(config)) {
 *     test.run();
 *     auto stats = test.getStatistics();
 *     std::cout << "Sent " << stats.packetsSent << " packets" << std::endl;
 * }
 * @endcode
 */
class PhasorInjectionTest {
public:
    PhasorInjectionTest();
    ~PhasorInjectionTest();
    
    /**
     * @brief Configure the test with provided parameters
     * @param config Test configuration
     * @return true on success, false on failure
     */
    bool configure(const PhasorInjectionConfig& config);
    
    /**
     * @brief Start the phasor injection test
     * @return true on success, false on failure
     */
    bool run();
    
    /**
     * @brief Stop the running test gracefully
     */
    void stop();
    
    /**
     * @brief Check if test is currently running
     * @return true if running, false otherwise
     */
    bool isRunning() const;
    
    /**
     * @brief Get current test statistics
     * @return Statistics structure
     */
    PhasorInjectionStats getStatistics() const;
    
    /**
     * @brief Get last error message
     * @return Error message string
     */
    std::string getLastError() const;
    
    /**
     * @brief Set callback for GOOSE message reception
     * @param callback Function to call when GOOSE is received
     */
    void setGooseCallback(std::function<void(const std::string& gocbRef, uint32_t stNum, uint32_t sqNum)> callback);
    
    /**
     * @brief Set callback for progress updates
     * @param callback Function to call with packet count and elapsed time
     */
    void setProgressCallback(std::function<void(uint32_t packets, double seconds)> callback);
    
    /**
     * @brief Print current configuration to console
     */
    void printConfiguration() const;
    
    /**
     * @brief Print test statistics to console
     */
    void printStatistics() const;

private:
    // Configuration
    PhasorInjectionConfig config_;
    
    // Statistics
    PhasorInjectionStats stats_;
    
    // Runtime state
    std::atomic<bool> running_;
    std::string lastError_;
    
    // Threading
    std::thread gooseThread_;
    
    // Callbacks
    std::function<void(const std::string&, uint32_t, uint32_t)> gooseCallback_;
    std::function<void(uint32_t, double)> progressCallback_;
    
    // Internal methods
    void gooseCaptureThreadFunc();
    bool openSocket();
    void transmissionLoop();
};

#endif // PHASOR_INJECTION_TEST_H
