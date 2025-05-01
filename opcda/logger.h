// logger.h
#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using namespace std;

enum class LogMode
{
  NONE,
  CONSOLE,
  BUFFER,
  FILE
};

class Logger
{
public:
  static Logger& instance();

  void set_mode( LogMode mode, const string& filename = "" );

  void log( const string& message );
  void logError( const string& message );
  void logWarning( const string& message );
  void logDebug( const string& message );
  void logInfo( const string& message );


  void flush();

private:
  Logger();
  ~Logger();


  Logger( const Logger& ) = delete;
  Logger& operator=( const Logger& ) = delete;
  Logger( Logger&& ) = delete;
  Logger& operator=( Logger&& ) = delete;


  LogMode m_mode;
  string m_filename;
  vector<string> m_logBuffer;
  ofstream m_logFile;
  mutex m_lock;


  void writeToConsole( const string& message );
  void writeToBuffer( const string& message );
  void writeToFile( const string& message );


  string getCurrentTimeString();
};

namespace OPCDA::CLI::LOG
{

} // namespace OPCDA::CLI::LOG


#endif