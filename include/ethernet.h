#ifndef ETHERNET_H
#define ETHERNET_H

#include <array>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdint>

/**
 * @brief Ethernet frame header (14 bytes)
 * 
 * Format:
 * - Destination MAC (6 bytes)
 * - Source MAC (6 bytes)
 */
class Ethernet {
public:
    std::string macSrc;
    std::string macDst;

    Ethernet(const std::string& dst, const std::string& src) : macSrc(src), macDst(dst) {
        // Validate MAC addresses at construction
        macStrToBytes(macSrc);
        macStrToBytes(macDst);
    }

    /**
     * @brief Convert MAC string "XX:XX:XX:XX:XX:XX" to 6-byte array
     * @param mac MAC address string (format: XX:XX:XX:XX:XX:XX)
     * @return 6-byte array
     */
    std::array<uint8_t, 6> macStrToBytes(const std::string& mac) const {
        std::array<uint8_t, 6> bytes;
        
        // Expected format: "XX:XX:XX:XX:XX:XX" (17 chars)
        if (mac.length() != 17) {
            throw std::invalid_argument("Invalid MAC address format: expected XX:XX:XX:XX:XX:XX");
        }
        
        // Parse each byte pair
        for (size_t i = 0, byteIdx = 0; i < mac.length() && byteIdx < 6; i += 3, ++byteIdx) {
            // Check for colon separator (except after last byte)
            if (byteIdx > 0 && mac[i - 1] != ':') {
                throw std::invalid_argument("Invalid MAC address: missing colon separator");
            }
            
            // Parse two hex digits
            if (i + 1 >= mac.length()) {
                throw std::invalid_argument("Invalid MAC address: truncated byte");
            }
            
            try {
                bytes[byteIdx] = static_cast<uint8_t>(std::stoi(mac.substr(i, 2), nullptr, 16));
            } catch (const std::exception&) {
                throw std::invalid_argument("Invalid MAC address: non-hex digit");
            }
        }
        
        return bytes;
    }

    /**
     * @brief Get encoded Ethernet header (12 bytes: dst + src MAC)
     * @return Ethernet header bytes
     */
    std::vector<uint8_t> getEncoded() const {
        std::vector<uint8_t> encoded;
        encoded.reserve(12);

        auto dstBytes = macStrToBytes(macDst);
        auto srcBytes = macStrToBytes(macSrc);

        encoded.insert(encoded.end(), dstBytes.begin(), dstBytes.end());
        encoded.insert(encoded.end(), srcBytes.begin(), srcBytes.end());

        return encoded;
    }
};

#endif // ETHERNET_H
