@ECHO OFF

if defined LOKI_TMP (
	goto STARTCOMPILING
)

:: Viual C++ 7.1
if defined VS71COMNTOOLS  (
	if exist "%VS71COMNTOOLS%\vsvars32.bat" (
		echo -
		echo - Visual C++ 7.1 found.
		echo -
		call "%VS71COMNTOOLS%\vsvars32.bat"
		set LOKI_TMP=true
		goto STARTCOMPILING
	)
)

:: Viual C++ 8.0
if defined VS80COMNTOOLS (
	if exist "%VS80COMNTOOLS%\vsvars32.bat" (
		echo -
		echo - Visual C++ 8.0 found.
		echo -
		call "%VS80COMNTOOLS%\vsvars32.bat"
		set LOKI_TMP=true
		goto STARTCOMPILING
	)
)

:: Toolkit 2003
if defined VCToolkitInstallDir (
	if exist "%VCToolkitInstallDir%\vcvars32.bat" (
		echo -
		echo - VC 7.1 Toolkit found.
		echo -
		call "%VCToolkitInstallDir%\vcvars32.bat"
		set LOKI_TMP=true
		set LOKI_MSVC_NOLIB=true
		goto STARTCOMPILING
	)
)


echo -
echo - No Visual C++ found, please set the enviroment variable 
echo - 
echo - VCToolkitInstallDir  or  VS71COMNTOOLS or VS80COMNTOOLS 
echo - 
echo - to your Visual Studio folder which contains vsvars32.bat.
echo - 
echo - Or call the vsvars32.bat.
echo -

goto ERROR
		


:STARTCOMPILING


:: buid process

if defined LOKI_TMP (


@ECHO ON

cd src
call make.msvc.bat
call make.msvc.dll.bat
cd ..

cd test
call make.msvc.bat
cd ..

goto LEAVE

)

:ERROR
echo -
echo -
echo - An error occured. Compiling aborted.
echo - 
pause



:LEAVE

