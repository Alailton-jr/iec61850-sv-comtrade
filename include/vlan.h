#ifndef VLAN_H
#define VLAN_H

#include <vector>
#include <string>
#include <stdexcept>
#include <cstdint>

/**
 * @brief IEEE 802.1Q VLAN tagging (4 bytes)
 * 
 * Format:
 * - TPID: 0x8100 (2 bytes)
 * - TCI: Priority (3 bits) + DEI (1 bit) + VLAN ID (12 bits) = 2 bytes
 */
class Virtual_LAN {
private:
    uint8_t priority;  // 3 bits: 0-7
    bool DEI;          // Drop Eligible Indicator
    uint16_t ID;       // 12 bits: 0-4095

public:
    Virtual_LAN(uint8_t pri, bool dei, uint16_t id) : priority(pri), DEI(dei), ID(id) {
        // Validate priority (3 bits: 0-7)
        if (priority > 7) {
            throw std::invalid_argument("VLAN priority must be 0-7, got " + std::to_string(priority));
        }
        // Validate VLAN ID (12 bits: 0-4095)
        if (ID > 4095) {
            throw std::invalid_argument("VLAN ID must be 0-4095, got " + std::to_string(ID));
        }
    }

    // Getters
    uint8_t getPriority() const { return priority; }
    bool getDEI() const { return DEI; }
    uint16_t getID() const { return ID; }

    // Setters with validation
    void setPriority(uint8_t pri) {
        if (pri > 7) {
            throw std::invalid_argument("VLAN priority must be 0-7, got " + std::to_string(pri));
        }
        priority = pri;
    }

    void setDEI(bool dei) {
        DEI = dei;
    }

    void setID(uint16_t id) {
        if (id > 4095) {
            throw std::invalid_argument("VLAN ID must be 0-4095, got " + std::to_string(id));
        }
        ID = id;
    }

    /**
     * @brief Get encoded VLAN tag (4 bytes)
     * @return VLAN tag bytes
     */
    std::vector<uint8_t> getEncoded() const {
        std::vector<uint8_t> encoded;
        encoded.reserve(4);

        // TCI = Priority (3 bits) << 13 | DEI (1 bit) << 12 | VLAN ID (12 bits)
        uint16_t tci = static_cast<uint16_t>((static_cast<uint16_t>(priority) << 13) | 
                                              (static_cast<uint16_t>(DEI) << 12) | 
                                              ID);

        // TPID: 0x8100
        encoded.push_back(0x81);
        encoded.push_back(0x00);
        
        // TCI (big-endian)
        encoded.push_back((tci >> 8) & 0xFF);
        encoded.push_back(tci & 0xFF);

        return encoded;
    }
};

#endif // VLAN_H
