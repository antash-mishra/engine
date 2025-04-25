#!/bin/bash

echo "Building Project"
cmake --build build

echo "Running Executable"
./build/game_engine
