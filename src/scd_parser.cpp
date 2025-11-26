#include "scd_parser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>

ScdParser::ScdParser() 
    : loaded_(false) {
}

ScdParser::~ScdParser() {
    clear();
}

void ScdParser::clear() {
    ieds_.clear();
    loaded_ = false;
    lastError_.clear();
}

void ScdParser::setError(const std::string& msg) {
    lastError_ = msg;
    loaded_ = false;
}

std::string ScdParser::trim(const std::string& str) const {
    if (str.empty()) return "";
    
    size_t start = 0;
    size_t end = str.length();
    
    while (start < end && std::isspace(static_cast<unsigned char>(str[start]))) {
        start++;
    }
    
    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        end--;
    }
    
    return str.substr(start, end - start);
}

std::string ScdParser::normalizeMAC(const std::string& mac) const {
    std::string normalized;
    for (char c : mac) {
        if (c == '-' || c == ':') {
            normalized += ':';
        } else {
            normalized += std::toupper(static_cast<unsigned char>(c));
        }
    }
    return normalized;
}

uint16_t ScdParser::parseAppId(const std::string& appIdStr) const {
    std::string str = trim(appIdStr);
    
    // Remove 0x prefix if present
    if (str.size() >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str = str.substr(2);
    }
    
    // Parse as hex
    uint16_t appId = 0;
    std::istringstream iss(str);
    iss >> std::hex >> appId;
    
    return appId;
}

std::string ScdParser::extractAttribute(const std::string& tag, const std::string& attrName) const {
    size_t pos = tag.find(attrName + "=\"");
    if (pos == std::string::npos) {
        pos = tag.find(attrName + "='");
        if (pos == std::string::npos) return "";
    }
    
    pos += attrName.length() + 2;  // Skip 'name="'
    size_t endPos = tag.find_first_of("\"'", pos);
    if (endPos == std::string::npos) return "";
    
    return tag.substr(pos, endPos - pos);
}

std::string ScdParser::extractTagContent(const std::string& xml, const std::string& tagName, 
                                         size_t startPos, size_t* nextPos) const {
    std::string openTag = "<" + tagName;
    std::string closeTag = "</" + tagName + ">";
    
    size_t tagStart = xml.find(openTag, startPos);
    if (tagStart == std::string::npos) return "";
    
    size_t contentStart = xml.find(">", tagStart);
    if (contentStart == std::string::npos) return "";
    contentStart++;
    
    size_t contentEnd = xml.find(closeTag, contentStart);
    if (contentEnd == std::string::npos) return "";
    
    if (nextPos) {
        *nextPos = contentEnd + closeTag.length();
    }
    
    return xml.substr(contentStart, contentEnd - contentStart);
}

std::string ScdParser::extractPTypeValue(const std::string& xml, const std::string& pType,
                                         size_t startPos, size_t endPos) const {
    size_t pos = startPos;
    
    while (pos < endPos) {
        size_t pStart = xml.find("<P ", pos);
        if (pStart == std::string::npos || pStart >= endPos) break;
        
        size_t pEnd = xml.find("</P>", pStart);
        if (pEnd == std::string::npos || pEnd >= endPos) break;
        
        std::string pTag = xml.substr(pStart, pEnd - pStart + 4);
        
        std::string type = extractAttribute(pTag, "type");
        if (type == pType) {
            size_t contentStart = pTag.find(">") + 1;
            size_t contentEnd = pTag.find("</P>");
            return trim(pTag.substr(contentStart, contentEnd - contentStart));
        }
        
        pos = pEnd + 4;
    }
    
    return "";
}

