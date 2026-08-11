@ECHO OFF
g++ -std=c++17 main.cpp -lsqlite3 -o exchange.exe
IF ERRORLEVEL 1 EXIT /B 1
exchange.exe
