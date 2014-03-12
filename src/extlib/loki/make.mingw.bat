@ECHO OFF
set LOKI_TMP=%1
if defined LOKI_TMP (
	set PATH=%~1;%PATH%
)
set LOKI_TMP=
@ECHO ON

mingw32-make check OS=Windows

