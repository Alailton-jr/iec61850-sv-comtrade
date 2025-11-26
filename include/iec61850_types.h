#ifndef IEC61850_TYPES_H
#define IEC61850_TYPES_H

#include <vector>
#include <cstdint>
#include <cstring>

/**
 * @brief IEC 61850-8-1 UtcTime type (8 bytes)
 * 
 * Format:
 * - Seconds since epoch (4 bytes)
 * - Fraction of second (4 bytes, resolution: 2^-32 seconds)
 */
class UtcTime {
public:
    uint32_t seconds;
    uint32_t fraction;
    uint8_t defined;

    UtcTime() : seconds(0), fraction(0), defined(0) {}

    UtcTime(uint32_t sec, uint32_t frac) : seconds(sec), defined(1) {
        // Convert nanoseconds to IEC 61850 fraction format (2^-32 seconds)
        this->fraction = static_cast<uint32_t>((static_cast<uint64_t>(frac) * (1ULL << 32)) / 1000000000ULL);
    }

    std::vector<uint8_t> getEncoded() const {
        std::vector<uint8_t> encoded(8);
        
        // Seconds (big-endian)
        encoded[0] = (seconds >> 24) & 0xFF;
        encoded[1] = (seconds >> 16) & 0xFF;
        encoded[2] = (seconds >> 8) & 0xFF;
        encoded[3] = seconds & 0xFF;
        
        // Fraction (big-endian)
        encoded[4] = (fraction >> 24) & 0xFF;
        encoded[5] = (fraction >> 16) & 0xFF;
        encoded[6] = (fraction >> 8) & 0xFF;
        encoded[7] = fraction & 0xFF;
        
        return encoded;
    }

    static std::vector<uint8_t> staticGetEncoded(uint32_t sec, uint32_t frac) {
        uint32_t fraction = static_cast<uint32_t>((static_cast<uint64_t>(frac) * (1ULL << 32)) / 1000000000ULL);
        std::vector<uint8_t> encoded(8);
        
        encoded[0] = (sec >> 24) & 0xFF;
        encoded[1] = (sec >> 16) & 0xFF;
        encoded[2] = (sec >> 8) & 0xFF;
        encoded[3] = sec & 0xFF;
        encoded[4] = (fraction >> 24) & 0xFF;
        encoded[5] = (fraction >> 16) & 0xFF;
        encoded[6] = (fraction >> 8) & 0xFF;
        encoded[7] = fraction & 0xFF;
        
        return encoded;
    }
};

/**
 * @brief IEC 61850-9-2 Quality flags (4 bytes)
 * 
 * Bit 0-10: Validity (good, invalid, questionable)
 * Bit 11: Overflow
 * Bit 12: OutOfRange
 * Bit 13: BadReference
 * Bit 14: Oscillatory
 * Bit 15: Failure
 * Bit 16: OldData
 * Bit 17: Inconsistent
 * Bit 18: Inaccurate
 * Bit 19-20: Source (process, substituted)
 * Bit 21: Test
 * Bit 22: OperatorBlocked
 */
struct Quality {
    uint32_t value;
    
    Quality() : value(0) {}
    explicit Quality(uint32_t val) : value(val) {}
    
    // Helper methods
    void setValidity(uint8_t validity) {
        value = (value & ~0x3) | (validity & 0x3);
    }
    
    void setTest(bool test) {
        if (test) value |= (1 << 21);
        else value &= ~(1 << 21);
    }
    
    void setOldData(bool oldData) {
        if (oldData) value |= (1 << 16);
        else value &= ~(1 << 16);
    }
    
    std::vector<uint8_t> getEncoded() const {
        std::vector<uint8_t> encoded(4);
        encoded[0] = (value >> 24) & 0xFF;
        encoded[1] = (value >> 16) & 0xFF;
        encoded[2] = (value >> 8) & 0xFF;
        encoded[3] = value & 0xFF;
        return encoded;
    }
};

#endif // IEC61850_TYPES_H
