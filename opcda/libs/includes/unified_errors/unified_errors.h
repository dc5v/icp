// unified_errors.h
#ifndef UNIFIED_ERRORS_H
#define UNIFIED_ERRORS_H
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

enum class U_ProtocolType : uint8_t
{
  SYSTEM = 0,
  OPCDA = 1,
  OPCUA = 2,
  PISDK = 3
};

enum class U_ErrorSeverity : uint8_t
{
  GOOD = 0,
  INFO = 1,
  WARNING = 2,
  UNCERTAIN = 3,
  ERROR = 4,
  CRITICAL_ERROR = 5,
  FATAL_ERROR = 6
};

enum class U_ErrorCategory : uint8_t
{
  NONE = 0,
  CONNECTION = 1,
  SECURITY = 2,
  COMMUNICATION = 3,
  CONFIGURATION = 4,
  DEVICE = 5,
  TAG = 6,
  DATA_QUALITY = 7,
  TIMEOUT = 8,
  RESOURCE = 9,
  PERMISSION = 10,
  VALIDATION = 11,
  INTERNAL = 12,
  EXTERNAL = 13,
  UNKNOWN = 255
};

struct OriginalError
{
  uint32_t errorCode;
  uint32_t detailCode;
  string errorMessage;
  string detailMessage;
  string additionalInfo;

  OriginalError() : errorCode( 0 ), detailCode( 0 )
  {
  }

  OriginalError( uint32_t code, uint32_t detail, const string& message, const string& detailMsg, const string& additional = "" ) : errorCode( code ), detailCode( detail ), errorMessage( message ), detailMessage( detailMsg ), additionalInfo( additional )
  {
  }

  bool hasError() const
  {
    return errorCode != 0 || !errorMessage.empty();
  }

  string getFullMessage() const
  {
    if ( !hasError() )
    {
      return "";
    }

    stringstream ss;
    ss << errorMessage;

    if ( !detailMessage.empty() )
    {
      ss << " (" << detailMessage << ")";
    }
    if ( !additionalInfo.empty() )
    {
      ss << " [" << additionalInfo << "]";
    }
    return ss.str();
  }
};

