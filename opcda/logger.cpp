// logger.cpp
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "logger.h"
#include "result_formatter.hpp"

using namespace std;

Logger& Logger::instance()
{
  static Logger instance;
  return instance;
}

Logger::Logger() : m_mode( LogMode::NONE )
{
}

Logger::~Logger()
{
  flush();

  if ( m_logFile.is_open() )
  {
    m_logFile.close();
  }
}

void Logger::set_mode( LogMode mode, const string& filename )
{
  lock_guard<mutex> lock( m_lock );

  if ( mode == LogMode::FILE )
  {
    m_filename = filename;

    if ( m_logFile.is_open() )
    {
      m_logFile.close();
    }

    m_logFile.open( m_filename, ios::app );

    if ( !m_logFile )
    {
      cerr << "Error: Failed to open log file: " << filename << endl;
      m_mode = LogMode::CONSOLE;
    }
  }

  m_mode = mode;
}

string Logger::getCurrentTimeString()
{
  auto now = chrono::system_clock::now();
  auto time = chrono::system_clock::to_time_t( now );


  auto ms = chrono::duration_cast<chrono::milliseconds>( now.time_since_epoch() ) % 1000;

  stringstream ss;
  struct tm timeinfo;

#ifdef _WIN32
  localtime_s( &timeinfo, &time );
  ss << put_time( &timeinfo, "%Y-%m-%d %H:%M:%S" );
#else
  ss << put_time( localtime( &time ), "%Y-%m-%d %H:%M:%S" );
#endif

  ss << '.' << setfill( '0' ) << setw( 3 ) << ms.count();
  return ss.str();
}

void Logger::log( const string& message )
{
  string formatted = "[" + getCurrentTimeString() + "] " + message;

  lock_guard<mutex> lock( m_lock );
  switch ( m_mode )
  {
    case LogMode::CONSOLE:
      writeToConsole( formatted );
      break;
    case LogMode::BUFFER:
      writeToBuffer( formatted );
      break;
    case LogMode::FILE:
      writeToFile( formatted );
      break;
    case LogMode::NONE:

      break;
  }
}

void Logger::logError( const string& message )
{
  log( "ERROR: " + message );
}

void Logger::logWarning( const string& message )
{
  log( "WARNING: " + message );
}

void Logger::logDebug( const string& message )
{
  log( "DEBUG: " + message );
}

void Logger::logInfo( const string& message )
{
  log( "INFO: " + message );
}

void Logger::flush()
{
  lock_guard<mutex> lock( m_lock );

  if ( m_mode == LogMode::BUFFER && !m_logBuffer.empty() )
  {
    vector<string> logs = m_logBuffer;
    m_logBuffer.clear();
    ResultFormatter::getInstance().printLogs( logs );
  }

  if ( m_mode == LogMode::FILE && m_logFile.is_open() )
  {
    m_logFile.flush();
  }
}

void Logger::writeToConsole( const string& message )
{
  cerr << message << endl;
}

void Logger::writeToBuffer( const string& message )
{
  m_logBuffer.push_back( message );
}

void Logger::writeToFile( const string& message )
{
  if ( m_logFile.is_open() )
  {
    m_logFile << message << endl;
    m_logFile.flush();
  }
  else
  {

    writeToConsole( "WARNING: Log file not open, writing to console: " + message );
  }
}