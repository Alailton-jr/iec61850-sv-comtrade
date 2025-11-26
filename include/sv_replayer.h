#ifndef SV_REPLAYER_H
#define SV_REPLAYER_H

#include "comtrade_parser.h"
#include <string>
#include <vector>
#include <cstdint>

/**
 * @brief Configuration for SV stream
 */
struct SVStreamConfig {
    std::string srcMAC;         // Source MAC address (format: "01:0C:CD:01:00:00")
    std::string dstMAC;         // Destination MAC address
    uint16_t appID;             // Application ID
    uint16_t vlanID;            // VLAN ID
    uint8_t vlanPriority;       // VLAN Priority (0-7)
    std::string svID;           // SV ID string
    uint32_t confRev;           // Configuration revision
    uint8_t smpSynch;           // Sample synchronization (0=none, 1=local, 2=global)
    std::string interface;      // Network interface name (e.g., "eth0")
};

/**
 * @brief Replays COMTRADE samples as IEC 61850-9-2 Sampled Value packets
 * 
 * This class reads COMTRADE files and transmits the samples over the network
 * as properly formatted SV packets with accurate timing.
 */
class SVReplayer {
public:
    SVReplayer();
    ~SVReplayer();
    
    /**
     * @brief Load COMTRADE file for replay
     * @param comtradePath Path to .cfg file
     * @return true if loaded successfully
     */
    bool loadComtrade(const std::string& comtradePath);
    
    /**
     * @brief Configure SV stream parameters
     * @param config Stream configuration
     */
    void configure(const SVStreamConfig& config);
    
    /**
     * @brief Start replaying samples
     * @param loopCount Number of times to loop (0 = infinite)
     * @return true if started successfully
     */
    bool start(int loopCount = 1);
    
    /**
     * @brief Stop replay
     */
    void stop();
    
    /**
     * @brief Check if currently replaying
     * @return true if replaying
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Get current sample index
     * @return Current sample number
     */
    int getCurrentSample() const { return currentSample_; }
    
    /**
     * @brief Get total samples
     * @return Total number of samples
     */
    int getTotalSamples() const;

private:
    void replayLoop();
    bool initRawSocket();
    void closeRawSocket();
    bool sendSVPacket(const ComtradeSample& sample, uint16_t smpCnt);
    
    ComtradeParser parser_;
    SVStreamConfig config_;
    bool running_;
    int currentSample_;
    int loopCount_;
    int socketFd_;
};

#endif // SV_REPLAYER_H
