@echo off

if "%1" NEQ "" (
    goto :munge %1
)

call munge_fonts Segoe
call munge_fonts Cascadia
call munge_fonts Digits
call munge_fonts Big
call munge_fonts Forte

goto :exit

:munge
echo %1
..\..\FontParser\bin\Debug\net8.0-windows\FontParser.exe ..\components\assets\font\%1.bitmapfont ..\components\assets\%1.c ..\components\assets\include\font\%1.h

:exit
