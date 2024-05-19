#!/bin/bash


CXX=g++
CXXFLAGS="-std=c++11 -lpthread -Wall -Wextra"


SERVER_SRC="server.cpp"
READER_SRC="reader_client.cpp"
WRITER_SRC="writer_client.cpp"
MONITOR_SRC="monitor_client.cpp"


SERVER_BIN="server"
READER_BIN="reader_client"
WRITER_BIN="writer_client"
MONITOR_BIN="monitor_client"


echo "Compiling server..."
$CXX $CXXFLAGS $SERVER_SRC -o $SERVER_BIN
if [ $? -eq 0 ]; then
    echo "Server compiled successfully."
else
    echo "Failed to compile server."
    exit 1
fi


echo "Compiling reader client..."
$CXX $CXXFLAGS $READER_SRC -o $READER_BIN
if [ $? -eq 0 ]; then
    echo "Reader client compiled successfully."
else
    echo "Failed to compile reader client."
    exit 1
fi


echo "Compiling writer client..."
$CXX $CXXFLAGS $WRITER_SRC -o $WRITER_BIN
if [ $? -eq 0 ]; then
    echo "Writer client compiled successfully."
else
    echo "Failed to compile writer client."
    exit 1
fi

echo "Compiling monitor client..."
$CXX $CXXFLAGS $MONITOR_SRC -o $MONITOR_BIN
if [ $? -eq 0 ]; then
    echo "Monitor client compiled successfully."
else
    echo "Failed to compile monitor client."
    exit 1
fi

echo "All programs compiled successfully."
