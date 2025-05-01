// crash_handler.h
#ifndef CRASH_HANDLER_H
#define CRASH_HANDLER_H

#include <signal.h>
#include <string>

using namespace std;

void setupCrashHandler();
void crashHandler( int sig );
string getStackTrace();

#endif