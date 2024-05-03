@echo off

if "%1" NEQ "" (
    goto :munge %1
)

call munge_fonts Segoe
call munge_fonts Cascadia
call munge_fonts Digits

goto :exit

:munge
echo %1
FontParser.exe ..\components\assets\font\%1.bitmapfont ..\components\assets\%1.c ..\components\assets\include\font\%1.h

:exit
