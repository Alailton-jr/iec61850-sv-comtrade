#ifndef GOOSE_LISTENER_H
#define GOOSE_LISTENER_H

#include <string>
#include <functional>
#include <cstdint>
#include <vector>

/**
 * @brief GOOSE message data
 */
struct GooseMessage {
    std::string srcMAC;         // Source MAC address
    std::string goCBRef;        // Control block reference
    uint32_t stNum;             // State number
    uint32_t sqNum;             // Sequence number
    bool test;                  // Test flag
    uint32_t confRev;           // Configuration revision
    bool ndsCom;                // Needs commissioning
    uint32_t timeAllowedToLive; // Time allowed to live (ms)
    std::vector<bool> dataSet;  // Boolean data points
};

/**
 * @brief Callback function type for GOOSE messages
 */
using GooseCallback = std::function<void(const GooseMessage&)>;

/**
 * @brief Listens for IEC 61850 GOOSE messages on the network
 * 
 * This class captures GOOSE messages and can trigger callbacks or
 * stop signals based on message content.
 */
class GooseListener {
public:
    GooseListener();
    ~GooseListener();
    
    /**
     * @brief Start listening for GOOSE messages
     * @param interface Network interface name (e.g., "eth0")
     * @return true if started successfully
     */
    bool start(const std::string& interface);
    
    /**
     * @brief Stop listening
     */
    void stop();
    
    /**
     * @brief Check if currently listening
     * @return true if listening
     */
    bool isListening() const { return listening_; }
    
    /**
     * @brief Set callback for GOOSE message reception
     * @param callback Function to call when GOOSE message received
     */
    void setCallback(GooseCallback callback);
    
    /**
     * @brief Configure to stop on specific GOOSE condition
     * @param goCBRef Control block reference to monitor
     * @param dataIndex Data index to monitor (0-based)
     * @param triggerValue Value that triggers stop (true/false)
     */
    void setStopCondition(const std::string& goCBRef, int dataIndex, bool triggerValue);
    
    /**
     * @brief Check if stop condition was triggered
     * @return true if condition met
     */
    bool isStopTriggered() const { return stopTriggered_; }

private:
    void captureLoop();
    bool initRawSocket(const std::string& interface);
    void closeRawSocket();
    bool parseGoosePacket(const uint8_t* packet, size_t length, GooseMessage& msg);
    
    int socketFd_;
    bool listening_;
    GooseCallback callback_;
    
    // Stop condition
    std::string stopGoCBRef_;
    int stopDataIndex_;
    bool stopTriggerValue_;
    bool stopTriggered_;
};

#endif // GOOSE_LISTENER_H
