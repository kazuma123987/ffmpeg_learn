@echo off
cd ..
if exist build (
    del /q build\*
    for /d %%x in (build\*) do rmdir /s /q "%%x"
)
@REM if exist bin (
@REM     del /q bin\*
@REM     for /d %%x in (bin\*) do rmdir /s /q "%%x"
@REM )