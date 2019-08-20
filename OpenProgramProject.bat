
@echo off

set "UE_Engine_Dir=..\..\..\..\Engine"

set "current_dir_name=%cd%"
:loop
set "current_dir_name=%current_dir_name:*\=%"
set "current_dir_nametmp=%current_dir_name:\=%"
if not "%current_dir_nametmp%"=="%current_dir_name%" goto loop

call GenerateProgramProject.bat

rem open project vs solution
echo open Intermediate\ProjectFiles\%current_dir_name%.vcxproj
"%UE_Engine_Dir%\Intermediate\ProjectFiles\%current_dir_name%.vcxproj"
