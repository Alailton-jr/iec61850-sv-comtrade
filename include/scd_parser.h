#ifndef SCD_PARSER_H
#define SCD_PARSER_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>

/**
 * @brief Sampled Value Control Block configuration from SCL/SCD
 */
struct SampledValueControl {
    std::string name;           // Control block name (e.g., "MSVCB1")
    std::string svID;           // SV identifier (e.g., "SV_Phasors_1")
    std::string dataSet;        // Associated dataset name
    bool multicast;             // Multicast flag
    std::string smpMod;         // Sampling mode (SmpPerPeriod, SmpPerSec, SecPerSmp)
    int smpRate;                // Sample rate (samples per period or per second)
    int noASDU;                 // Number of ASDUs per frame
    int confRev;                // Configuration revision
    
    // Communication parameters
    std::string macAddress;     // Multicast MAC address (01-0C-CD-04-00-01)
    uint16_t appId;             // Application ID (0x4000)
    int vlanId;                 // VLAN ID (0 = no VLAN)
    int vlanPriority;           // VLAN priority (0-7)
    
    SampledValueControl() 
        : multicast(true), smpRate(80), noASDU(1), confRev(1),
          appId(0x4000), vlanId(0), vlanPriority(4) {}
};

/**
 * @brief Functional Constrained Data Attribute (dataset entry)
 */
struct FCDA {
    std::string ldInst;         // Logical device instance
    std::string prefix;         // LN prefix (optional)
    std::string lnClass;        // Logical node class (e.g., "TCTR")
    std::string lnInst;         // Logical node instance
    std::string doName;         // Data object name (e.g., "AmpSv")
    std::string daName;         // Data attribute name (e.g., "instMag.i")
    std::string fc;             // Functional constraint (e.g., "MX")
    
    FCDA() = default;
};

/**
 * @brief Dataset configuration
 */
struct DataSet {
    std::string name;           // Dataset name (e.g., "PhsCurrs")
    std::string ldInst;         // Logical device instance
    std::vector<FCDA> fcdas;    // List of FCDAs in dataset
    
    DataSet() = default;
};

/**
 * @brief IED (Intelligent Electronic Device) configuration
 */
struct IEDConfig {
    std::string name;                               // IED name
    std::string apName;                             // Access Point name
    std::map<std::string, DataSet> dataSets;        // Datasets by name
    std::vector<SampledValueControl> svControls;    // SV control blocks
    
    IEDConfig() = default;
};

/**
 * @brief SCL/SCD file parser for IEC 61850-9-2 Sampled Values
 * 
 * Parses System Configuration Description (SCD) or Substation Configuration Language (SCL)
 * files to extract Sampled Value control block configurations for IED communication.
 * 
 * Supports:
 * - IEC 61850-9-2LE (Light Edition) process bus
 * - SampledValueControl blocks
 * - DataSet definitions with FCDA entries
 * - Communication parameters (MAC, APPID, VLAN)
 */
class ScdParser {
public:
    ScdParser();
    ~ScdParser();
    
    /**
     * @brief Load and parse SCD/SCL file
     * @param filePath Path to .scd or .scl file
     * @return true on success, false on failure
     */
    bool load(const std::string& filePath);
    
    /**
     * @brief Check if file is loaded and parsed
     */
    bool isLoaded() const { return loaded_; }
    
    /**
     * @brief Get all IED configurations
     */
    const std::map<std::string, IEDConfig>& getIEDs() const { return ieds_; }
    
    /**
     * @brief Get specific IED configuration by name
     * @param iedName IED name to lookup
     * @return Pointer to IED config or nullptr if not found
     */
    const IEDConfig* getIED(const std::string& iedName) const;
    
    /**
     * @brief Get all Sampled Value Control blocks across all IEDs
     */
    std::vector<SampledValueControl> getAllSVControls() const;
    
    /**
     * @brief Find SV control block by svID
     * @param svId SV identifier to search for
     * @return Pointer to SV control or nullptr if not found
     */
    const SampledValueControl* findSVControlBySvId(const std::string& svId) const;
    
    /**
     * @brief Find SV control block by MAC address
     * @param macAddress MAC address string (formats: 01:0C:CD:04:00:01 or 01-0C-CD-04-00-01)
     * @return Pointer to SV control or nullptr if not found
     */
    const SampledValueControl* findSVControlByMac(const std::string& macAddress) const;
    
    /**
     * @brief Find SV control block by APPID
     * @param appId Application ID (e.g., 0x4000)
     * @return Pointer to SV control or nullptr if not found
     */
    const SampledValueControl* findSVControlByAppId(uint16_t appId) const;
    
    /**
     * @brief Get dataset for a given SV control block
     * @param svControl SV control block
     * @return Pointer to dataset or nullptr if not found
     */
    const DataSet* getDataSetForSV(const SampledValueControl& svControl) const;
    
    /**
     * @brief Get number of expected data channels from dataset
     * @param dataSet Dataset to analyze
     * @return Number of FCDA entries (channels)
     */
    int getChannelCount(const DataSet& dataSet) const;
    
    /**
     * @brief Get last parsing error message
     */
    std::string getLastError() const { return lastError_; }
    
    /**
     * @brief Clear all parsed data
     */
    void clear();
    
    /**
     * @brief Generate SCD file from configuration
     * @param config SV control block configuration
     * @param outputPath Output file path
     * @return true on success
     */
    static bool generateSCD(const SampledValueControl& config, const std::string& outputPath);
    
private:
    bool loaded_;
    std::string lastError_;
    std::map<std::string, IEDConfig> ieds_;  // IEDs by name
    
    // Helper functions for parsing
    void setError(const std::string& msg);
    bool parseXML(const std::string& xmlContent);
    bool parseIED(const std::string& xmlContent, size_t& pos);
    bool parseDataSet(const std::string& xmlContent, size_t startPos, size_t endPos, DataSet& dataSet);
    bool parseSVControl(const std::string& xmlContent, size_t startPos, size_t endPos, 
                       SampledValueControl& svControl);
    bool parseCommunication(const std::string& xmlContent);
    bool parseSMVAddress(const std::string& xmlContent, size_t startPos, size_t endPos,
                        SampledValueControl& svControl);
    
    // XML utility functions
    std::string extractAttribute(const std::string& tag, const std::string& attrName) const;
    std::string extractTagContent(const std::string& xml, const std::string& tagName, 
                                  size_t startPos = 0, size_t* nextPos = nullptr) const;
    std::string extractPTypeValue(const std::string& xml, const std::string& pType, 
                                  size_t startPos, size_t endPos) const;
    std::string trim(const std::string& str) const;
    std::string normalizeMAC(const std::string& mac) const;
    uint16_t parseAppId(const std::string& appIdStr) const;
};

#endif // SCD_PARSER_H
