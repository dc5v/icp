@ECHO OFF

SETLOCAL EnableDelayedExpansion

SET IS_X64=0
SET EXEC=opcda-cli
SET BUILD_DIR=build
SET OBJ_DIR=build\obj
SET LIBS_DIR=build\libs

SET BUILD_TOOL_DIR=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build

IF NOT EXIST %BUILD_DIR% (
    MKDIR %BUILD_DIR%
    ECHO Created %BUILD_DIR% directory
)

IF NOT EXIST %OBJ_DIR% (
    MKDIR %OBJ_DIR%
    ECHO Created %OBJ_DIR% directory
)

IF NOT EXIST %LIBS_DIR% (
    MKDIR %LIBS_DIR%
    ECHO Created %LIBS_DIR% directory
)

FOR %%A IN (%*) DO (
    IF "%%A"=="-64" (
        SET IS_X64=1
    )
)

IF "%IS_X64%"=="1" (
    CALL "%BUILD_TOOL_DIR%\vcvars64.bat" > nul 2>&1
    SET OUTPUT_NAME="%BUILD_DIR%\%EXEC%.exe"
    SET OPC_LIB="libs\x64\opccomn_x64.lib" "libs\x64\opcda_x64.lib" "libs\x64\shlwapi_x64.lib"
    SET PLATFORM=x64
    SET SRC_DLL_PATH="C:\Program Files\Common Files\OPC Foundation\Binary"
) ELSE (
    CALL "%BUILD_TOOL_DIR%\vcvars32.bat" > nul 2>&1
    SET OUTPUT_NAME="%BUILD_DIR%\%EXEC%-x86.exe"
    SET OPC_LIB="libs\x86\opccomn_x86.lib" "libs\x86\opcda_x86.lib" "libs\x86\shlwapi_x86.lib"
    SET PLATFORM=x86
    SET SRC_DLL_PATH="C:\Program Files (x86)\Common Files\OPC Foundation\Binary"
)

ECHO BUILD

cl /EHsc /MD /std:c++17 /D "_WINDOWS" /D "_CONSOLE" /D "_UNICODE" /D "UNICODE" ^
    /Zc:wchar_t /I"C:\Program Files (x86)\Common Files\OPC Foundation\Include" ^
    /I"C:\Program Files (x86)\Common Files\OPC Foundation\Include\opcda" /Fo"%OBJ_DIR%\\" ^
    *.cpp /link %OPC_LIB% ole32.lib oleaut32.lib uuid.lib /NODEFAULTLIB:LIBCMT /OUT:%OUTPUT_NAME%

IF %ERRORLEVEL% EQU 0 (
    ECHO [O] BUILD SUCCESS
    
    ECHO Preparing dependency files for distribution...
    
    REM List of OPC DLLs needed for deployment
    SET DLLS_TO_COPY=opccomn_ps.dll opcproxy.dll opcenum.dll opcda_ps.dll
    
    REM Create manifest for Registration-Free COM
    ECHO Creating Registration-Free COM manifest...
    > "%BUILD_DIR%\%EXEC%.manifest" (
        ECHO ^<?xml version="1.0" encoding="UTF-8" standalone="yes"?^>
        ECHO ^<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0"^>
        ECHO   ^<assemblyIdentity type="win32" name="%EXEC%" version="1.0.0.0" /^>
        FOR %%D IN (%DLLS_TO_COPY%) DO (
            ECHO   ^<file name="%%D"^>
            ECHO     ^<comClass description="OPC Foundation COM Class" clsid="*" threadingModel="Apartment" /^>
            ECHO   ^</file^>
        )
        ECHO ^</assembly^>
    )
    
    REM Copy required DLLs and type libraries
    ECHO Copying required DLLs and type libraries to %LIBS_DIR%...
    
    FOR %%D IN (%DLLS_TO_COPY%) DO (
        CALL :CopyFileWithVersionCheck "%%D"
    )
    
    REM Add TLB files if needed
    CALL :CopyFileWithVersionCheck "opcda.tlb"
    
    REM Apply manifest to executable
    IF EXIST "%BUILD_DIR%\%EXEC%.manifest" (
        mt.exe -manifest "%BUILD_DIR%\%EXEC%.manifest" -outputresource:%OUTPUT_NAME%;1
        IF %ERRORLEVEL% EQU 0 (
            ECHO [O] Manifest applied successfully
        ) ELSE (
            ECHO [X] Failed to apply manifest
        )
    )
    
) ELSE (
    ECHO [X] BUILD FAILED 
)

