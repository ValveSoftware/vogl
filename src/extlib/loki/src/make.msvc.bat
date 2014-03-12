
cl -c -Zm200 -O2 -DNDEBUG -MT -EHsc -GR -W4 -I"." -I"..\include" OrderedStatic.cpp SafeFormat.cpp SmallObj.cpp SmartPtr.cpp Singleton.cpp StrongPtr.cpp

link /lib /NOLOGO /OUT:"..\lib\loki.lib" OrderedStatic.obj SafeFormat.obj SmallObj.obj SmartPtr.obj Singleton.obj StrongPtr.obj

del *.obj

