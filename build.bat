@echo off
set application_name=Bristol
set build_options= /DBUILD_WIN32=1 /DUNICODE /DAPPLICATION_NAME=%application_name%
set compile_flags= /nologo /Zi /FC /I ..
set link_flags= /incremental:no /debug shell32.lib gdi32.lib user32.lib

if not exist build mkdir build
pushd build
cl.exe %build_options% %compile_flags% ../win32_main.c /link %link_flags% /out:%application_name%.exe
popd