#ifndef GOOSE_DECODER_H
#define GOOSE_DECODER_H

#include <vector>
#include <string>
#include <cstdint>
#include "iec61850_types.h"

/**
 * @brief Simple GOOSE message decoder for capture
 * 
 * Decodes IEC 61850-8-1 GOOSE PDU fields
 */
struct GooseMessage {
    uint16_t appID;
    std::string gocbRef;
    uint32_t timeAllowedToLive;
    std::string datSet;
    std::string goID;
    UtcTime timestamp;
    uint32_t stNum;
    uint32_t sqNum;
    bool simulation;
    uint32_t confRev;
    bool ndsCom;
    uint32_t numDatSetEntries;
    
    std::vector<uint8_t> rawData;  // Raw allData field
    bool valid;
    
    GooseMessage() : appID(0), timeAllowedToLive(0), stNum(0), sqNum(0), 
                     simulation(false), confRev(0), ndsCom(false), 
                     numDatSetEntries(0), valid(false) {}
};

/**
 * @brief Decode GOOSE packet from raw bytes
 * @param packet Raw Ethernet frame
 * @return Decoded GOOSE message (check valid field)
 */
inline GooseMessage decodeGoose(const std::vector<uint8_t>& packet) {
    GooseMessage msg;
    
    // Minimum GOOSE frame: 14 (Eth) + 4 (VLAN) + 2 (EtherType) + 8 (Header) = 28 bytes
    if (packet.size() < 28) {
        return msg; // Invalid
    }
    
    // Find GOOSE EtherType (0x88B8)
    size_t offset = 12; // Skip dst+src MAC
    
    // Check for VLAN tag
    if (packet[offset] == 0x81 && packet[offset + 1] == 0x00) {
        offset += 4; // Skip VLAN
    }
    
    // Check EtherType
    if (offset + 2 > packet.size()) return msg;
    if (packet[offset] != 0x88 || packet[offset + 1] != 0xB8) {
        return msg; // Not GOOSE
    }
    offset += 2;
    
    // Read APPID
    if (offset + 2 > packet.size()) return msg;
    msg.appID = (static_cast<uint16_t>(packet[offset]) << 8) | packet[offset + 1];
    offset += 2;
    
    // Read Length (not used for parsing, but advance offset)
    if (offset + 2 > packet.size()) return msg;
    offset += 2;
    
    // Skip Reserved1 and Reserved2
    offset += 4;
    
    // Check PDU tag (0x61)
    if (offset >= packet.size() || packet[offset] != 0x61) return msg;
    offset++;
    
    // Read PDU length
    if (offset >= packet.size()) return msg;
    uint16_t pduLen = 0;
    if (packet[offset] & 0x80) {
        // Long form
        uint8_t numLenBytes = packet[offset] & 0x7F;
        offset++;
        if (numLenBytes == 1) {
            if (offset >= packet.size()) return msg;
            pduLen = packet[offset];
            offset++;
        } else if (numLenBytes == 2) {
            if (offset + 1 >= packet.size()) return msg;
            pduLen = (static_cast<uint16_t>(packet[offset]) << 8) | packet[offset + 1];
            offset += 2;
        }
    } else {
        pduLen = packet[offset];
        offset++;
    }
    
    // Parse PDU fields (simplified - only extract key fields)
    size_t pduEnd = offset + pduLen;
    
    while (offset < pduEnd && offset < packet.size()) {
        uint8_t tag = packet[offset++];
        if (offset >= packet.size()) break;
        
        // Read length
        uint16_t fieldLen = 0;
        if (packet[offset] & 0x80) {
            uint8_t numLenBytes = packet[offset] & 0x7F;
            offset++;
            if (numLenBytes == 1 && offset < packet.size()) {
                fieldLen = packet[offset++];
            } else if (numLenBytes == 2 && offset + 1 < packet.size()) {
                fieldLen = (static_cast<uint16_t>(packet[offset]) << 8) | packet[offset + 1];
                offset += 2;
            }
        } else {
            fieldLen = packet[offset++];
        }
        
        if (offset + fieldLen > packet.size()) break;
        
        // Extract based on tag
        switch (tag) {
            case 0x80: // gocbRef
                msg.gocbRef = std::string(packet.begin() + offset, packet.begin() + offset + fieldLen);
                break;
            case 0x81: // timeAllowedToLive
                if (fieldLen == 4) {
                    msg.timeAllowedToLive = (static_cast<uint32_t>(packet[offset]) << 24) |
                                           (static_cast<uint32_t>(packet[offset + 1]) << 16) |
                                           (static_cast<uint32_t>(packet[offset + 2]) << 8) |
                                           packet[offset + 3];
                }
                break;
            case 0x82: // datSet
                msg.datSet = std::string(packet.begin() + offset, packet.begin() + offset + fieldLen);
                break;
            case 0x85: // stNum
                if (fieldLen == 4) {
                    msg.stNum = (static_cast<uint32_t>(packet[offset]) << 24) |
                               (static_cast<uint32_t>(packet[offset + 1]) << 16) |
                               (static_cast<uint32_t>(packet[offset + 2]) << 8) |
                               packet[offset + 3];
                }
                break;
            case 0x86: // sqNum
                if (fieldLen == 4) {
                    msg.sqNum = (static_cast<uint32_t>(packet[offset]) << 24) |
                               (static_cast<uint32_t>(packet[offset + 1]) << 16) |
                               (static_cast<uint32_t>(packet[offset + 2]) << 8) |
                               packet[offset + 3];
                }
                break;
        }
        
        offset += fieldLen;
    }
    
    msg.valid = !msg.gocbRef.empty();
    return msg;
}

#endif // GOOSE_DECODER_H