class UnifiedError
{
private:
  U_ProtocolType m_protocol;
  U_ErrorSeverity m_severity;
  U_ErrorCategory m_category;
  uint32_t m_unifiedCode;
  string m_errorMessage;
  string m_detailMessage;
  OriginalError m_originalError;
  int64_t m_timestamp;
  string m_source;

public:
  UnifiedError() : m_protocol( U_ProtocolType::SYSTEM ), m_severity( U_ErrorSeverity::GOOD ), m_category( U_ErrorCategory::NONE ), m_unifiedCode( 0 ), m_timestamp( getCurrentTimestamp() )
  {
  }
  UnifiedError( U_ProtocolType protocol, U_ErrorSeverity severity, U_ErrorCategory category, uint32_t unifiedCode, const string& errorMessage, const string& detailMessage = "", const OriginalError& originalError = OriginalError(), const string& source = "" )
      : m_protocol( protocol ), m_severity( severity ), m_category( category ), m_unifiedCode( unifiedCode ), m_errorMessage( errorMessage ), m_detailMessage( detailMessage ), m_originalError( originalError ), m_timestamp( getCurrentTimestamp() ), m_source( source )
  {
  }
  bool hasError() const
  {
    return m_severity >= U_ErrorSeverity::ERROR;
  }
  bool hasWarning() const
  {
    return m_severity == U_ErrorSeverity::WARNING || m_severity == U_ErrorSeverity::UNCERTAIN;
  }
  bool isGood() const
  {
    return m_severity == U_ErrorSeverity::GOOD;
  }
  U_ProtocolType getProtocol() const
  {
    return m_protocol;
  }
  U_ErrorSeverity getSeverity() const
  {
    return m_severity;
  }
  U_ErrorCategory getCategory() const
  {
    return m_category;
  }
  uint32_t getUnifiedCode() const
  {
    return m_unifiedCode;
  }
  const string& getErrorMessage() const
  {
    return m_errorMessage;
  }
  const string& getDetailMessage() const
  {
    return m_detailMessage;
  }
  const OriginalError& getOriginalError() const
  {
    return m_originalError;
  }
  int64_t getTimestamp() const
  {
    return m_timestamp;
  }
  const string& getSource() const
  {
    return m_source;
  }
  string getFullUnifiedMessage() const
  {
    if ( m_errorMessage.empty() )
    {
      return "";
    }
    stringstream ss;
    ss << m_errorMessage;
    if ( !m_detailMessage.empty() )
    {
      ss << " (" << m_detailMessage << ")";
    }
    return ss.str();
  }
  string getFormattedErrorCode() const
  {
    stringstream ss;
    ss << uppercase << setfill( '0' ) << hex << setw( 2 ) << static_cast<int>( m_protocol ) << "-" << setw( 2 ) << static_cast<int>( m_category ) << "-" << setw( 2 ) << static_cast<int>( m_severity ) << "-" << setw( 8 ) << m_unifiedCode;
    return ss.str();
  }
  string toString( bool includeOriginal = true ) const
  {
    stringstream ss;
    ss << "[" << protocolToString( m_protocol ) << "] " << severityToString( m_severity ) << " " << categoryToString( m_category ) << ": ";
    ss << getFullUnifiedMessage();
    ss << " (Code: " << getFormattedErrorCode() << ")";
    if ( includeOriginal && m_originalError.hasError() )
    {
      ss << " | Original: " << m_originalError.getFullMessage() << " (Code: 0x" << hex << uppercase << setfill( '0' ) << setw( 8 ) << m_originalError.errorCode;
      if ( m_originalError.detailCode != 0 )
      {
        ss << ", Detail: 0x" << setw( 8 ) << m_originalError.detailCode;
      }
      ss << ")";
    }
    return ss.str();
  }

private:
  static int64_t getCurrentTimestamp()
  {
    auto now = chrono::system_clock::now();
    return chrono::duration_cast<chrono::milliseconds>( now.time_since_epoch() ).count();
  }
  static string protocolToString( U_ProtocolType protocol )
  {
    switch ( protocol )
    {
      case U_ProtocolType::SYSTEM:
        return "SYSTEM";
      case U_ProtocolType::OPCDA:
        return "OPC DA";
      case U_ProtocolType::OPCUA:
        return "OPC UA";
      case U_ProtocolType::PISDK:
        return "PI SDK";
      default:
        return "UNKNOWN";
    }
  }
  static string severityToString( U_ErrorSeverity severity )
  {
    switch ( severity )
    {
      case U_ErrorSeverity::GOOD:
        return "GOOD";
      case U_ErrorSeverity::INFO:
        return "INFO";
      case U_ErrorSeverity::WARNING:
        return "WARNING";
      case U_ErrorSeverity::UNCERTAIN:
        return "UNCERTAIN";
      case U_ErrorSeverity::ERROR:
        return "ERROR";
      case U_ErrorSeverity::CRITICAL_ERROR:
        return "CRITICAL";
      case U_ErrorSeverity::FATAL_ERROR:
        return "FATAL";
      default:
        return "UNKNOWN";
    }
  }
  static string categoryToString( U_ErrorCategory category )
  {
    switch ( category )
    {
      case U_ErrorCategory::NONE:
        return "NO ERROR";
      case U_ErrorCategory::CONNECTION:
        return "CONNECTION";
      case U_ErrorCategory::SECURITY:
        return "SECURITY";
      case U_ErrorCategory::COMMUNICATION:
        return "COMMUNICATION";
      case U_ErrorCategory::CONFIGURATION:
        return "CONFIGURATION";
      case U_ErrorCategory::DEVICE:
        return "DEVICE";
      case U_ErrorCategory::TAG:
        return "TAG";
      case U_ErrorCategory::DATA_QUALITY:
        return "DATA QUALITY";
      case U_ErrorCategory::TIMEOUT:
        return "TIMEOUT";
      case U_ErrorCategory::RESOURCE:
        return "RESOURCE";
      case U_ErrorCategory::PERMISSION:
        return "PERMISSION";
      case U_ErrorCategory::VALIDATION:
        return "VALIDATION";
      case U_ErrorCategory::INTERNAL:
        return "INTERNAL";
      case U_ErrorCategory::EXTERNAL:
        return "EXTERNAL";
      case U_ErrorCategory::UNKNOWN:
        return "UNKNOWN";
      default:
        return "UNCLASSIFIED";
    }
  }
};
namespace OpcConstants
{
  constexpr uint16_t U_OPC_QUALITY_MASK = 0x00C0;
  constexpr uint16_t U_OPC_STATUS_MASK = 0x00FC;
  constexpr uint16_t U_OPC_LIMIT_MASK = 0x0003;
  constexpr uint16_t U_OPC_QUALITY_BAD = 0x0000;
  constexpr uint16_t U_OPC_QUALITY_UNCERTAIN = 0x0040;
  constexpr uint16_t U_OPC_QUALITY_GOOD = 0x00C0;
  constexpr uint16_t U_OPC_QUALITY_CONFIG_ERROR = 0x0004;
  constexpr uint16_t U_OPC_QUALITY_NOT_CONNECTED = 0x0008;
  constexpr uint16_t U_OPC_QUALITY_DEVICE_FAILURE = 0x000C;
  constexpr uint16_t U_OPC_QUALITY_SENSOR_FAILURE = 0x0010;
  constexpr uint16_t U_OPC_QUALITY_LAST_KNOWN = 0x0014;
  constexpr uint16_t U_OPC_QUALITY_COMM_FAILURE = 0x0018;
  constexpr uint16_t U_OPC_QUALITY_OUT_OF_SERVICE = 0x001C;
  constexpr uint16_t U_OPC_QUALITY_WAITING_FOR_INITIAL_DATA = 0x0020;
  constexpr uint16_t U_OPC_QUALITY_LAST_USABLE = 0x0044;
  constexpr uint16_t U_OPC_QUALITY_SENSOR_CAL = 0x0050;
  constexpr uint16_t U_OPC_QUALITY_EGU_EXCEEDED = 0x0054;
  constexpr uint16_t U_OPC_QUALITY_SUB_NORMAL = 0x0058;
  constexpr uint16_t U_OPC_QUALITY_LOCAL_OVERRIDE = 0x00D8;
  constexpr uint16_t U_OPC_LIMIT_OK = 0x0000;
  constexpr uint16_t U_OPC_LIMIT_LOW = 0x0001;
  constexpr uint16_t U_OPC_LIMIT_HIGH = 0x0002;
  constexpr uint16_t U_OPC_LIMIT_CONST = 0x0003;
} // namespace OpcConstants
class ErrorConverter
{
public:
  static ErrorConverter& getInstance()
  {
    static ErrorConverter instance;
    return instance;
  }
  UnifiedError fromOpcDaQuality( uint16_t quality, const string& source = "" )
  {
    using namespace OpcConstants;
    uint16_t qualityStatus = quality & U_OPC_QUALITY_MASK;
    uint16_t subStatus = quality & U_OPC_STATUS_MASK;
    uint16_t limitBits = quality & U_OPC_LIMIT_MASK;
    U_ErrorSeverity severity = U_ErrorSeverity::GOOD;
    U_ErrorCategory category = U_ErrorCategory::NONE;
    uint32_t unifiedCode = 0;
    string errorMessage;
    string detailMessage;
    switch ( qualityStatus )
    {
      case U_OPC_QUALITY_BAD:
        severity = U_ErrorSeverity::ERROR;
        errorMessage = "Bad Quality";
        category = U_ErrorCategory::DATA_QUALITY;
        unifiedCode = 4000;
        break;
      case U_OPC_QUALITY_UNCERTAIN:
        severity = U_ErrorSeverity::UNCERTAIN;
        errorMessage = "Uncertain Quality";
        category = U_ErrorCategory::DATA_QUALITY;
        unifiedCode = 3000;
        break;
      case U_OPC_QUALITY_GOOD:
        severity = U_ErrorSeverity::GOOD;
        errorMessage = "Good Quality";
        category = U_ErrorCategory::NONE;
        unifiedCode = 0;
        break;
      default:
        severity = U_ErrorSeverity::ERROR;
        errorMessage = "Unknown Quality";
        category = U_ErrorCategory::UNKNOWN;
        unifiedCode = 8000;
        break;
    }
    string subStatusMessage;
    string limitMessage;
    if ( qualityStatus == U_OPC_QUALITY_BAD )
    {
      switch ( subStatus )
      {
        case U_OPC_QUALITY_CONFIG_ERROR:
          category = U_ErrorCategory::CONFIGURATION;
          subStatusMessage = "Configuration Error";
          unifiedCode = 4001;
          break;
        case U_OPC_QUALITY_NOT_CONNECTED:
          category = U_ErrorCategory::CONNECTION;
          subStatusMessage = "Not Connected";
          unifiedCode = 4002;
          break;
        case U_OPC_QUALITY_DEVICE_FAILURE:
          category = U_ErrorCategory::DEVICE;
          subStatusMessage = "Device Failure";
          unifiedCode = 4003;
          break;
        case U_OPC_QUALITY_SENSOR_FAILURE:
          category = U_ErrorCategory::TAG;
          subStatusMessage = "Sensor Failure";
          unifiedCode = 4004;
          break;
        case U_OPC_QUALITY_LAST_KNOWN:
          subStatusMessage = "Last Known Value";
          unifiedCode = 4005;
          break;
        case U_OPC_QUALITY_COMM_FAILURE:
          category = U_ErrorCategory::COMMUNICATION;
          subStatusMessage = "Communication Failure";
          unifiedCode = 4006;
          break;
        case U_OPC_QUALITY_OUT_OF_SERVICE:
          subStatusMessage = "Out Of Service";
          unifiedCode = 4007;
          break;
        case U_OPC_QUALITY_WAITING_FOR_INITIAL_DATA:
          subStatusMessage = "Waiting For Initial Data";
          unifiedCode = 4008;
          break;
        default:
          if ( subStatus != 0 )
          {
            subStatusMessage = "Unknown Sub-Status";
            unifiedCode = 4999;
          }
          break;
      }
    }
    else if ( qualityStatus == U_OPC_QUALITY_UNCERTAIN )
    {
      switch ( subStatus )
      {
        case U_OPC_QUALITY_LAST_USABLE:
          subStatusMessage = "Last Usable Value";
          unifiedCode = 3001;
          break;
        case U_OPC_QUALITY_SENSOR_CAL:
          category = U_ErrorCategory::TAG;
          subStatusMessage = "Sensor Calibration";
          unifiedCode = 3002;
          break;
        case U_OPC_QUALITY_EGU_EXCEEDED:
          subStatusMessage = "Engineering Unit Exceeded";
          unifiedCode = 3003;
          break;
        case U_OPC_QUALITY_SUB_NORMAL:
          subStatusMessage = "Sub Normal";
          unifiedCode = 3004;
          break;
        default:
          if ( subStatus != 0 )
          {
            subStatusMessage = "Unknown Sub-Status";
            unifiedCode = 3999;
          }
          break;
      }
    }
    else if ( qualityStatus == U_OPC_QUALITY_GOOD )
    {
      if ( subStatus == U_OPC_QUALITY_LOCAL_OVERRIDE )
      {
        subStatusMessage = "Local Override";
        unifiedCode = 1;
      }
      else if ( subStatus != 0 )
      {
        subStatusMessage = "Unknown Sub-Status";
        unifiedCode = 999;
      }
    }
    switch ( limitBits )
    {
      case U_OPC_LIMIT_OK:
        break;
      case U_OPC_LIMIT_LOW:
        limitMessage = "Low Limit";
        unifiedCode += 10000;
        break;
      case U_OPC_LIMIT_HIGH:
        limitMessage = "High Limit";
        unifiedCode += 20000;
        break;
      case U_OPC_LIMIT_CONST:
        limitMessage = "Constant";
        unifiedCode += 30000;
        break;
      default:
        if ( limitBits != 0 )
        {
          limitMessage = "Unknown Limit";
          unifiedCode += 90000;
        }
        break;
    }
    if ( !subStatusMessage.empty() )
    {
      detailMessage = subStatusMessage;
    }
    OriginalError originalError( quality, subStatus, "OPC Quality Error", subStatusMessage, limitMessage );
    return UnifiedError( U_ProtocolType::OPCDA, severity, category, unifiedCode, errorMessage, detailMessage, originalError, source );
  }
  UnifiedError createSystemError( U_ErrorSeverity severity, U_ErrorCategory category, const string& errorMessage, const string& detailMessage = "", const string& source = "" )
  {
    uint32_t unifiedCode = ( static_cast<uint32_t>( severity ) * 1000 ) + ( static_cast<uint32_t>( category ) * 100 );
    OriginalError originalError( unifiedCode, 0, errorMessage, detailMessage, "" );
    return UnifiedError( U_ProtocolType::SYSTEM, severity, category, unifiedCode, errorMessage, detailMessage, originalError, source );
  }

private:
  ErrorConverter()
  {
  }
};
#endif