#ifndef SAMPLED_VALUE_H
#define SAMPLED_VALUE_H

#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <cmath>
#include "iec61850_types.h"

/**
 * @brief IEC 61850-9-2 Sampled Value packet builder
 * 
 * Simplified implementation for manual phasor injection test
 * Supports 8 channels: 4 currents + 4 voltages (INT32 format)
 */
class SampledValue {
public:
    // Header fields
    uint16_t appID;
    uint16_t reserved1 = 0;
    uint16_t reserved2 = 0;

    // SAVPDU fields
    uint8_t noAsdu;
    
    // ASDU fields
    std::string svID;
    std::string datSet;
    uint16_t smpCnt;
    uint32_t confRev;
    UtcTime refrTm;
    uint8_t smpSynch;
    uint16_t smpRate;
    uint16_t smpMod;

    SampledValue(uint16_t app_id, const std::string& sv_id, uint16_t sample_rate = 4800)
        : appID(app_id), noAsdu(1), svID(sv_id), smpCnt(0), 
          confRev(1), smpSynch(1), smpRate(sample_rate), smpMod(0) {
        refrTm.defined = 0;
    }

    /**
     * @brief Build complete SV packet with 8 channels of phasor data
     * @param phasors Array of 8 phasors {mag, angle} for IA, IB, IC, IN, VA, VB, VC, VN
     * @param qualities Array of 8 quality values (optional, default = 0)
     * @return Complete Ethernet frame ready for transmission
     */
    std::vector<uint8_t> buildPacket(const double phasors[8][2], const uint32_t* qualities = nullptr) {
        std::vector<uint8_t> packet;
        packet.reserve(256);

        // Convert phasors to INT32 samples (instantaneous values at this sample count)
        // Calculate instantaneous value for each channel
        double omega = 2.0 * M_PI * 60.0; // 60 Hz
        double t = static_cast<double>(smpCnt) / static_cast<double>(smpRate);
        
        int32_t samples[8];
        for (int i = 0; i < 8; i++) {
            double magnitude = phasors[i][0];
            double angle_deg = phasors[i][1];
            double angle_rad = angle_deg * M_PI / 180.0;
            
            // Instantaneous value = Magnitude * sqrt(2) * cos(ωt + φ)
            samples[i] = static_cast<int32_t>(magnitude * 1.414213562 * std::cos(omega * t + angle_rad));
        }

        // Build ASDU
        std::vector<uint8_t> asduData;
        
        // svID (Tag 0x80 - VisibleString)
        asduData.push_back(0x80);
        asduData.push_back(static_cast<uint8_t>(svID.length()));
        asduData.insert(asduData.end(), svID.begin(), svID.end());
        
        // smpCnt (Tag 0x82 - INTEGER)
        asduData.push_back(0x82);
        asduData.push_back(0x02);
        asduData.push_back((smpCnt >> 8) & 0xFF);
        asduData.push_back(smpCnt & 0xFF);
        
        // confRev (Tag 0x83 - INTEGER)
        asduData.push_back(0x83);
        asduData.push_back(0x04);
        asduData.push_back((confRev >> 24) & 0xFF);
        asduData.push_back((confRev >> 16) & 0xFF);
        asduData.push_back((confRev >> 8) & 0xFF);
        asduData.push_back(confRev & 0xFF);
        
        // smpSynch (Tag 0x85 - BOOLEAN)
        asduData.push_back(0x85);
        asduData.push_back(0x01);
        asduData.push_back(smpSynch);
        
        // smpRate (Tag 0x86 - INTEGER) - Required for IEC 61850-9-2LE
        asduData.push_back(0x86);
        asduData.push_back(0x02);
        asduData.push_back((smpRate >> 8) & 0xFF);
        asduData.push_back(smpRate & 0xFF);
        
        // seqData (Tag 0x87 - SEQUENCE OF Data)
        // Each channel is 8 bytes (4 bytes INT32 value + 4 bytes quality)
        // Total: 8 channels * 8 bytes = 64 bytes
        asduData.push_back(0x87);
        asduData.push_back(64); // Length of data array (not including tag/length)
        
        for (int i = 0; i < 8; i++) {
            // INT32 value (4 bytes, big-endian)
            asduData.push_back((samples[i] >> 24) & 0xFF);
            asduData.push_back((samples[i] >> 16) & 0xFF);
            asduData.push_back((samples[i] >> 8) & 0xFF);
            asduData.push_back(samples[i] & 0xFF);
            
            // Quality (4 bytes, big-endian)
            uint32_t quality = qualities ? qualities[i] : 0;
            asduData.push_back((quality >> 24) & 0xFF);
            asduData.push_back((quality >> 16) & 0xFF);
            asduData.push_back((quality >> 8) & 0xFF);
            asduData.push_back(quality & 0xFF);
        }
        
        // Wrap ASDU in SEQUENCE (Tag 0x30)
        uint16_t asduLen = static_cast<uint16_t>(asduData.size());
        std::vector<uint8_t> seqAsdu;
        seqAsdu.push_back(0x30);
        if (asduLen > 127) {
            seqAsdu.push_back(0x81);
            seqAsdu.push_back(asduLen & 0xFF);
        } else {
            seqAsdu.push_back(asduLen & 0xFF);
        }
        seqAsdu.insert(seqAsdu.end(), asduData.begin(), asduData.end());
        
        // Build SAVPDU
        std::vector<uint8_t> savpdu;
        
        // noAsdu (Tag 0x80 - INTEGER)
        savpdu.push_back(0x80);
        savpdu.push_back(0x01);
        savpdu.push_back(noAsdu);
        
        // seqAsdu (Tag 0xA2 - SEQUENCE OF)
        uint16_t seqAsduLen = static_cast<uint16_t>(seqAsdu.size());
        savpdu.push_back(0xA2);
        if (seqAsduLen > 127) {
            savpdu.push_back(0x81);
            savpdu.push_back(seqAsduLen & 0xFF);
        } else {
            savpdu.push_back(seqAsduLen & 0xFF);
        }
        savpdu.insert(savpdu.end(), seqAsdu.begin(), seqAsdu.end());
        
        // Build final packet with IEC 61850-9-2 header
        uint16_t savpduLen = static_cast<uint16_t>(savpdu.size());
        
        // Calculate total length: Reserved1(2) + Reserved2(2) + SAVPDU tag(1) + length bytes + savpduLen
        uint16_t lengthBytes = 0;
        if (savpduLen > 255) {
            lengthBytes = 3; // 0x82 + 2 bytes
        } else if (savpduLen > 127) {
            lengthBytes = 2; // 0x81 + 1 byte
        } else {
            lengthBytes = 1; // 1 byte
        }
        uint16_t totalLen = 4 + 1 + lengthBytes + savpduLen + 4;
        
        // EtherType (0x88BA for SV)
        packet.push_back(0x88);
        packet.push_back(0xBA);
        
        // APPID
        packet.push_back((appID >> 8) & 0xFF);
        packet.push_back(appID & 0xFF);
        
        // Length
        packet.push_back((totalLen >> 8) & 0xFF);
        packet.push_back(totalLen & 0xFF);
        
        // Reserved1
        packet.push_back(0x00);
        packet.push_back(0x00);
        
        // Reserved2
        packet.push_back(0x00);
        packet.push_back(0x00);
        
        // SAVPDU (Tag 0x60)
        packet.push_back(0x60);
        if (savpduLen > 255) {
            packet.push_back(0x82);
            packet.push_back((savpduLen >> 8) & 0xFF);
            packet.push_back(savpduLen & 0xFF);
        } else if (savpduLen > 127) {
            packet.push_back(0x81);
            packet.push_back(savpduLen & 0xFF);
        } else {
            packet.push_back(savpduLen & 0xFF);
        }
        packet.insert(packet.end(), savpdu.begin(), savpdu.end());
        
        return packet;
    }

    void incrementSampleCount() {
        smpCnt++;
        if (smpCnt >= smpRate) smpCnt = 0;
    }
};

#endif // SAMPLED_VALUE_H
