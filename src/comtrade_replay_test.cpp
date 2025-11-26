#include "comtrade_replay_test.h"
#include "comtrade_parser.h"
#include "ethernet.h"
#include "vlan.h"
#include "sampled_value.h"
#include "goose_decoder.h"
#include "raw_socket.h"
#include "timer.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <time.h>

ComtradeReplayTest::ComtradeReplayTest() 
    : running_(false), numSamples_(0) {
}

ComtradeReplayTest::~ComtradeReplayTest() {
    stop();
}

bool ComtradeReplayTest::configure(const ComtradeReplayConfig& config) {
    if (running_) {
        lastError_ = "Cannot configure while test is running";
        return false;
    }
    
    config_ = config;
    
    // Auto-detect source MAC if not provided
    if (config_.srcMac.empty()) {
        RawSocket tempSocket;
        if (!tempSocket.open(config_.iface)) {
            lastError_ = "Failed to open interface " + config_.iface + " to detect MAC address";
            return false;
        }
        config_.srcMac = tempSocket.getMacAddress();
        tempSocket.close();
        
        if (config_.srcMac == "00:00:00:00:00:00") {
            lastError_ = "Failed to detect MAC address for interface " + config_.iface;
            return false;
        }
    }
    
    // Validate configuration
    if (config_.sampleRate == 0) {
        lastError_ = "Sample rate must be greater than 0";
        return false;
    }
    
    if (config_.iface.empty()) {
        lastError_ = "Interface name cannot be empty";
        return false;
    }
    
    if (config_.cfgFilePath.empty()) {
        lastError_ = "COMTRADE .cfg file path cannot be empty";
        return false;
    }
    
    // Load and process COMTRADE file
    if (!loadComtradeFile()) {
        return false;
    }
    
    return true;
}

bool ComtradeReplayTest::loadComtradeFile() {
    // Parse COMTRADE file
    ComtradeParser parser;
    if (!parser.load(config_.cfgFilePath, config_.datFilePath)) {
        lastError_ = "Failed to load COMTRADE file: " + parser.getLastError();
        return false;
    }
    
    const ComtradeConfig& cfg = parser.getConfig();
    std::vector<ComtradeSample> samples = parser.getAllSamples();
    
    if (samples.empty()) {
        lastError_ = "COMTRADE file contains no samples";
        return false;
    }
    
    // Get original sample rate
    double originalSampleRate = parser.getSampleRate(0);
    stats_.comtradeSampleRate = static_cast<int>(originalSampleRate);
    stats_.totalComtradeSamples = static_cast<int>(samples.size());
    stats_.outputSampleRate = config_.sampleRate;
    
    // Extract analog data for mapped channels
    std::vector<std::vector<double>> analogData(8);  // 8 SV channels
    
    // Initialize with zeros
    for (int i = 0; i < 8; i++) {
        analogData[i].resize(samples.size(), 0.0);
    }
    
    // Map COMTRADE channels to SV channels
    for (const auto& mapping : config_.channelMapping) {
        const std::string& comtradeName = mapping.first;
        int svChannel = mapping.second;
        
        if (svChannel < 0 || svChannel >= 8) {
            lastError_ = "Invalid SV channel index: " + std::to_string(svChannel);
            return false;
        }
        
        // Find COMTRADE channel
        const AnalogChannel* ch = parser.getAnalogChannel(comtradeName);
        if (!ch) {
            lastError_ = "COMTRADE channel not found: " + comtradeName;
            std::cerr << "Available COMTRADE analog channels:" << std::endl;
            for (const auto& availableCh : cfg.analogChannels) {
                std::cerr << "  " << availableCh.name << std::endl;
            }
            return false;
        }
        
        // Extract values
        for (size_t i = 0; i < samples.size(); i++) {
            if (ch->index < static_cast<int>(samples[i].analogValues.size())) {
                analogData[svChannel][i] = samples[i].analogValues[ch->index];
            }
        }
    }
    
    // Resample to target sample rate if needed
    std::vector<std::vector<double>> resampledAnalog;
    if (std::abs(originalSampleRate - config_.sampleRate) > 0.1) {
        if (config_.verboseOutput) {
            std::cout << "Resampling from " << originalSampleRate 
                      << " Hz to " << config_.sampleRate << " Hz..." << std::endl;
        }
        resampledAnalog = resampleData(analogData, originalSampleRate, config_.sampleRate);
        stats_.samplesInterpolated = static_cast<uint32_t>(resampledAnalog[0].size());
    } else {
        resampledAnalog = analogData;
        stats_.samplesInterpolated = static_cast<uint32_t>(analogData[0].size());
    }
    
    // Convert to INT32 format for SV packets
    resampledData_.clear();
    resampledData_.resize(8);
    numSamples_ = static_cast<int>(resampledAnalog[0].size());
    
    for (int ch = 0; ch < 8; ch++) {
        resampledData_[ch].reserve(numSamples_);
        for (int i = 0; i < numSamples_; i++) {
            // Convert to INT32 (already in engineering units from COMTRADE)
            resampledData_[ch].push_back(static_cast<int32_t>(resampledAnalog[ch][i]));
        }
    }
    
    if (config_.verboseOutput) {
        std::cout << "Loaded COMTRADE file:" << std::endl;
        std::cout << "  Station: " << cfg.stationName << std::endl;
        std::cout << "  Original samples: " << stats_.totalComtradeSamples 
                  << " @ " << stats_.comtradeSampleRate << " Hz" << std::endl;
        std::cout << "  Resampled: " << numSamples_ 
                  << " @ " << stats_.outputSampleRate << " Hz" << std::endl;
        std::cout << "  Mapped channels: " << config_.channelMapping.size() << std::endl;
    }
    
    return true;
}

