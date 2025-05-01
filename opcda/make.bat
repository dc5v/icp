@ECHO OFF
SETLOCAL EnableDelayedExpansion

SET IS_X64=0
SET EXEC=opcda-cli
SET BUILD_DIR=build
SET OBJ_DIR=build\obj
SET LIBS_DIR=build\libs
SET LOG_FILE=%BUILD_DIR%\build_log.txt
SET TIMESTAMP_DIR=%BUILD_DIR%\timestamps

SET BUILD_TOOL_DIR=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build

REM Create necessary directories
IF NOT EXIST %BUILD_DIR% MKDIR %BUILD_DIR%
IF NOT EXIST %OBJ_DIR% MKDIR %OBJ_DIR%
IF NOT EXIST %LIBS_DIR% MKDIR %LIBS_DIR%
IF NOT EXIST %TIMESTAMP_DIR% MKDIR %TIMESTAMP_DIR%

REM Process command line arguments
SET DLL_ONLY=0
SET REG_ONLY=0

FOR %%A IN (%*) DO (
    IF "%%A"=="-64" SET IS_X64=1
    IF "%%A"=="clean" GOTO :CLEAN
    IF "%%A"=="rebuild" GOTO :CLEAN
    IF "%%A"=="dll" SET DLL_ONLY=1
    IF "%%A"=="reg" SET REG_ONLY=1
)

REM Set up compilation environment
IF "%IS_X64%"=="1" (
    CALL "%BUILD_TOOL_DIR%\vcvars64.bat" > nul 2>&1
    SET OUTPUT_NAME="%BUILD_DIR%\%EXEC%.exe"
    SET OPC_LIB="libs\x64\opccomn_x64.lib" "libs\x64\opcda_x64.lib" "libs\x64\shlwapi_x64.lib"
    SET SRC_DLL_PATH="C:\Program Files\Common Files\OPC Foundation\Binary"
) ELSE (
    CALL "%BUILD_TOOL_DIR%\vcvars32.bat" > nul 2>&1
    SET OUTPUT_NAME="%BUILD_DIR%\%EXEC%-x86.exe"
    SET OPC_LIB="libs\x86\opccomn_x86.lib" "libs\x86\opcda_x86.lib" "libs\x86\shlwapi_x86.lib"
    SET SRC_DLL_PATH="C:\Program Files (x86)\Common Files\OPC Foundation\Binary"
)

REM Handle special modes
IF "%DLL_ONLY%"=="1" (
    CALL :PROCESS_DLLS
    GOTO :END
)

IF "%REG_ONLY%"=="1" (
    CALL :GENERATE_REGSVR_LIST
    GOTO :END
)

ECHO Build started at %TIME% > %LOG_FILE%

REM Compile all cpp files with incremental checking
SET NEED_LINK=0
SET COMPILE_COUNT=0
SET SKIP_COUNT=0

