
cl -c -Zm200 -O2 -DNDEBUG -MT -D_WINDLL -DLOKI_MAKE_DLL -EHsc -GR -W4 -wd4251 -I"." -I"..\include" OrderedStatic.cpp SafeFormat.cpp SmallObj.cpp SmartPtr.cpp Singleton.cpp StrongPtr.cpp

link /NOLOGO /DLL /OUT:"..\lib\loki015.dll" OrderedStatic.obj SafeFormat.obj SmallObj.obj SmartPtr.obj Singleton.obj StrongPtr.obj

del *.obj

