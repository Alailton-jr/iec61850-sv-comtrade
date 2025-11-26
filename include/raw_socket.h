#ifndef RAW_SOCKET_H
#define RAW_SOCKET_H

#include <string>
#include <vector>
#include <cstdint>

#ifdef __APPLE__
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/bpf.h>
#include <net/if.h>
#include <fcntl.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if_dl.h>
#include <cstring>
#elif defined(__linux__)
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <cstring>
#endif

/**
 * @brief Cross-platform raw socket for Layer 2 packet injection and capture
 * 
 * macOS: Uses Berkeley Packet Filter (BPF)
 * Linux: Uses AF_PACKET (to be implemented)
 * Windows: Uses Npcap (to be implemented)
 */
class RawSocket {
private:
    int fd_;
    std::string interface_;
    bool isOpen_;
    
#ifdef __APPLE__
    size_t bufferSize_;
    std::vector<uint8_t> readBuffer_;
#elif defined(__linux__)
    int ifindex_;
    struct sockaddr_ll sll_;
    std::vector<uint8_t> readBuffer_;
#endif

public:
    RawSocket() : fd_(-1), isOpen_(false) {
#ifdef __APPLE__
        bufferSize_ = 0;
#elif defined(__linux__)
        ifindex_ = -1;
        std::memset(&sll_, 0, sizeof(sll_));
        readBuffer_.resize(65536);  // Max Ethernet frame size
#endif
    }

    ~RawSocket() {
        close();
    }

    /**
     * @brief Open raw socket on specified network interface
     * @param interface Interface name (e.g., "en0", "eth0")
     * @return true on success, false on failure
     */
    bool open(const std::string& interface) {
        interface_ = interface;
        
#ifdef __APPLE__
        // Open BPF device (macOS)
        for (int i = 0; i < 99; i++) {
            std::string bpfDevice = "/dev/bpf" + std::to_string(i);
            fd_ = ::open(bpfDevice.c_str(), O_RDWR);
            if (fd_ >= 0) break;
        }
        
        if (fd_ < 0) {
            return false;
        }
        
        // Bind to interface
        struct ifreq ifr;
        std::memset(&ifr, 0, sizeof(ifr));
        std::strncpy(ifr.ifr_name, interface.c_str(), sizeof(ifr.ifr_name) - 1);
        
        if (ioctl(fd_, BIOCSETIF, &ifr) < 0) {
            ::close(fd_);
            fd_ = -1;
            return false;
        }
        
        // Enable immediate mode
        unsigned int enable = 1;
        ioctl(fd_, BIOCIMMEDIATE, &enable);
        
        // Get buffer size
        if (ioctl(fd_, BIOCGBLEN, &bufferSize_) < 0) {
            bufferSize_ = 4096;
        }
        readBuffer_.resize(bufferSize_);
        
        // Set write mode (allows sending packets)
        ioctl(fd_, BIOCSHDRCMPLT, &enable);
        
        isOpen_ = true;
        return true;
        
#elif defined(__linux__)
        // Create raw packet socket (ETH_P_ALL to receive all protocols)
        fd_ = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
        if (fd_ < 0) {
            return false;
        }
        
        // Get interface index
        struct ifreq ifr;
        std::memset(&ifr, 0, sizeof(ifr));
        std::strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);
        
        if (ioctl(fd_, SIOCGIFINDEX, &ifr) < 0) {
            ::close(fd_);
            fd_ = -1;
            return false;
        }
        ifindex_ = ifr.ifr_ifindex;
        
        // Prepare sockaddr_ll for sending
        std::memset(&sll_, 0, sizeof(sll_));
        sll_.sll_family = AF_PACKET;
        sll_.sll_ifindex = ifindex_;
        sll_.sll_halen = ETH_ALEN;
        
        // Bind socket to interface
        struct sockaddr_ll bind_addr;
        std::memset(&bind_addr, 0, sizeof(bind_addr));
        bind_addr.sll_family = AF_PACKET;
        bind_addr.sll_protocol = htons(ETH_P_ALL);
        bind_addr.sll_ifindex = ifindex_;
        