REM Get all cpp files
FOR %%F IN (*.cpp) DO (
    SET SRC_FILE=%%F
    SET OBJ_FILE=%OBJ_DIR%\%%~nF.obj
    SET TIMESTAMP_FILE=%TIMESTAMP_DIR%\%%~nF.timestamp
    
    REM Check if we need to compile this file
    SET NEED_COMPILE=0
    
    IF NOT EXIST !OBJ_FILE! (
        SET NEED_COMPILE=1
        ECHO Compiling !SRC_FILE! - object file does not exist >> %LOG_FILE%
    ) ELSE (
        IF NOT EXIST !TIMESTAMP_FILE! (
            SET NEED_COMPILE=1
            ECHO Compiling !SRC_FILE! - timestamp does not exist >> %LOG_FILE%
        ) ELSE (
            REM Check if source file is newer than timestamp
            FOR /F "tokens=2" %%T IN ('dir /A:-D /O:D /T:W "!SRC_FILE!" ^| findstr /V /C:"Directory" ^| findstr /R /C:"[0-9]"') DO SET SRC_DATE=%%T
            FOR /F "tokens=2" %%T IN ('dir /A:-D /O:D /T:W "!TIMESTAMP_FILE!" ^| findstr /V /C:"Directory" ^| findstr /R /C:"[0-9]"') DO SET TIMESTAMP_DATE=%%T
            
            IF "!SRC_DATE!" GTR "!TIMESTAMP_DATE!" (
                SET NEED_COMPILE=1
                ECHO Compiling !SRC_FILE! - source is newer than timestamp >> %LOG_FILE%
            )
            
            REM Check if header files were modified (simple implementation)
            FOR %%H IN (*.h *.hpp) DO (
                IF EXIST %%H (
                    FOR /F "tokens=2" %%T IN ('dir /A:-D /O:D /T:W "%%H" ^| findstr /V /C:"Directory" ^| findstr /R /C:"[0-9]"') DO SET HEADER_DATE=%%T
                    IF "!HEADER_DATE!" GTR "!TIMESTAMP_DATE!" (
                        SET NEED_COMPILE=1
                        ECHO Compiling !SRC_FILE! - header %%H is newer than timestamp >> %LOG_FILE%
                    )
                )
            )
        )
    )
    
    IF "!NEED_COMPILE!"=="1" (
        ECHO Compiling !SRC_FILE!
        cl /c /EHsc /MD /std:c++17 /W4 /D "_WINDOWS" /D "_CONSOLE" /D "_UNICODE" /D "UNICODE" ^
            /Zc:wchar_t /I"C:\Program Files (x86)\Common Files\OPC Foundation\Include" ^
            /I"C:\Program Files (x86)\Common Files\OPC Foundation\Include\opcda" /Fo"!OBJ_FILE!" ^
            !SRC_FILE! >> %LOG_FILE% 2>&1
            
        IF !ERRORLEVEL! NEQ 0 (
            ECHO Error compiling !SRC_FILE! - see %LOG_FILE% for details
            TYPE %LOG_FILE%
            EXIT /B 1
        )
        
        REM Update timestamp file
        ECHO Compiled on %DATE% %TIME% > "!TIMESTAMP_FILE!"
        
        SET NEED_LINK=1
        SET /A COMPILE_COUNT+=1
    ) ELSE (
        ECHO Skipping !SRC_FILE! - no changes detected
        SET /A SKIP_COUNT+=1
    )
)

ECHO.
ECHO Compilation summary: !COMPILE_COUNT! file(s) compiled, !SKIP_COUNT! file(s) skipped.

REM Link if needed
IF "!NEED_LINK!"=="1" (
    ECHO Linking...
    
    REM Get all object files
    SET OBJ_FILES=
    FOR %%F IN (*.cpp) DO (
        SET OBJ_FILES=!OBJ_FILES! "%OBJ_DIR%\%%~nF.obj"
    )
    
    link /OUT:%OUTPUT_NAME% !OBJ_FILES! %OPC_LIB% ole32.lib oleaut32.lib uuid.lib /NODEFAULTLIB:LIBCMT >> %LOG_FILE% 2>&1
    
    IF !ERRORLEVEL! NEQ 0 (
        ECHO Error linking - see %LOG_FILE% for details
        TYPE %LOG_FILE%
        EXIT /B 1
    )
    
    ECHO Linking successful.
    
    REM Process dependencies
    CALL :PROCESS_DLLS
    CALL :GENERATE_REGSVR_LIST
) ELSE (
    IF EXIST %OUTPUT_NAME% (
        ECHO No changes detected, using existing executable.
    ) ELSE (
        ECHO No object files have changed, but executable does not exist. Forcing link...
        
        REM Get all object files
        SET OBJ_FILES=
        FOR %%F IN (*.cpp) DO (
            SET OBJ_FILES=!OBJ_FILES! "%OBJ_DIR%\%%~nF.obj"
        )
        
        link /OUT:%OUTPUT_NAME% !OBJ_FILES! %OPC_LIB% ole32.lib oleaut32.lib uuid.lib /NODEFAULTLIB:LIBCMT >> %LOG_FILE% 2>&1
        
        IF !ERRORLEVEL! NEQ 0 (
            ECHO Error linking - see %LOG_FILE% for details
            TYPE %LOG_FILE%
            EXIT /B 1
        )
        
        ECHO Linking successful.
        
        REM Process dependencies
        CALL :PROCESS_DLLS
        CALL :GENERATE_REGSVR_LIST
    )
)

ECHO Build completed successfully.
GOTO :END

