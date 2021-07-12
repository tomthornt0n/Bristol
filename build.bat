@echo off
set application_name=drawing_popup
set build_options= /DBUILD_WIN32=1 /DUNICODE /DAPPLICATION_NAME=%application_name%
set compile_flags= /nologo /Zi /FC /I ..
set link_flags= opengl32.lib /incremental:no /debug gdi32.lib user32.lib winmm.lib dxguid.lib Dinput8.lib

if not exist build mkdir build
pushd build
cl.exe %build_options% %compile_flags% ../win32_main.c /link %link_flags% /out:%application_name%.exe
popd