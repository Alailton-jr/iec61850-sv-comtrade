#include "phasor_injection_test.h"
#include "ethernet.h"
#include "vlan.h"
#include "sampled_value.h"
#include "goose_decoder.h"
#include "raw_socket.h"
#include "timer.h"
#include <iostream>
#include <iomanip>
#include <time.h>

PhasorInjectionTest::PhasorInjectionTest() : running_(false) {
}

PhasorInjectionTest::~PhasorInjectionTest() {
    stop();
}

bool PhasorInjectionTest::configure(const PhasorInjectionConfig& config) {
    if (running_) {
        lastError_ = "Cannot configure while test is running";
        return false;
    }
    
    config_ = config;
    
    // Auto-detect source MAC if not provided
    if (config_.srcMac.empty()) {
        RawSocket tempSocket;
        if (!tempSocket.open(config_.interface)) {
            lastError_ = "Failed to open interface " + config_.interface + " to detect MAC address";
            return false;
        }
        config_.srcMac = tempSocket.getMacAddress();
        tempSocket.close();
        
        if (config_.srcMac == "00:00:00:00:00:00") {
            lastError_ = "Failed to detect MAC address for interface " + config_.interface;
            return false;
        }
    }
    
    // Validate configuration
    if (config_.sampleRate == 0) {
        lastError_ = "Sample rate must be greater than 0";
        return false;
    }
    
    if (config_.interface.empty()) {
        lastError_ = "Interface name cannot be empty";
        return false;
    }
    
    return true;
}