:CLEAN
ECHO Cleaning build directory...
IF EXIST %OBJ_DIR% RD /S /Q %OBJ_DIR%
IF EXIST %TIMESTAMP_DIR% RD /S /Q %TIMESTAMP_DIR%
IF EXIST %BUILD_DIR%\%EXEC%.exe DEL /F /Q %BUILD_DIR%\%EXEC%.exe
IF EXIST %BUILD_DIR%\%EXEC%-x86.exe DEL /F /Q %BUILD_DIR%\%EXEC%-x86.exe
IF EXIST %LOG_FILE% DEL /F /Q %LOG_FILE%

MKDIR %OBJ_DIR%
MKDIR %TIMESTAMP_DIR%

IF "%1"=="rebuild" (
    ECHO Rebuilding...
    GOTO :MAIN
) ELSE (
    ECHO Clean completed.
    GOTO :END
)

:PROCESS_DLLS
ECHO Processing DLL dependencies...

REM Copy required DLLs and TLB files to LIBS_DIR
IF EXIST %SRC_DLL_PATH% (
    FOR %%F IN (%SRC_DLL_PATH%\*.dll %SRC_DLL_PATH%\*.tlb) DO (
        SET SRC_FILE=%%F
        SET DEST_FILE=%LIBS_DIR%\%%~nxF
        
        REM Check version and update if needed
        CALL :COPY_IF_NEWER "!SRC_FILE!" "!DEST_FILE!"
    )
    ECHO DLL and TLB files copied to %LIBS_DIR%
) ELSE (
    ECHO Source DLL path %SRC_DLL_PATH% not found
)

EXIT /B 0

:GENERATE_REGSVR_LIST
ECHO Generating regsvr32 list...

SET REGSVR_LIST=%LIBS_DIR%\regsvr32.txt

> %REGSVR_LIST% (
    ECHO # Files requiring regsvr32 registration
    ECHO # Generated: %DATE% %TIME%
    ECHO # 
    ECHO # To register these files, use:
    ECHO #   regsvr32 [filename]
    ECHO #
    ECHO # To unregister:
    ECHO #   regsvr32 /u [filename]
    ECHO #
    ECHO.
)

FOR %%F IN (%LIBS_DIR%\*.dll) DO (
    DUMPBIN /EXPORTS "%%F" | FINDSTR /C:"DllRegisterServer" /C:"DllUnregisterServer" > NUL
    IF !ERRORLEVEL! EQU 0 (
        ECHO %%~nxF >> %REGSVR_LIST%
    )
)

ECHO Registration list saved to %REGSVR_LIST%
EXIT /B 0

:COPY_IF_NEWER
SET SRC_FILE=%~1
SET DEST_FILE=%~2

IF NOT EXIST "%DEST_FILE%" (
    ECHO Copying %~nx1 to %LIBS_DIR%
    COPY /Y "%SRC_FILE%" "%DEST_FILE%" > nul
    EXIT /B
)

REM Get file versions
FOR /F "tokens=2 delims==" %%V IN ('wmic datafile where name^="%SRC_FILE:\=\\%" get Version /value 2^>nul') DO (
    SET SRC_VER=%%V
)

FOR /F "tokens=2 delims==" %%V IN ('wmic datafile where name^="%DEST_FILE:\=\\%" get Version /value 2^>nul') DO (
    SET DEST_VER=%%V
)

IF "!SRC_VER!"=="" (
    REM No version info, use file date instead
    FOR /F "tokens=2" %%T IN ('dir /A:-D /O:D /T:W "%SRC_FILE%" ^| findstr /V /C:"Directory" ^| findstr /R /C:"[0-9]"') DO SET SRC_DATE=%%T
    FOR /F "tokens=2" %%T IN ('dir /A:-D /O:D /T:W "%DEST_FILE%" ^| findstr /V /C:"Directory" ^| findstr /R /C:"[0-9]"') DO SET DEST_DATE=%%T
    
    IF "!SRC_DATE!" GTR "!DEST_DATE!" (
        ECHO Copying newer version of %~nx1 (based on date)
        COPY /Y "%SRC_FILE%" "%DEST_FILE%" > nul
    )
) ELSE (
    IF "!SRC_VER!" GTR "!DEST_VER!" (
        ECHO Copying newer version of %~nx1: !SRC_VER! ^> !DEST_VER!
        COPY /Y "%SRC_FILE%" "%DEST_FILE%" > nul
    )
)
EXIT /B

:END
ENDLOCAL