std::vector<std::vector<double>> ComtradeReplayTest::resampleData(
    const std::vector<std::vector<double>>& input,
    double inputRate,
    double outputRate) {
    
    if (input.empty() || input[0].empty()) {
        return input;
    }
    
    int inputSamples = static_cast<int>(input[0].size());
    double ratio = outputRate / inputRate;
    int outputSamples = static_cast<int>(std::ceil(inputSamples * ratio));
    
    std::vector<std::vector<double>> output(input.size());
    
    for (size_t ch = 0; ch < input.size(); ch++) {
        output[ch].reserve(outputSamples);
        
        for (int i = 0; i < outputSamples; i++) {
            // Calculate corresponding input index
            double inputIndex = i / ratio;
            
            // Linear interpolation
            double value = interpolateLinear(input[ch], inputIndex);
            output[ch].push_back(value);
        }
    }
    
    return output;
}

double ComtradeReplayTest::interpolateLinear(const std::vector<double>& data, double index) {
    if (data.empty()) {
        return 0.0;
    }
    
    if (index <= 0.0) {
        return data[0];
    }
    
    if (index >= data.size() - 1) {
        return data.back();
    }
    
    // Linear interpolation between floor and ceil
    int i0 = static_cast<int>(std::floor(index));
    int i1 = i0 + 1;
    double frac = index - i0;
    
    return data[i0] * (1.0 - frac) + data[i1] * frac;
}

bool ComtradeReplayTest::run() {
    if (running_) {
        lastError_ = "Test is already running";
        return false;
    }
    
    if (config_.iface.empty() || numSamples_ == 0) {
        lastError_ = "Test not configured. Call configure() first";
        return false;
    }
    
    // Reset statistics
    stats_.packetsSent = 0;
    stats_.packetsFailed = 0;
    stats_.stoppedByGoose = false;
    stats_.gooseStopReason.clear();
    stats_.startTime = std::chrono::steady_clock::now();
    
    // Start GOOSE monitoring thread if enabled
    running_ = true;
    if (config_.enableGooseMonitoring) {
        gooseThread_ = std::thread(&ComtradeReplayTest::gooseCaptureThreadFunc, this);
    }
    
    // Print configuration
    if (config_.verboseOutput) {
        printConfiguration();
    }
    
    // Start transmission
    transmissionLoop();
    
    // Wait for GOOSE thread to finish
    if (gooseThread_.joinable()) {
        gooseThread_.join();
    }
    
    stats_.endTime = std::chrono::steady_clock::now();
    
    // Print statistics
    if (config_.verboseOutput) {
        printStatistics();
    }
    
    return true;
}

void ComtradeReplayTest::stop() {
    running_ = false;
    if (gooseThread_.joinable()) {
        gooseThread_.join();
    }
}

bool ComtradeReplayTest::isRunning() const {
    return running_;
}

ComtradeReplayStats ComtradeReplayTest::getStatistics() const {
    return stats_;
}

std::string ComtradeReplayTest::getLastError() const {
    return lastError_;
}

void ComtradeReplayTest::setGooseCallback(
    std::function<void(const std::string&, uint32_t, uint32_t)> callback) {
    gooseCallback_ = callback;
}