bool PhasorInjectionTest::run() {
    if (running_) {
        lastError_ = "Test is already running";
        return false;
    }
    
    if (config_.interface.empty()) {
        lastError_ = "Test not configured. Call configure() first";
        return false;
    }
    
    // Reset statistics
    stats_ = PhasorInjectionStats();
    stats_.startTime = std::chrono::steady_clock::now();
    
    // Start GOOSE monitoring thread if enabled
    running_ = true;
    if (config_.enableGooseMonitoring) {
        gooseThread_ = std::thread(&PhasorInjectionTest::gooseCaptureThreadFunc, this);
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

void PhasorInjectionTest::stop() {
    running_ = false;
    
    if (gooseThread_.joinable()) {
        gooseThread_.join();
    }
}

bool PhasorInjectionTest::isRunning() const {
    return running_;
}

PhasorInjectionStats PhasorInjectionTest::getStatistics() const {
    return stats_;
}

std::string PhasorInjectionTest::getLastError() const {
    return lastError_;
}

void PhasorInjectionTest::setGooseCallback(std::function<void(const std::string&, uint32_t, uint32_t)> callback) {
    gooseCallback_ = callback;
}

void PhasorInjectionTest::setProgressCallback(std::function<void(uint32_t, double)> callback) {
    progressCallback_ = callback;
}

void PhasorInjectionTest::printConfiguration() const {
    std::cout << "\n=== IEC 61850 Sampled Value Injection Test ===" << std::endl;
    std::cout << "\nConfiguration:" << std::endl;
    std::cout << "  Interface: " << config_.interface << std::endl;
    std::cout << "  Source MAC: " << config_.srcMac << std::endl;
    std::cout << "  Destination MAC: " << config_.dstMac << std::endl;
    std::cout << "  VLAN ID: " << config_.vlanId 
              << " (Priority: " << static_cast<int>(config_.vlanPriority) << ")" << std::endl;
    std::cout << "  APPID: 0x" << std::hex << config_.appId << std::dec << std::endl;
    std::cout << "  SV ID: " << config_.svId << std::endl;
    std::cout << "  Sample Rate: " << config_.sampleRate << " samples/sec" << std::endl;
    
    if (config_.enableGooseMonitoring) {
        std::cout << "  GOOSE Stop: Enabled (monitoring for '" << config_.stopGooseRef << "')" << std::endl;
    }
    
    std::cout << "\nPhasor Values:" << std::endl;
    const char* labels[] = {"IA", "IB", "IC", "IN", "VA", "VB", "VC", "VN"};
    for (int i = 0; i < 8; i++) {
        std::cout << "  " << labels[i] << ": " 
                  << std::fixed << std::setprecision(2)
                  << config_.phasors[i][0] << " ∠ " 
                  << config_.phasors[i][1] << "°" << std::endl;
    }
    std::cout << std::endl;
}

void PhasorInjectionTest::printStatistics() const {
    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Total packets sent: " << stats_.packetsSent << std::endl;
    std::cout << "Total packets failed: " << stats_.packetsFailed << std::endl;
    std::cout << "Total time: " << std::fixed << std::setprecision(3) 
              << stats_.getElapsedSeconds() << " seconds" << std::endl;
    
    if (stats_.getElapsedSeconds() > 0) {
        std::cout << "Average rate: " << std::fixed << std::setprecision(2)
                  << stats_.getAverageRate() << " packets/sec" << std::endl;
    }
    
    if (stats_.stoppedByGoose) {
        std::cout << "Stopped by GOOSE: " << stats_.gooseStopReason << std::endl;
    }
    std::cout << std::endl;
}

void PhasorInjectionTest::gooseCaptureThreadFunc() {
    RawSocket socket;
    
    if (!socket.open(config_.interface)) {
        std::cerr << "Failed to open socket for GOOSE capture on " << config_.interface << std::endl;
        return;
    }
    
    if (config_.verboseOutput) {
        std::cout << "GOOSE capture started on " << config_.interface << std::endl;
        std::cout << "Waiting for GOOSE with gocbRef containing: " << config_.stopGooseRef << std::endl;
    }
    
    while (running_) {
        std::vector<uint8_t> frame = socket.receive();
        
        if (!frame.empty()) {
            // Check if it's GOOSE (EtherType 0x88B8)
            if (frame.size() > 16) {
                size_t ethTypeOffset = 12;
                // Check for VLAN
                if (frame[12] == 0x81 && frame[13] == 0x00) {
                    ethTypeOffset = 16;
                }
                
                if (ethTypeOffset + 2 <= frame.size() &&
                    frame[ethTypeOffset] == 0x88 && frame[ethTypeOffset + 1] == 0xB8) {
                    
                    GooseMessage msg = decodeGoose(frame);
                    
                    if (msg.valid) {
                        if (config_.verboseOutput) {
                            std::cout << "\n[GOOSE Received]" << std::endl;
                            std::cout << "  AppID: 0x" << std::hex << msg.appID << std::dec << std::endl;
                            std::cout << "  gocbRef: " << msg.gocbRef << std::endl;
                            std::cout << "  datSet: " << msg.datSet << std::endl;
                            std::cout << "  stNum: " << msg.stNum << std::endl;
                            std::cout << "  sqNum: " << msg.sqNum << std::endl;
                        }
                        
                        // Call user callback if set
                        if (gooseCallback_) {
                            gooseCallback_(msg.gocbRef, msg.stNum, msg.sqNum);
                        }
                        
                        // Check stop condition
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
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    socket.close();
    
    if (config_.verboseOutput) {
        std::cout << "GOOSE capture stopped" << std::endl;
    }
}

void PhasorInjectionTest::transmissionLoop() {
    // Open raw socket
    RawSocket socket;
    if (!socket.open(config_.interface)) {
        lastError_ = "Failed to open raw socket on " + config_.interface;
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
        std::cout << "Starting SV transmission... (Press Ctrl+C to stop";
        if (config_.enableGooseMonitoring) {
            std::cout << " or wait for GOOSE";
        }
        std::cout << ")" << std::endl << std::endl;
    }
    
    // High-precision timer setup (following transient.cpp approach)
    Timer timer;
    struct timespec t_ini, t_start, now_realtime;
    
    // Calculate wait period in nanoseconds for sample rate
    long waitPeriod = static_cast<long>(1e9 / config_.sampleRate);
    
    // Get current REALTIME to align to next full second boundary (e.g., 20.000000)
    clock_gettime(CLOCK_REALTIME, &now_realtime);
    
    // Wait until the next full second boundary
    struct timespec next_second;
    next_second.tv_sec = now_realtime.tv_sec + 1;
    next_second.tv_nsec = 0;
    
    if (config_.verboseOutput) {
        std::cout << "Current time: " << now_realtime.tv_sec << "." 
                  << std::setfill('0') << std::setw(9) << now_realtime.tv_nsec << std::endl;
        std::cout << "Waiting until: " << next_second.tv_sec << ".000000000" << std::endl;
    }
    
    // Sleep until next second boundary (platform-specific)
#ifdef __linux__
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_second, nullptr);
#else
    // macOS: Calculate relative sleep time
    struct timespec sleep_duration;
    sleep_duration.tv_sec = 0;
    sleep_duration.tv_nsec = 1000000000L - now_realtime.tv_nsec;
    if (sleep_duration.tv_nsec >= 1000000000L) {
        sleep_duration.tv_sec++;
        sleep_duration.tv_nsec -= 1000000000L;
    }
    nanosleep(&sleep_duration, nullptr);
#endif
    
    // Now get MONOTONIC time for the periodic timer (maintains relative precision)
    clock_gettime(CLOCK_MONOTONIC, &t_ini);
    
    // Pre-build initial frame outside the loop
    auto svPayload = sv.buildPacket(config_.phasors);
    std::vector<uint8_t> frame;
    frame.reserve(ethHeader.size() + vlanTag.size() + svPayload.size());
    frame.insert(frame.end(), ethHeader.begin(), ethHeader.end());
    frame.insert(frame.end(), vlanTag.begin(), vlanTag.end());
    frame.insert(frame.end(), svPayload.begin(), svPayload.end());
    
    // Start timer at aligned time
    timer.start_period(t_ini);
    timer.wait_period(waitPeriod);
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    
    // High-precision transmission loop
    while (running_) {
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
                          << "(smpCnt: " << sv.smpCnt << ")" << std::endl;
                
                // Call progress callback if set
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
        
        // Increment sample count and rebuild frame for next iteration
        sv.incrementSampleCount();
        
        // Rebuild SV payload with new sample count
        svPayload = sv.buildPacket(config_.phasors);
        
        // Update frame with new payload (reuse Ethernet+VLAN headers)
        frame.resize(ethHeader.size() + vlanTag.size());
        frame.insert(frame.end(), svPayload.begin(), svPayload.end());
        
        // Wait for next period with high-precision absolute timer
        timer.wait_period(waitPeriod);
    }
    
    socket.close();
    
    if (config_.verboseOutput) {
        std::cout << "\nStopping transmission..." << std::endl;
    }
}
