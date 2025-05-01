// crash_handler.cpp
#include <iostream>
#include <sstream>

#include "crash_handler.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <cxxabi.h>
#include <execinfo.h>
#include <unistd.h>
#endif

void setupCrashHandler()
{
  signal( SIGSEGV, crashHandler );
  signal( SIGABRT, crashHandler );
  signal( SIGFPE, crashHandler );
  signal( SIGILL, crashHandler );
#ifndef _WIN32
  signal( SIGBUS, crashHandler );
#endif
}

// 간단한 스택 트레이스 구현
string getStackTrace()
{
  stringstream stackTrace;

#ifdef _WIN32
  // Windows에서는 간단한 정보만 제공
  CONTEXT context;
  ZeroMemory( &context, sizeof( CONTEXT ) );
  context.ContextFlags = CONTEXT_FULL;
  RtlCaptureContext( &context );

  stackTrace << "  Program Counter: 0x" << hex
#ifdef _M_X64
             << context.Rip
#else
             << context.Eip
#endif
             << dec << endl;

  stackTrace << "  Stack trace information unavailable. Enable more detailed logs for debugging." << endl;
#else
  // UNIX/Linux 환경에서의 스택 트레이스 구현
  void* array[50];
  int size = backtrace( array, 50 );
  char** messages = backtrace_symbols( array, size );

  for ( int i = 0; i < size && messages != nullptr; i++ )
  {
    stackTrace << "  " << messages[i] << "\n";
  }

  if ( messages )
  {
    free( messages );
  }
#endif

  return stackTrace.str();
}

void crashHandler( int sig )
{
  // 충돌 정보 출력
  cerr << "crash:" << endl;
  cerr << "  code: " << sig << endl;

  // 신호 이름 얻기
  string signal_name;
  switch ( sig )
  {
    case SIGSEGV:
      signal_name = "Segmentation fault (SIGSEGV)";
      break;
    case SIGABRT:
      signal_name = "Abort/assert (SIGABRT)";
      break;
    case SIGFPE:
      signal_name = "Floating point exception (SIGFPE)";
      break;
    case SIGILL:
      signal_name = "Illegal instruction (SIGILL)";
      break;
#ifndef _WIN32
    case SIGBUS:
      signal_name = "Bus error (SIGBUS)";
      break;
#endif
    default:
      signal_name = "Unknown signal";
      break;
  }

  cerr << "  message: " << signal_name << endl;
  cerr << "  stacks: " << endl;
  cerr << getStackTrace();

  // 프로그램 종료
  exit( sig );
}