#ifndef RAW_SOCKET_H
#define RAW_SOCKET_H

#include <string>
#include <vector>
#include <cstdint>

#ifdef _WIN32
    // Windows with Npcap
    #define HAVE_REMOTE
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    #include <pcap.h>
    #pragma comment(lib, "wpcap.lib")
    #pragma comment(lib, "iphlpapi.lib")
    #pragma comment(lib, "ws2_32.lib")
#elif defined(__APPLE__)
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
 * Linux: Uses AF_PACKET
 * Windows: Uses Npcap (WinPcap successor)
 */
class RawSocket {
private:
#ifdef _WIN32
    pcap_t* pcap_handle_;
    std::string interface_;
    bool isOpen_;
    std::vector<uint8_t> readBuffer_;
#else
    int fd_;
    std::string interface_;
    bool isOpen_;
#endif
    
#ifdef __APPLE__
    size_t bufferSize_;
    std::vector<uint8_t> readBuffer_;
#elif defined(__linux__)
    int ifindex_;
    struct sockaddr_ll sll_;
    std::vector<uint8_t> readBuffer_;
#endif

public:
#ifdef _WIN32
    RawSocket() : pcap_handle_(nullptr), isOpen_(false) {
        readBuffer_.resize(65536);
    }
#else
    RawSocket() : fd_(-1), isOpen_(false) {
#ifdef __APPLE__
        bufferSize_ = 0;
#elif defined(__linux__)
        ifindex_ = -1;
        std::memset(&sll_, 0, sizeof(sll_));
        readBuffer_.resize(65536);  // Max Ethernet frame size
#endif
    }
#endif

    ~RawSocket() {
        close();
    }

    /**
     * @brief Open raw socket on specified network interface
     * @param interface Interface name (e.g., "en0", "eth0", "Ethernet" on Windows)
     * @return true on success, false on failure
     */
    bool open(const std::string& iface) {
        interface_ = iface;
        
#ifdef _WIN32
        // Windows with Npcap
        char errbuf[PCAP_ERRBUF_SIZE];
        
        // Convert interface name to device name
        // On Windows, if interface doesn't start with "\\Device\\NPF_", prepend it
        std::string device_name;
        if (iface.find("\\Device\\NPF_") == 0) {
            device_name = iface;
        } else {
            // Try to find the adapter by friendly name
            pcap_if_t* alldevs;
            if (pcap_findalldevs(&alldevs, errbuf) == -1) {
                return false;
            }
            
            bool found = false;
            for (pcap_if_t* d = alldevs; d != nullptr; d = d->next) {
                if (d->description && iface == d->description) {
                    device_name = d->name;
                    found = true;
                    break;
                }
                // Also try direct name match
                std::string name_only = d->name;
                if (name_only.find("NPF_") != std::string::npos) {
                    size_t pos = name_only.find("NPF_") + 4;
                    name_only = name_only.substr(pos);
                    if (name_only == iface) {
                        device_name = d->name;
                        found = true;
                        break;
                    }
                }
            }
            
            if (!found && alldevs) {
                // If not found by name, use first available adapter
                device_name = alldevs->name;
            }
            
            pcap_freealldevs(alldevs);
            
            if (device_name.empty()) {
                return false;
            }
        }
        
        // Open the adapter in promiscuous mode
        pcap_handle_ = pcap_open_live(
            device_name.c_str(),
            65536,          // snaplen
            1,              // promiscuous mode
            10,             // read timeout (ms)
            errbuf
        );
        
        if (pcap_handle_ == nullptr) {
            return false;
        }
        
        // Set to non-blocking mode
        if (pcap_setnonblock(pcap_handle_, 1, errbuf) == -1) {
            pcap_close(pcap_handle_);
            pcap_handle_ = nullptr;
            return false;
        }
        
        isOpen_ = true;
        return true;
        
#elif defined(__APPLE__)
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
        
        // return true;    

        isOpen_ = true;

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
        
        return true;
        
#else
        return false;
#endif
    }

    /**
     * @brief Close raw socket
     */
    void close() {
#ifdef _WIN32
        if (pcap_handle_) {
            pcap_close(pcap_handle_);
            pcap_handle_ = nullptr;
        }
#else
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
#endif
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
#ifdef _WIN32
        if (!isOpen_ || !pcap_handle_) return -1;
        
        int result = pcap_sendpacket(pcap_handle_, frame.data(), static_cast<int>(frame.size()));
        return (result == 0) ? static_cast<ssize_t>(frame.size()) : -1;
#else
        if (!isOpen_ || fd_ < 0) return -1;
#endif
        
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
#ifdef _WIN32
        if (!isOpen_ || !pcap_handle_) return frame;
        
        struct pcap_pkthdr* header;
        const u_char* pkt_data;
        
        int result = pcap_next_ex(pcap_handle_, &header, &pkt_data);
        if (result == 1) {
            // Packet captured successfully
            frame.assign(pkt_data, pkt_data + header->caplen);
        }
        // result == 0: timeout (no packet)
        // result == -1: error
        // result == -2: EOF (offline capture)
        
        return frame;
#else
        if (!isOpen_ || fd_ < 0) return frame;
#endif
        
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
#ifdef _WIN32
        // Windows: Use GetAdaptersAddresses
        ULONG bufferSize = 15000;
        PIP_ADAPTER_ADDRESSES pAddresses = nullptr;
        ULONG result;
        
        // Allocate buffer
        pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(bufferSize);
        if (!pAddresses) {
            return "00:00:00:00:00:00";
        }
        
        // Get adapters
        result = GetAdaptersAddresses(AF_UNSPEC, 
                                     GAA_FLAG_INCLUDE_PREFIX, 
                                     nullptr, 
                                     pAddresses, 
                                     &bufferSize);
        
        if (result == ERROR_BUFFER_OVERFLOW) {
            free(pAddresses);
            pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(bufferSize);
            if (!pAddresses) {
                return "00:00:00:00:00:00";
            }
            result = GetAdaptersAddresses(AF_UNSPEC, 
                                         GAA_FLAG_INCLUDE_PREFIX, 
                                         nullptr, 
                                         pAddresses, 
                                         &bufferSize);
        }
        
        std::string macStr = "00:00:00:00:00:00";
        
        if (result == NO_ERROR) {
            PIP_ADAPTER_ADDRESSES pCurr = pAddresses;
            while (pCurr) {
                // Check if this is our interface (by matching friendly name or adapter name)
                if (pCurr->FriendlyName) {
                    std::wstring wName(pCurr->FriendlyName);
                    std::string adapterName(wName.begin(), wName.end());
                    
                    if (adapterName.find(interface_) != std::string::npos ||
                        interface_.find(adapterName) != std::string::npos) {
                        
                        if (pCurr->PhysicalAddressLength == 6) {
                            char mac[18];
                            snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                                    pCurr->PhysicalAddress[0], pCurr->PhysicalAddress[1],
                                    pCurr->PhysicalAddress[2], pCurr->PhysicalAddress[3],
                                    pCurr->PhysicalAddress[4], pCurr->PhysicalAddress[5]);
                            macStr = std::string(mac);
                            break;
                        }
                    }
                }
                pCurr = pCurr->Next;
            }
        }
        
        free(pAddresses);
        return macStr;
        
#elif defined(__APPLE__)
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
