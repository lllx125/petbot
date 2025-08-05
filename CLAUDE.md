# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ESP32-based "Petbot" project that implements a simple UART echo server. The device receives messages over UART and echoes them back with an "Echo: " prefix. It uses the ESP-IDF framework for ESP32 development.

## Build System and Commands

This project uses ESP-IDF's build system based on CMake. Key commands:

```bash
# Set target chip (required before first build)
idf.py set-target esp32

# Build the project
idf.py build

# Flash to device (requires device connected via USB)
idf.py flash

# Monitor serial output
idf.py monitor

# Build and flash in one command
idf.py build flash

# Build, flash, and monitor
idf.py build flash monitor

# Clean build files
idf.py clean

# Full clean (delete entire build directory)
idf.py fullclean

# Configure project settings
idf.py menuconfig
```

## Code Architecture

### Project Structure
- `CMakeLists.txt` - Root CMake configuration using ESP-IDF project template
- `main/` - Main application directory
  - `main.c` - Entry point with `app_main()` function
  - `CMakeLists.txt` - Component configuration

### Core Application (`main/main.c`)
- **Entry Point**: `app_main()` function (ESP-IDF convention, not standard `main()`)
- **UART Configuration**: Uses UART_NUM_0 at 115200 baud for communication
- **Echo Logic**: Infinite loop that reads UART data and echoes it back with prefix
- **Logging**: Uses ESP-IDF logging system with "PETBOT_ECHO" tag
- **Task Management**: Uses FreeRTOS tasks and delays to prevent watchdog timeouts

### Key Constants
- `UART_NUM`: UART_NUM_0 (primary UART port)
- `BUF_SIZE`: 1024 bytes for UART buffer
- `TAG`: "PETBOT_ECHO" for logging

## ESP-IDF Specifics

- Target must be set before first build using `idf.py set-target <chip>`
- Uses FreeRTOS for task management
- ESP logging system for debug output
- UART driver for serial communication
- Standard ESP-IDF project structure with component registration