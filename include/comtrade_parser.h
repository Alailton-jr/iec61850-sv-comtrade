#ifndef COMTRADE_PARSER_H
#define COMTRADE_PARSER_H

#include <string>
#include <vector>
#include <cstdint>

/**
 * @brief COMTRADE data format
 */
enum class DataFormat {
    ASCII,
    BINARY,
    BINARY32
};

/**
 * @brief Analog channel configuration
 */
struct AnalogChannel {
    int index;              // Channel index (0-based)
    std::string name;       // Channel identifier (e.g., "VA", "IA")
    std::string phase;      // Phase identifier (e.g., "A", "B", "C")
    std::string units;      // Engineering units (e.g., "kV", "A")
    double a;               // Scaling multiplier: engValue = a * rawValue + b
    double b;               // Scaling offset
    double skew;            // Time skew in microseconds
    double min;             // Min expected value
    double max;             // Max expected value
    double primary;         // Primary factor
    double secondary;       // Secondary factor
    char ps;                // Primary/Secondary flag ('P' or 'S')
};

/**
 * @brief Digital channel configuration
 */
struct DigitalChannel {
    int index;              // Channel index (0-based)
    std::string name;       // Channel identifier
    int normalState;        // Normal state (0 or 1)
};

/**
 * @brief Sample rate configuration
 */
struct SampleRate {
    double rate;            // Samples per second (Hz)
    int endSample;          // Last sample at this rate
};

/**
 * @brief COMTRADE configuration data (from .cfg file)
 */
struct ComtradeConfig {
    std::string stationName;
    std::string recDeviceId;
    int revisionYear;       // 1991, 1999, 2013
    
    int totalChannels;
    int numAnalogChannels;
    int numDigitalChannels;
    
    std::vector<AnalogChannel> analogChannels;
    std::vector<DigitalChannel> digitalChannels;
    
    double lineFreq;        // Hz (50 or 60)
    
    int numSampleRates;
    std::vector<SampleRate> sampleRates;
    
    std::string startDate;  // dd/mm/yyyy format
    std::string startTime;  // hh:mm:ss.ssssss format
    
    DataFormat dataFormat;
    
    double timeFactor;      // Time base multiplier
    
    int totalSamples;
};

/**
 * @brief Single COMTRADE sample
 */
struct ComtradeSample {
    int sampleNumber;                    // Sample index
    uint64_t timestamp;                  // Microseconds since start
    std::vector<double> analogValues;    // Scaled analog values
    std::vector<bool> digitalValues;     // Digital channel states
};

/**
 * @brief COMTRADE file parser
 * 
 * Parses IEEE C37.111 COMTRADE files (.cfg + .dat)
 * Supports 1991, 1999, and 2013 revisions
 * Handles ASCII and Binary data formats
 */
class ComtradeParser {
public:
    ComtradeParser();
    ~ComtradeParser();
    
    /**
     * @brief Load and parse COMTRADE files
     * @param cfgPath Path to .cfg file
     * @param datPath Path to .dat file (optional, auto-detected if empty)
     * @return true if successful, false otherwise
     */
    bool load(const std::string& cfgPath, const std::string& datPath = "");
    
    /**
     * @brief Get configuration
     * @return Reference to parsed configuration
     */
    const ComtradeConfig& getConfig() const { return config_; }
    
    /**
     * @brief Get total number of samples
     * @return Sample count
     */
    int getTotalSamples() const { return config_.totalSamples; }
    
    /**
     * @brief Get sample rate at given index
     * @param sampleIndex Sample number (0-based)
     * @return Sample rate in Hz
     */
    double getSampleRate(int sampleIndex) const;
    
    /**
     * @brief Get sample at given index
     * @param index Sample number (0-based)
     * @param sample Output parameter for sample data
     * @return true if successful, false if index out of range
     */
    bool getSample(int index, ComtradeSample& sample) const;
    
    /**
     * @brief Get all samples (for batch processing)
     * @return Vector of all samples
     */
    std::vector<ComtradeSample> getAllSamples() const;
    
    /**
     * @brief Get analog channel by name
     * @param name Channel name
     * @return Pointer to channel config, nullptr if not found
     */
    const AnalogChannel* getAnalogChannel(const std::string& name) const;
    
    /**
     * @brief Check if file is loaded
     * @return true if data is available
     */
    bool isLoaded() const { return loaded_; }
    
    /**
     * @brief Get last error message
     * @return Error description
     */
    std::string getLastError() const { return lastError_; }
    
    /**
     * @brief Clear loaded data
     */
    void clear();

private:
    bool parseCfg(const std::string& cfgPath);
    bool parseDatAscii(const std::string& datPath);
    bool parseDatBinary(const std::string& datPath);
    bool parseDatBinary32(const std::string& datPath);
    
    // Helper functions
    std::vector<std::string> splitLine(const std::string& line, char delim = ',');
    std::string trim(const std::string& str);
    bool parseAnalogChannelLine(const std::string& line, AnalogChannel& channel);
    bool parseDigitalChannelLine(const std::string& line, DigitalChannel& channel);
    uint64_t calculateTimestamp(int sampleNumber) const;
    
    void setError(const std::string& msg);
    
    ComtradeConfig config_;
    std::vector<ComtradeSample> samples_;
    bool loaded_;
    std::string lastError_;
};

#endif // COMTRADE_PARSER_H