        if (bind(fd_, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
            ::close(fd_);
            fd_ = -1;
            return false;
        }
        
        // **Performance optimizations for Linux**
        
        // 1. Set socket to non-blocking mode for async I/O
        int flags = fcntl(fd_, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
        }
        
        // 2. Increase socket buffer sizes for high throughput (4800 Hz = ~307 KB/s for SV)
        int sndbuf = 1048576;  // 1 MB send buffer
        int rcvbuf = 2097152;  // 2 MB receive buffer
        setsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
        setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
        
        // 3. Disable routing lookups for raw sockets (bypass kernel routing)
        int dontroute = 1;
        setsockopt(fd_, SOL_SOCKET, SO_DONTROUTE, &dontroute, sizeof(dontroute));
        
        // 4. Set high priority for time-critical traffic (requires CAP_NET_ADMIN or root)
        int priority = 7;  // Highest priority (0-7)
        setsockopt(fd_, SOL_SOCKET, SO_PRIORITY, &priority, sizeof(priority));
        
        // 5. Enable packet timestamps for precise timing measurements (optional)
        int timestamp = 1;
        setsockopt(fd_, SOL_SOCKET, SO_TIMESTAMP, &timestamp, sizeof(timestamp));
        
        // 6. Set to promiscuous mode to capture all packets on interface
        struct packet_mreq mreq;
        std::memset(&mreq, 0, sizeof(mreq));
        mreq.mr_ifindex = ifindex_;
        mreq.mr_type = PACKET_MR_PROMISC;
        setsockopt(fd_, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
        
        isOpen_ = true;
        return true;
        
#else
        return false;
#endif
    }

    /**
     * @brief Close raw socket
     */
    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
        isOpen_ = false;
    }

    /**
     * @brief Check if socket is open
     */
    bool isOpen() const {
        return isOpen_;
    }

    /**
     * @brief Send raw Ethernet frame
     * @param frame Complete Ethernet frame (dst MAC + src MAC + ethertype/VLAN + payload)
     * @return Number of bytes sent, -1 on error
     */
    ssize_t send(const std::vector<uint8_t>& frame) {
        if (!isOpen_ || fd_ < 0) return -1;
        
#ifdef __APPLE__
        return ::write(fd_, frame.data(), frame.size());
#elif defined(__linux__)
        // Use sendto with pre-configured sockaddr_ll for maximum speed
        return sendto(fd_, frame.data(), frame.size(), 0, 
                     (struct sockaddr*)&sll_, sizeof(sll_));
#else
        return -1;
#endif
    }

    /**
     * @brief Receive raw Ethernet frame (non-blocking)
     * @return Received frame (empty if no data available)
     */
    std::vector<uint8_t> receive() {
        std::vector<uint8_t> frame;
        if (!isOpen_ || fd_ < 0) return frame;
        
#ifdef __APPLE__
        ssize_t bytesRead = ::read(fd_, readBuffer_.data(), readBuffer_.size());
        if (bytesRead > 0) {
            // BPF header is 18 bytes (struct bpf_hdr)
            struct bpf_hdr* bpfHeader = reinterpret_cast<struct bpf_hdr*>(readBuffer_.data());
            size_t packetLen = bpfHeader->bh_caplen;
            size_t offset = bpfHeader->bh_hdrlen;
            
            if (offset + packetLen <= static_cast<size_t>(bytesRead)) {
                frame.assign(readBuffer_.begin() + offset, readBuffer_.begin() + offset + packetLen);
            }
        }
#elif defined(__linux__)
        // Direct read from AF_PACKET socket (no extra headers, just raw Ethernet frame)
        ssize_t bytesRead = recvfrom(fd_, readBuffer_.data(), readBuffer_.size(), 
                                     MSG_DONTWAIT, nullptr, nullptr);
        if (bytesRead > 0) {
            frame.assign(readBuffer_.begin(), readBuffer_.begin() + bytesRead);
        }
#endif
        
        return frame;
    }

    /**
     * @brief Get MAC address of the interface
     * @return MAC address string (XX:XX:XX:XX:XX:XX)
     */
    std::string getMacAddress() const {
#ifdef __APPLE__
        struct ifaddrs* ifap = nullptr;
        if (getifaddrs(&ifap) == 0) {
            for (struct ifaddrs* ifa = ifap; ifa != nullptr; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_LINK) {
                    if (interface_ == ifa->ifa_name) {
                        struct sockaddr_dl* sdl = reinterpret_cast<struct sockaddr_dl*>(ifa->ifa_addr);
                        unsigned char* mac = reinterpret_cast<unsigned char*>(LLADDR(sdl));
                        
                        char macStr[18];
                        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                        
                        freeifaddrs(ifap);
                        return std::string(macStr);
                    }
                }
            }
            freeifaddrs(ifap);
        }
#elif defined(__linux__)
        struct ifreq ifr;
        std::memset(&ifr, 0, sizeof(ifr));
        std::strncpy(ifr.ifr_name, interface_.c_str(), IFNAMSIZ - 1);
        
        if (fd_ >= 0 && ioctl(fd_, SIOCGIFHWADDR, &ifr) == 0) {
            unsigned char* mac = reinterpret_cast<unsigned char*>(ifr.ifr_hwaddr.sa_data);
            
            char macStr[18];
            snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            
            return std::string(macStr);
        }
#endif
        return "00:00:00:00:00:00";
    }
};

#endif // RAW_SOCKET_H