bool ScdParser::load(const std::string& filePath) {
    clear();
    
    std::ifstream file(filePath);
    if (!file.is_open()) {
        setError("Failed to open file: " + filePath);
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string xmlContent = buffer.str();
    
    if (!parseXML(xmlContent)) {
        return false;
    }
    
    loaded_ = true;
    return true;
}

bool ScdParser::parseXML(const std::string& xmlContent) {
    // Parse all IEDs
    size_t pos = 0;
    while ((pos = xmlContent.find("<IED ", pos)) != std::string::npos) {
        if (!parseIED(xmlContent, pos)) {
            // Continue to next IED even if one fails
            pos = xmlContent.find("</IED>", pos);
            if (pos != std::string::npos) pos += 6;
        }
    }
    
    // Parse Communication section to associate addresses with SV controls
    if (!parseCommunication(xmlContent)) {
        // Not critical - SV controls can exist without communication section
    }
    
    if (ieds_.empty()) {
        setError("No IEDs found in SCL file");
        return false;
    }
    
    return true;
}

bool ScdParser::parseIED(const std::string& xmlContent, size_t& pos) {
    size_t iedStart = pos;
    size_t iedEnd = xmlContent.find("</IED>", iedStart);
    if (iedEnd == std::string::npos) return false;
    
    std::string iedSection = xmlContent.substr(iedStart, iedEnd - iedStart);
    
    // Extract IED name
    size_t nameStart = iedSection.find("name=\"");
    if (nameStart == std::string::npos) {
        pos = iedEnd + 6;
        return false;
    }
    nameStart += 6;
    size_t nameEnd = iedSection.find("\"", nameStart);
    std::string iedName = iedSection.substr(nameStart, nameEnd - nameStart);
    
    IEDConfig iedConfig;
    iedConfig.name = iedName;
    
    // Find AccessPoint name
    size_t apStart = iedSection.find("<AccessPoint ");
    if (apStart != std::string::npos) {
        size_t apNameStart = iedSection.find("name=\"", apStart);
        if (apNameStart != std::string::npos) {
            apNameStart += 6;
            size_t apNameEnd = iedSection.find("\"", apNameStart);
            iedConfig.apName = iedSection.substr(apNameStart, apNameEnd - apNameStart);
        }
    }
    
    // Parse DataSets
    size_t dsPos = 0;
    while ((dsPos = iedSection.find("<DataSet ", dsPos)) != std::string::npos) {
        size_t dsEnd = iedSection.find("</DataSet>", dsPos);
        if (dsEnd == std::string::npos) break;
        
        DataSet dataSet;
        if (parseDataSet(iedSection, dsPos, dsEnd, dataSet)) {
            iedConfig.dataSets[dataSet.name] = dataSet;
        }
        
        dsPos = dsEnd + 10;
    }
    
    // Parse SampledValueControl blocks
    size_t svPos = 0;
    while ((svPos = iedSection.find("<SampledValueControl", svPos)) != std::string::npos) {
        size_t svEnd = iedSection.find("/>", svPos);
        if (svEnd == std::string::npos) {
            svEnd = iedSection.find("</SampledValueControl>", svPos);
            if (svEnd == std::string::npos) break;
            svEnd += 22;
        } else {
            svEnd += 2;
        }
        
        SampledValueControl svControl;
        if (parseSVControl(iedSection, svPos, svEnd, svControl)) {
            iedConfig.svControls.push_back(svControl);
        }
        
        svPos = svEnd;
    }
    
    ieds_[iedName] = iedConfig;
    pos = iedEnd + 6;
    
    return true;
}

bool ScdParser::parseDataSet(const std::string& xmlContent, size_t startPos, size_t endPos,
                             DataSet& dataSet) {
    std::string dsSection = xmlContent.substr(startPos, endPos - startPos);
    
    // Extract dataset name
    size_t nameStart = dsSection.find("name=\"");
    if (nameStart == std::string::npos) return false;
    nameStart += 6;
    size_t nameEnd = dsSection.find("\"", nameStart);
    dataSet.name = dsSection.substr(nameStart, nameEnd - nameStart);
    
    // Parse FCDA entries
    size_t fcdaPos = 0;
    while ((fcdaPos = dsSection.find("<FCDA ", fcdaPos)) != std::string::npos) {
        size_t fcdaEnd = dsSection.find("/>", fcdaPos);
        if (fcdaEnd == std::string::npos) break;
        
        std::string fcdaTag = dsSection.substr(fcdaPos, fcdaEnd - fcdaPos);
        
        FCDA fcda;
        fcda.ldInst = extractAttribute(fcdaTag, "ldInst");
        fcda.prefix = extractAttribute(fcdaTag, "prefix");
        fcda.lnClass = extractAttribute(fcdaTag, "lnClass");
        fcda.lnInst = extractAttribute(fcdaTag, "lnInst");
        fcda.doName = extractAttribute(fcdaTag, "doName");
        fcda.daName = extractAttribute(fcdaTag, "daName");
        fcda.fc = extractAttribute(fcdaTag, "fc");
        
        dataSet.fcdas.push_back(fcda);
        
        fcdaPos = fcdaEnd + 2;
    }
    
    return !dataSet.fcdas.empty();
}

bool ScdParser::parseSVControl(const std::string& xmlContent, size_t startPos, size_t endPos,
                               SampledValueControl& svControl) {
    std::string svSection = xmlContent.substr(startPos, endPos - startPos);
    
    svControl.name = extractAttribute(svSection, "name");
    svControl.svID = extractAttribute(svSection, "svID");
    svControl.dataSet = extractAttribute(svSection, "datSet");
    
    std::string multicastStr = extractAttribute(svSection, "multicast");
    svControl.multicast = (multicastStr == "true" || multicastStr == "1");
    
    svControl.smpMod = extractAttribute(svSection, "smpMod");
    
    std::string smpRateStr = extractAttribute(svSection, "smpRate");
    if (!smpRateStr.empty()) {
        svControl.smpRate = std::stoi(smpRateStr);
    }
    
    std::string noASDUStr = extractAttribute(svSection, "noASDU");
    if (!noASDUStr.empty()) {
        svControl.noASDU = std::stoi(noASDUStr);
    }
    
    std::string confRevStr = extractAttribute(svSection, "confRev");
    if (!confRevStr.empty()) {
        svControl.confRev = std::stoi(confRevStr);
    }
    
    return !svControl.name.empty() && !svControl.svID.empty();
}

bool ScdParser::parseCommunication(const std::string& xmlContent) {
    size_t commStart = xmlContent.find("<Communication");
    if (commStart == std::string::npos) return false;
    
    size_t commEnd = xmlContent.find("</Communication>", commStart);
    if (commEnd == std::string::npos) return false;
    
    std::string commSection = xmlContent.substr(commStart, commEnd - commStart);
    
    // Parse each SMV address section
    size_t smvPos = 0;
    while ((smvPos = commSection.find("<SMV ", smvPos)) != std::string::npos) {
        size_t smvEnd = commSection.find("</SMV>", smvPos);
        if (smvEnd == std::string::npos) break;
        
        std::string smvSection = commSection.substr(smvPos, smvEnd - smvPos);
        
        // Extract svID to match with control block
        std::string svId = extractAttribute(smvSection, "svID");
        if (svId.empty()) {
            smvPos = smvEnd + 6;
            continue;
        }
        
        // Find the corresponding SV control block
        SampledValueControl* svControl = nullptr;
        for (auto& iedPair : ieds_) {
            for (auto& sv : iedPair.second.svControls) {
                if (sv.svID == svId) {
                    svControl = &sv;
                    break;
                }
            }
            if (svControl) break;
        }
        
        if (svControl) {
            // Parse Address section
            size_t addrStart = smvSection.find("<Address>");
            size_t addrEnd = smvSection.find("</Address>");
            
            if (addrStart != std::string::npos && addrEnd != std::string::npos) {
                parseSMVAddress(smvSection, addrStart, addrEnd, *svControl);
            }
        }
        
        smvPos = smvEnd + 6;
    }
    
    return true;
}

bool ScdParser::parseSMVAddress(const std::string& xmlContent, size_t startPos, size_t endPos,
                                SampledValueControl& svControl) {
    std::string macStr = extractPTypeValue(xmlContent, "MAC-Address", startPos, endPos);
    if (!macStr.empty()) {
        svControl.macAddress = normalizeMAC(macStr);
    }
    
    std::string appIdStr = extractPTypeValue(xmlContent, "APPID", startPos, endPos);
    if (!appIdStr.empty()) {
        svControl.appId = parseAppId(appIdStr);
    }
    
    std::string vlanIdStr = extractPTypeValue(xmlContent, "VLAN-ID", startPos, endPos);
    if (!vlanIdStr.empty()) {
        svControl.vlanId = std::stoi(vlanIdStr);
    }
    
    std::string vlanPrioStr = extractPTypeValue(xmlContent, "VLAN-PRIORITY", startPos, endPos);
    if (!vlanPrioStr.empty()) {
        svControl.vlanPriority = std::stoi(vlanPrioStr);
    }
    
    return true;
}

const IEDConfig* ScdParser::getIED(const std::string& iedName) const {
    auto it = ieds_.find(iedName);
    if (it != ieds_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<SampledValueControl> ScdParser::getAllSVControls() const {
    std::vector<SampledValueControl> allControls;
    
    for (const auto& iedPair : ieds_) {
        for (const auto& sv : iedPair.second.svControls) {
            allControls.push_back(sv);
        }
    }
    
    return allControls;
}

const SampledValueControl* ScdParser::findSVControlBySvId(const std::string& svId) const {
    for (const auto& iedPair : ieds_) {
        for (const auto& sv : iedPair.second.svControls) {
            if (sv.svID == svId) {
                return &sv;
            }
        }
    }
    return nullptr;
}

const SampledValueControl* ScdParser::findSVControlByMac(const std::string& macAddress) const {
    std::string normalizedSearch = normalizeMAC(macAddress);
    
    for (const auto& iedPair : ieds_) {
        for (const auto& sv : iedPair.second.svControls) {
            if (normalizeMAC(sv.macAddress) == normalizedSearch) {
                return &sv;
            }
        }
    }
    return nullptr;
}

const SampledValueControl* ScdParser::findSVControlByAppId(uint16_t appId) const {
    for (const auto& iedPair : ieds_) {
        for (const auto& sv : iedPair.second.svControls) {
            if (sv.appId == appId) {
                return &sv;
            }
        }
    }
    return nullptr;
}

const DataSet* ScdParser::getDataSetForSV(const SampledValueControl& svControl) const {
    // Find the IED that contains this SV control
    for (const auto& iedPair : ieds_) {
        for (const auto& sv : iedPair.second.svControls) {
            if (sv.name == svControl.name && sv.svID == svControl.svID) {
                // Found the IED, now lookup the dataset
                auto dsIt = iedPair.second.dataSets.find(svControl.dataSet);
                if (dsIt != iedPair.second.dataSets.end()) {
                    return &dsIt->second;
                }
            }
        }
    }
    return nullptr;
}

int ScdParser::getChannelCount(const DataSet& dataSet) const {
    return static_cast<int>(dataSet.fcdas.size());
}

bool ScdParser::generateSCD(const SampledValueControl& config, const std::string& outputPath) {
    std::stringstream ss;
    
    ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    ss << "<SCL xmlns=\"http://www.iec.ch/61850/2003/SCL\" version=\"2007\" revision=\"B\">\n";
    ss << "  <Header id=\"SV_Generated_System\" version=\"1\" revision=\"0\" toolID=\"VirtualTestSet\" nameStructure=\"IEDName\"/>\n\n";
    
    ss << "  <IED name=\"SV_Publisher\" manufacturer=\"VirtualTestSet\" configVersion=\"1\">\n";
    ss << "    <AccessPoint name=\"AP1\">\n";
    ss << "      <Server>\n";
    ss << "        <Authentication/>\n";
    ss << "        <LDevice inst=\"LD_SV\" desc=\"Sampled Values Logical Device\">\n";
    ss << "          <LN0 lnClass=\"LLN0\" inst=\"\" lnType=\"LLN0_Type\" desc=\"Logical Node Zero\">\n";
    ss << "            <DataSet name=\"" << config.dataSet << "\" desc=\"Sampled Values Dataset\">\n";
    ss << "              <!-- FCDA entries for 8 channels (IEC 61850-9-2LE) -->\n";
    ss << "              <FCDA ldInst=\"LD_SV\" lnClass=\"TCTR\" lnInst=\"1\" doName=\"AmpSv\" daName=\"instMag.i\" fc=\"MX\"/>\n";
    ss << "              <FCDA ldInst=\"LD_SV\" lnClass=\"TCTR\" lnInst=\"2\" doName=\"AmpSv\" daName=\"instMag.i\" fc=\"MX\"/>\n";
    ss << "              <FCDA ldInst=\"LD_SV\" lnClass=\"TCTR\" lnInst=\"3\" doName=\"AmpSv\" daName=\"instMag.i\" fc=\"MX\"/>\n";
    ss << "              <FCDA ldInst=\"LD_SV\" lnClass=\"TCTR\" lnInst=\"4\" doName=\"AmpSv\" daName=\"instMag.i\" fc=\"MX\"/>\n";
    ss << "              <FCDA ldInst=\"LD_SV\" lnClass=\"TVTR\" lnInst=\"1\" doName=\"VolSv\" daName=\"instMag.i\" fc=\"MX\"/>\n";
    ss << "              <FCDA ldInst=\"LD_SV\" lnClass=\"TVTR\" lnInst=\"2\" doName=\"VolSv\" daName=\"instMag.i\" fc=\"MX\"/>\n";
    ss << "              <FCDA ldInst=\"LD_SV\" lnClass=\"TVTR\" lnInst=\"3\" doName=\"VolSv\" daName=\"instMag.i\" fc=\"MX\"/>\n";
    ss << "              <FCDA ldInst=\"LD_SV\" lnClass=\"TVTR\" lnInst=\"4\" doName=\"VolSv\" daName=\"instMag.i\" fc=\"MX\"/>\n";
    ss << "            </DataSet>\n\n";
    
    ss << "            <SampledValueControl\n";
    ss << "                name=\"" << config.name << "\"\n";
    ss << "                datSet=\"" << config.dataSet << "\"\n";
    ss << "                svID=\"" << config.svID << "\"\n";
    ss << "                multicast=\"" << (config.multicast ? "true" : "false") << "\"\n";
    ss << "                smpMod=\"" << config.smpMod << "\"\n";
    ss << "                smpRate=\"" << config.smpRate << "\"\n";
    ss << "                noASDU=\"" << config.noASDU << "\"\n";
    ss << "                confRev=\"" << config.confRev << "\">\n";
    ss << "              <IEDName>SV_Publisher</IEDName>\n";
    ss << "            </SampledValueControl>\n";
    ss << "          </LN0>\n\n";
    
    // Current Transformers (4 channels: IA, IB, IC, IN)
    ss << "          <LN lnClass=\"TCTR\" inst=\"1\" lnType=\"TCTR_Type\" desc=\"Current Transformer Phase A\"/>\n";
    ss << "          <LN lnClass=\"TCTR\" inst=\"2\" lnType=\"TCTR_Type\" desc=\"Current Transformer Phase B\"/>\n";
    ss << "          <LN lnClass=\"TCTR\" inst=\"3\" lnType=\"TCTR_Type\" desc=\"Current Transformer Phase C\"/>\n";
    ss << "          <LN lnClass=\"TCTR\" inst=\"4\" lnType=\"TCTR_Type\" desc=\"Current Transformer Neutral\"/>\n\n";
    
    // Voltage Transformers (4 channels: VA, VB, VC, VN)
    ss << "          <LN lnClass=\"TVTR\" inst=\"1\" lnType=\"TVTR_Type\" desc=\"Voltage Transformer Phase A\"/>\n";
    ss << "          <LN lnClass=\"TVTR\" inst=\"2\" lnType=\"TVTR_Type\" desc=\"Voltage Transformer Phase B\"/>\n";
    ss << "          <LN lnClass=\"TVTR\" inst=\"3\" lnType=\"TVTR_Type\" desc=\"Voltage Transformer Phase C\"/>\n";
    ss << "          <LN lnClass=\"TVTR\" inst=\"4\" lnType=\"TVTR_Type\" desc=\"Voltage Transformer Neutral\"/>\n";
    
    ss << "        </LDevice>\n";
    ss << "      </Server>\n";
    ss << "    </AccessPoint>\n";
    ss << "  </IED>\n\n";
    
    ss << "  <Communication>\n";
    ss << "    <SubNetwork name=\"ProcessBus\" type=\"8-MMS\" desc=\"IEC 61850-9-2 Process Bus\">\n";
    ss << "      <ConnectedAP iedName=\"SV_Publisher\" apName=\"AP1\">\n";
    ss << "        <Address>\n";
    ss << "          <P type=\"IP\">0.0.0.0</P>\n";
    ss << "          <P type=\"IP-SUBNET\">255.255.255.0</P>\n";
    ss << "          <P type=\"IP-GATEWAY\">0.0.0.0</P>\n";
    ss << "        </Address>\n\n";
    
    ss << "        <SMV ldInst=\"LD_SV\" cbName=\"" << config.name << "\" svID=\"" << config.svID << "\">\n";
    ss << "          <Address>\n";
    ss << "            <P type=\"MAC-Address\">" << config.macAddress << "</P>\n";
    ss << "            <P type=\"APPID\">" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << config.appId << "</P>\n";
    ss << "            <P type=\"VLAN-ID\">" << std::dec << config.vlanId << "</P>\n";
    ss << "            <P type=\"VLAN-PRIORITY\">" << config.vlanPriority << "</P>\n";
    ss << "          </Address>\n";
    ss << "        </SMV>\n";
    ss << "      </ConnectedAP>\n";
    ss << "    </SubNetwork>\n";
    ss << "  </Communication>\n\n";
    
    // DataTypeTemplates section with proper type definitions
    ss << "  <DataTypeTemplates>\n";
    ss << "    <!-- Logical Node Type Definitions -->\n";
    ss << "    <LNodeType id=\"LLN0_Type\" lnClass=\"LLN0\">\n";
    ss << "      <DO name=\"Mod\" type=\"INC_1\"/>\n";
    ss << "      <DO name=\"Beh\" type=\"INS_1\"/>\n";
    ss << "      <DO name=\"Health\" type=\"INS_1\"/>\n";
    ss << "      <DO name=\"NamPlt\" type=\"LPL_1\"/>\n";
    ss << "    </LNodeType>\n\n";
    
    ss << "    <LNodeType id=\"TCTR_Type\" lnClass=\"TCTR\">\n";
    ss << "      <DO name=\"Mod\" type=\"INC_1\"/>\n";
    ss << "      <DO name=\"Beh\" type=\"INS_1\"/>\n";
    ss << "      <DO name=\"Health\" type=\"INS_1\"/>\n";
    ss << "      <DO name=\"NamPlt\" type=\"LPL_1\"/>\n";
    ss << "      <DO name=\"AmpSv\" type=\"SAV_1\" desc=\"Sampled current value\"/>\n";
    ss << "    </LNodeType>\n\n";
    
    ss << "    <LNodeType id=\"TVTR_Type\" lnClass=\"TVTR\">\n";
    ss << "      <DO name=\"Mod\" type=\"INC_1\"/>\n";
    ss << "      <DO name=\"Beh\" type=\"INS_1\"/>\n";
    ss << "      <DO name=\"Health\" type=\"INS_1\"/>\n";
    ss << "      <DO name=\"NamPlt\" type=\"LPL_1\"/>\n";
    ss << "      <DO name=\"VolSv\" type=\"SAV_1\" desc=\"Sampled voltage value\"/>\n";
    ss << "    </LNodeType>\n\n";
    
    // Data Object Type Definitions
    ss << "    <!-- Data Object Type Definitions -->\n";
    ss << "    <DOType id=\"SAV_1\" cdc=\"SAV\">\n";
    ss << "      <DA name=\"instMag\" fc=\"MX\" bType=\"Struct\" type=\"AnalogueValue_1\"/>\n";
    ss << "      <DA name=\"q\" fc=\"MX\" bType=\"Quality\" type=\"Quality\"/>\n";
    ss << "      <DA name=\"t\" fc=\"MX\" bType=\"Timestamp\"/>\n";
    ss << "      <DA name=\"sVC\" fc=\"CF\" bType=\"Struct\" type=\"ScaledValueConfig\"/>\n";
    ss << "    </DOType>\n\n";
    
    ss << "    <DOType id=\"INC_1\" cdc=\"INC\">\n";
    ss << "      <DA name=\"stVal\" fc=\"ST\" bType=\"INT32\"/>\n";
    ss << "      <DA name=\"q\" fc=\"ST\" bType=\"Quality\"/>\n";
    ss << "      <DA name=\"t\" fc=\"ST\" bType=\"Timestamp\"/>\n";
    ss << "      <DA name=\"ctlModel\" fc=\"CF\" bType=\"Enum\" type=\"CtlModelKind\"/>\n";
    ss << "    </DOType>\n\n";
    
    ss << "    <DOType id=\"INS_1\" cdc=\"INS\">\n";
    ss << "      <DA name=\"stVal\" fc=\"ST\" bType=\"INT32\"/>\n";
    ss << "      <DA name=\"q\" fc=\"ST\" bType=\"Quality\"/>\n";
    ss << "      <DA name=\"t\" fc=\"ST\" bType=\"Timestamp\"/>\n";
    ss << "    </DOType>\n\n";
    
    ss << "    <DOType id=\"LPL_1\" cdc=\"LPL\">\n";
    ss << "      <DA name=\"vendor\" fc=\"DC\" bType=\"VisString255\"/>\n";
    ss << "      <DA name=\"swRev\" fc=\"DC\" bType=\"VisString255\"/>\n";
    ss << "      <DA name=\"d\" fc=\"DC\" bType=\"VisString255\"/>\n";
    ss << "      <DA name=\"configRev\" fc=\"DC\" bType=\"VisString255\"/>\n";
    ss << "      <DA name=\"ldNs\" fc=\"EX\" bType=\"VisString255\"/>\n";
    ss << "    </DOType>\n\n";
    
    // Data Attribute Type Definitions
    ss << "    <!-- Data Attribute Type Definitions -->\n";
    ss << "    <DAType id=\"AnalogueValue_1\">\n";
    ss << "      <BDA name=\"i\" bType=\"INT32\" desc=\"Instantaneous integer value\"/>\n";
    ss << "      <BDA name=\"f\" bType=\"FLOAT32\" desc=\"Instantaneous float value\"/>\n";
    ss << "    </DAType>\n\n";
    
    ss << "    <DAType id=\"ScaledValueConfig\">\n";
    ss << "      <BDA name=\"scaleFactor\" bType=\"FLOAT32\"/>\n";
    ss << "      <BDA name=\"offset\" bType=\"FLOAT32\"/>\n";
    ss << "    </DAType>\n\n";
    
    // Enumeration Type Definitions
    ss << "    <!-- Enumeration Type Definitions -->\n";
    ss << "    <EnumType id=\"CtlModelKind\">\n";
    ss << "      <EnumVal ord=\"0\">status-only</EnumVal>\n";
    ss << "      <EnumVal ord=\"1\">direct-with-normal-security</EnumVal>\n";
    ss << "      <EnumVal ord=\"2\">sbo-with-normal-security</EnumVal>\n";
    ss << "      <EnumVal ord=\"3\">direct-with-enhanced-security</EnumVal>\n";
    ss << "      <EnumVal ord=\"4\">sbo-with-enhanced-security</EnumVal>\n";
    ss << "    </EnumType>\n";
    
    ss << "  </DataTypeTemplates>\n\n";
    ss << "</SCL>\n";
    
    std::ofstream file(outputPath);
    if (!file.is_open()) {
        return false;
    }
    
    file << ss.str();
    file.close();
    
    return true;
}