void ComtradeReplayTest::setProgressCallback(
    std::function<void(uint32_t, double)> callback) {
    progressCallback_ = callback;
}

void ComtradeReplayTest::gooseCaptureThreadFunc() {
    RawSocket socket;
    if (!socket.open(config_.iface)) {
        if (config_.verboseOutput) {
            std::cerr << "Warning: Failed to open socket for GOOSE monitoring" << std::endl;
        }
        return;
    }
    
    if (config_.verboseOutput) {
        std::cout << "GOOSE monitoring started" << std::endl;
    }
    
    while (running_) {
        std::vector<uint8_t> frame = socket.receive();
        
        if (!frame.empty()) {
            size_t len = frame.size();
            
            // Check if this is a GOOSE frame (EtherType 0x88B8)
            if (len >= 16 && frame[12] == 0x88 && frame[13] == 0xB8) {
                // Decode GOOSE
                GooseMessage msg = decodeGoose(frame);
                
                if (msg.valid) {
                    if (config_.verboseOutput) {
                        std::cout << "\n[GOOSE Received]" << std::endl;
                        std::cout << "  AppID: 0x" << std::hex << msg.appID << std::dec << std::endl;
                        std::cout << "  gocbRef: " << msg.gocbRef << std::endl;
                        std::cout << "  stNum: " << msg.stNum << std::endl;
                        std::cout << "  sqNum: " << msg.sqNum << std::endl;
                    }
                    
                    if (gooseCallback_) {
                        gooseCallback_(msg.gocbRef, msg.stNum, msg.sqNum);
                    }
                    
                    if (msg.gocbRef.find(config_.stopGooseRef) != std::string::npos) {
                        if (config_.verboseOutput) {
                            std::cout << "\n*** Stop GOOSE detected! Stopping test... ***\n" << std::endl;
                        }
                        stats_.stoppedByGoose = true;
                        stats_.gooseStopReason = msg.gocbRef;
                        running_ = false;
                        break;
                    }
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    socket.close();
    
    if (config_.verboseOutput) {
        std::cout << "GOOSE capture stopped" << std::endl;
    }
}

void ComtradeReplayTest::transmissionLoop() {
    // Open raw socket
    RawSocket socket;
    if (!socket.open(config_.iface)) {
        lastError_ = "Failed to open raw socket on " + config_.iface;
        std::cerr << "Error: " << lastError_ << std::endl;
        std::cerr << "Note: This program requires root privileges (sudo)" << std::endl;
        running_ = false;
        return;
    }
    
    // Build Ethernet and VLAN headers
    Ethernet eth(config_.dstMac, config_.srcMac);
    Virtual_LAN vlan(config_.vlanPriority, false, config_.vlanId);
    
    auto ethHeader = eth.getEncoded();
    auto vlanTag = vlan.getEncoded();
    
    // Create SampledValue builder
    SampledValue sv(config_.appId, config_.svId, config_.sampleRate);
    
    if (config_.verboseOutput) {
        std::cout << "Starting COMTRADE replay... (Press Ctrl+C to stop";
        if (config_.enableGooseMonitoring) {
            std::cout << " or wait for GOOSE";
        }
        std::cout << ")" << std::endl << std::endl;
    }
    
    // High-precision timer setup
    Timer timer;
    struct timespec t_ini, t_start;
    
    // Calculate wait period in nanoseconds
    long waitPeriod = static_cast<long>(1e9 / config_.sampleRate);
    
    // Align to next second boundary
    clock_gettime(CLOCK_MONOTONIC, &t_ini);
    if (t_ini.tv_nsec > static_cast<long>(5e8)) {
        t_ini.tv_sec += 2;
    } else {
        t_ini.tv_sec += 1;
    }
    t_ini.tv_nsec = 0;
    
    // Start timer
#ifdef _WIN32
    // On Windows, start from current time (no timespec)
    timer.start_period(waitPeriod);
#else
    // On Unix, use timespec for precise alignment
    timer.start_period(t_ini);
#endif
    timer.wait_period(waitPeriod);
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    
    // Transmission loop
    int sampleIdx = 0;
    
    do {
        // Build current sample phasors from resampled data
        double phasors[8][2];
        for (int ch = 0; ch < 8; ch++) {
            // Use INT32 value directly (already scaled in engineering units)
            phasors[ch][0] = static_cast<double>(resampledData_[ch][sampleIdx]);
            phasors[ch][1] = 0.0;  // Phase angle not used for direct values
        }
        
        // Build SV packet
        auto svPayload = sv.buildPacket(phasors);
        
        // Build complete frame
        std::vector<uint8_t> frame;
        frame.reserve(ethHeader.size() + vlanTag.size() + svPayload.size());
        frame.insert(frame.end(), ethHeader.begin(), ethHeader.end());
        frame.insert(frame.end(), vlanTag.begin(), vlanTag.end());
        frame.insert(frame.end(), svPayload.begin(), svPayload.end());
        
        // Send frame
        ssize_t sent = socket.send(frame);
        
        if (sent > 0) {
            stats_.packetsSent++;
            
            // Print progress
            if (config_.verboseOutput && 
                config_.progressInterval > 0 && 
                stats_.packetsSent % config_.progressInterval == 0) {
                
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
                    now - stats_.startTime).count();
                
                std::cout << "Sent " << stats_.packetsSent << " packets in " 
                          << std::fixed << std::setprecision(1) << elapsed << "s "
                          << "(sample " << sampleIdx << "/" << numSamples_ << ", "
                          << "smpCnt: " << sv.smpCnt << ")" << std::endl;
                
                if (progressCallback_) {
                    progressCallback_(stats_.packetsSent, elapsed);
                }
            }
        } else {
            stats_.packetsFailed++;
            if (config_.verboseOutput && stats_.packetsFailed % 100 == 1) {
                std::cerr << "Warning: Failed to send packet (total failures: " 
                          << stats_.packetsFailed << ")" << std::endl;
            }
        }
        
        // Increment sample count
        sv.incrementSampleCount();
        sampleIdx++;
        
        // Check if we've reached the end
        if (sampleIdx >= numSamples_) {
            if (config_.loopPlayback) {
                sampleIdx = 0;  // Loop back to start
            } else {
                break;  // End of playback
            }
        }
        
        // Wait for next period
        timer.wait_period(waitPeriod);
        
    } while (running_);
    
    socket.close();
    
    if (config_.verboseOutput) {
        std::cout << "\nStopping transmission..." << std::endl;
    }
}

void ComtradeReplayTest::printConfiguration() const {
    std::cout << "\n=== COMTRADE Replay Configuration ===" << std::endl;
    std::cout << "COMTRADE file: " << config_.cfgFilePath << std::endl;
    std::cout << "Network interface: " << config_.iface << std::endl;
    std::cout << "Source MAC: " << config_.srcMac << std::endl;
    std::cout << "Destination MAC: " << config_.dstMac << std::endl;
    std::cout << "VLAN: ID=" << config_.vlanId 
              << ", Priority=" << static_cast<int>(config_.vlanPriority) << std::endl;
    std::cout << "SV: AppID=0x" << std::hex << config_.appId << std::dec
              << ", svID=" << config_.svId 
              << ", Rate=" << config_.sampleRate << " Hz" << std::endl;
    std::cout << "Channel mappings:" << std::endl;
    for (const auto& mapping : config_.channelMapping) {
        std::cout << "  " << mapping.first << " -> SV[" << mapping.second << "]" << std::endl;
    }
    std::cout << "Loop playback: " << (config_.loopPlayback ? "Yes" : "No") << std::endl;
    if (config_.enableGooseMonitoring) {
        std::cout << "GOOSE stop trigger: " << config_.stopGooseRef << std::endl;
    }
    std::cout << std::endl;
}

void ComtradeReplayTest::printStatistics() const {
    std::cout << "\n=== Replay Statistics ===" << std::endl;
    std::cout << "Original COMTRADE: " << stats_.totalComtradeSamples 
              << " samples @ " << stats_.comtradeSampleRate << " Hz" << std::endl;
    std::cout << "Resampled to: " << stats_.samplesInterpolated 
              << " samples @ " << stats_.outputSampleRate << " Hz" << std::endl;
    std::cout << "Packets sent: " << stats_.packetsSent << std::endl;
    std::cout << "Packets failed: " << stats_.packetsFailed << std::endl;
    std::cout << "Elapsed time: " << std::fixed << std::setprecision(3) 
              << stats_.getElapsedSeconds() << " seconds" << std::endl;
    std::cout << "Average rate: " << std::fixed << std::setprecision(1) 
              << stats_.getAverageRate() << " packets/sec" << std::endl;
    
    if (stats_.stoppedByGoose) {
        std::cout << "Stopped by GOOSE: " << stats_.gooseStopReason << std::endl;
    }
    std::cout << std::endl;
}
