CPP = cl.exe
CPP_OPTIONS = /nologo /EHsc /MD /O2 /D _CRT_SECURE_NO_WARNINGS /I ..\..\src
LIB_FILES = ..\..\src\http_server.cpp ..\..\src\sockets.cpp

main.exe : main.cpp
	$(CPP) /Femain.exe main.cpp $(LIB_FILES) $(CPP_OPTIONS) Ws2_32.lib