GOTO :END

:CopyFileWithVersionCheck
SET FILE_NAME=%~1
SET SRC_FILE=%SRC_DLL_PATH%\%FILE_NAME%
SET DEST_FILE=%LIBS_DIR%\%FILE_NAME%

IF NOT EXIST "%SRC_FILE%" (
    ECHO [!] Source file not found: %SRC_FILE%
    GOTO :EOF
)

SET COPY_FILE=1

IF EXIST "%DEST_FILE%" (
    FOR /F "tokens=2 delims==" %%I IN ('wmic datafile where name^="%DEST_FILE:\=\\%" get Version /value') DO (
        SET DEST_VERSION=%%I
    )
    
    FOR /F "tokens=2 delims==" %%I IN ('wmic datafile where name^="%SRC_FILE:\=\\%" get Version /value') DO (
        SET SRC_VERSION=%%I
    )
    
    IF NOT "!SRC_VERSION!"=="" (
        IF NOT "!DEST_VERSION!"=="" (
            ECHO Comparing versions for %FILE_NAME%: !SRC_VERSION! vs !DEST_VERSION!
            
            FOR /F "tokens=1-4 delims=." %%A IN ("!SRC_VERSION!") DO (
                SET SRC_MAJOR=%%A
                SET SRC_MINOR=%%B
                SET SRC_BUILD=%%C
                SET SRC_REVISION=%%D
            )
            
            FOR /F "tokens=1-4 delims=." %%A IN ("!DEST_VERSION!") DO (
                SET DEST_MAJOR=%%A
                SET DEST_MINOR=%%B
                SET DEST_BUILD=%%C
                SET DEST_REVISION=%%D
            )
            
            REM Compare versions
            IF !SRC_MAJOR! GTR !DEST_MAJOR! SET COPY_FILE=1 & GOTO :DO_COPY
            IF !SRC_MAJOR! EQU !DEST_MAJOR! (
                IF !SRC_MINOR! GTR !DEST_MINOR! SET COPY_FILE=1 & GOTO :DO_COPY
                IF !SRC_MINOR! EQU !DEST_MINOR! (
                    IF !SRC_BUILD! GTR !DEST_BUILD! SET COPY_FILE=1 & GOTO :DO_COPY
                    IF !SRC_BUILD! EQU !DEST_BUILD! (
                        IF !SRC_REVISION! GTR !DEST_REVISION! SET COPY_FILE=1 & GOTO :DO_COPY
                    )
                )
            )
            SET COPY_FILE=0
        )
    ) ELSE (
        REM For files without version info, check file date
        FOR /F "tokens=1-2" %%D IN ('dir /T:W "%SRC_FILE%" ^| findstr "%FILE_NAME%"') DO SET SRC_DATE=%%D %%E
        FOR /F "tokens=1-2" %%D IN ('dir /T:W "%DEST_FILE%" ^| findstr "%FILE_NAME%"') DO SET DEST_DATE=%%D %%E
        
        IF "!SRC_DATE!" NEQ "!DEST_DATE!" SET COPY_FILE=1
    )
)

:DO_COPY
IF "!COPY_FILE!"=="1" (
    ECHO Copying newer version of %FILE_NAME% to %LIBS_DIR%
    COPY /Y "%SRC_FILE%" "%DEST_FILE%" > nul
    IF %ERRORLEVEL% EQU 0 (
        ECHO [O] Successfully copied %FILE_NAME%
    ) ELSE (
        ECHO [X] Failed to copy %FILE_NAME%
    )
) ELSE (
    ECHO [i] Existing version of %FILE_NAME% is up to date
)

GOTO :EOF

:END
ENDLOCAL
