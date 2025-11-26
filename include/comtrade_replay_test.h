#ifndef COMTRADE_REPLAY_TEST_H
#define COMTRADE_REPLAY_TEST_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <functional>

// Forward declarations
class RawSocket;
class ComtradeParser;

/**
 * @brief Configuration for COMTRADE Replay Test
 */
struct ComtradeReplayConfig {
    // COMTRADE file paths
    std::string cfgFilePath;
    std::string datFilePath;  // Optional, auto-detected if empty
    
    // Network configuration
    std::string iface = "en0";
    std::string dstMac = "01:0C:CD:01:00:00";  // SV multicast
    std::string srcMac;  // Auto-detected from interface
    
    // VLAN configuration
    uint16_t vlanId = 4;
    uint8_t vlanPriority = 4;
    
    // SV configuration
    uint16_t appId = 0x4000;
    std::string svId = "ComtradeReplay";
    uint16_t sampleRate = 4800;  // Target output sample rate (Hz)
    
    // Channel mapping: maps COMTRADE channel names to SV channel indices (0-7)
    // Format: {"COMTRADE_NAME", SV_channel_index}
    // Example: {"IA", 0}, {"IB", 1}, {"IC", 2}, {"IN", 3}
    //          {"VA", 4}, {"VB", 5}, {"VC", 6}, {"VN", 7}
    std::vector<std::pair<std::string, int>> channelMapping;
    
    // GOOSE stop configuration
    std::string stopGooseRef = "STOP";
    bool enableGooseMonitoring = true;
    
    // Replay control
    bool loopPlayback = false;  // Loop continuously
    double startTimeOffset = 0.0;  // Start at this time offset (seconds)
    double endTimeOffset = 0.0;    // End at this time offset (0 = end of file)
    
    // Display configuration
    bool verboseOutput = true;
    uint32_t progressInterval = 1000;  // Print progress every N packets
};

/**
 * @brief Statistics from COMTRADE replay test
 */
struct ComtradeReplayStats {
    uint32_t packetsSent = 0;
    uint32_t packetsFailed = 0;
    uint32_t samplesInterpolated = 0;
    int comtradeSampleRate = 0;
    int outputSampleRate = 0;
    int totalComtradeSamples = 0;
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
 * @brief IEC 61850-9-2 COMTRADE Replay Test
 * 
 * This class replays COMTRADE files as IEC 61850-9-2 Sampled Value packets:
 * - Loads IEEE C37.111 COMTRADE files (.cfg + .dat)
 * - Interpolates/resamples data to 4800 Hz (or configured rate)
 * - Maps COMTRADE channels to SV packet channels
 * - Transmits with precise timing using high-precision timer
 * - Monitors network for GOOSE stop messages
 * - Supports looping and time-range playback
 * 
 * Example usage:
 * @code
 * ComtradeReplayTest test;
 * ComtradeReplayConfig config;
 * config.cfgFilePath = "fault.cfg";
 * config.interface = "en0";
 * config.channelMapping = {
 *     {"IA", 0}, {"IB", 1}, {"IC", 2}, {"IN", 3},
 *     {"VA", 4}, {"VB", 5}, {"VC", 6}, {"VN", 7}
 * };
 * 
 * if (test.configure(config)) {
 *     test.run();
 *     auto stats = test.getStatistics();
 *     std::cout << "Replayed " << stats.totalComtradeSamples 
 *               << " samples at " << stats.outputSampleRate << " Hz" << std::endl;
 * }
 * @endcode
 */
class ComtradeReplayTest {
public:
    ComtradeReplayTest();
    ~ComtradeReplayTest();
    
    /**
     * @brief Configure the test with provided parameters
     * @param config Configuration structure
     * @return true on success, false on failure
     */
    bool configure(const ComtradeReplayConfig& config);
    
    /**
     * @brief Run the COMTRADE replay test (blocking)
     * @return true on successful completion, false on error
     */
    bool run();
    
    /**
     * @brief Stop the running test gracefully
     * Thread-safe. Can be called from signal handler.
     */
    void stop();
    
    /**
     * @brief Check if test is currently running
     * @return true if running, false otherwise
     */
    bool isRunning() const;
    
    /**
     * @brief Get test statistics
     * @return Current statistics
     */
    ComtradeReplayStats getStatistics() const;
    
    /**
     * @brief Get last error message
     * @return Error description (empty if no error)
     */
    std::string getLastError() const;
    
    /**
     * @brief Set callback for GOOSE message reception
     * @param callback Function called when GOOSE message received
     */
    void setGooseCallback(std::function<void(const std::string& gocbRef, uint32_t stNum, uint32_t sqNum)> callback);
    
    /**
     * @brief Set callback for progress updates
     * @param callback Function called periodically with progress info
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
    // Internal methods
    void gooseCaptureThreadFunc();
    void transmissionLoop();
    bool loadComtradeFile();
    std::vector<std::vector<double>> resampleData(const std::vector<std::vector<double>>& input, 
                                                    double inputRate, 
                                                    double outputRate);
    double interpolateLinear(const std::vector<double>& data, double index);
    
    // Configuration and state
    ComtradeReplayConfig config_;
    ComtradeReplayStats stats_;
    std::atomic<bool> running_;
    std::string lastError_;
    
    // Threading
    std::thread gooseThread_;
    
    // Callbacks
    std::function<void(const std::string&, uint32_t, uint32_t)> gooseCallback_;
    std::function<void(uint32_t, double)> progressCallback_;
    
    // COMTRADE data (resampled to output rate)
    std::vector<std::vector<int32_t>> resampledData_;  // [channel][sample]
    int numSamples_;
};

#endif // COMTRADE_REPLAY_TEST_H
