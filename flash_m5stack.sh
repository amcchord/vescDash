#!/bin/bash

# M5Stack Core2 Automated Flashing Script
# This script automates the process of building and flashing code to M5Stack Core2

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to detect M5Stack Core2 port
detect_port() {
    print_status "Detecting M5Stack Core2 port..."
    
    # Common port patterns for M5Stack Core2
    POSSIBLE_PORTS=(
        "/dev/tty.usbserial-*"
        "/dev/tty.SLAB_USBtoUART"
        "/dev/cu.usbserial-*"
        "/dev/cu.SLAB_USBtoUART"
        "/dev/ttyUSB*"
        "/dev/ttyACM*"
    )
    
    DETECTED_PORT=""
    
    for pattern in "${POSSIBLE_PORTS[@]}"; do
        for port in $pattern; do
            if [ -e "$port" ]; then
                DETECTED_PORT="$port"
                print_success "Found M5Stack Core2 at: $DETECTED_PORT"
                return 0
            fi
        done
    done
    
    return 1
}

# Function to install PlatformIO if not present
install_platformio() {
    if ! command_exists pio; then
        print_status "PlatformIO not found. Installing PlatformIO..."
        
        if command_exists pip3; then
            pip3 install platformio
        elif command_exists pip; then
            pip install platformio
        else
            print_error "Neither pip nor pip3 found. Please install Python and pip first."
            exit 1
        fi
        
        # Add PlatformIO to PATH if needed
        if ! command_exists pio; then
            export PATH="$PATH:$HOME/.local/bin"
        fi
        
        if command_exists pio; then
            print_success "PlatformIO installed successfully"
        else
            print_error "Failed to install PlatformIO"
            exit 1
        fi
    else
        print_success "PlatformIO is already installed"
    fi
}

# Function to check project structure
check_project_structure() {
    print_status "Checking project structure..."
    
    if [ ! -f "platformio.ini" ]; then
        print_error "platformio.ini not found. Make sure you're in the correct project directory."
        exit 1
    fi
    
    if [ ! -d "src" ]; then
        print_error "src directory not found. Creating src directory..."
        mkdir -p src
    fi
    
    if [ ! -f "src/main.cpp" ]; then
        print_error "src/main.cpp not found. Please ensure your code is in src/main.cpp"
        exit 1
    fi
    
    print_success "Project structure is valid"
}

# Function to build the project
build_project() {
    print_status "Building project for M5Stack Core2..."
    
    if pio run -e m5stack-core2; then
        print_success "Build completed successfully"
        return 0
    else
        print_error "Build failed"
        return 1
    fi
}

# Function to flash the device with retry logic
flash_device() {
    local port="$1"
    local upload_speeds=(115200 460800 230400 57600)
    
    print_status "Flashing M5Stack Core2 on port: $port"
    
    # First attempt with current configuration
    if [ -n "$port" ]; then
        if pio run -e m5stack-core2 --target upload --upload-port "$port"; then
            print_success "Flash completed successfully"
            return 0
        fi
    else
        if pio run -e m5stack-core2 --target upload; then
            print_success "Flash completed successfully"
            return 0
        fi
    fi
    
    print_warning "Initial flash attempt failed. Trying with different upload speeds..."
    
    # Try different upload speeds
    for speed in "${upload_speeds[@]}"; do
        print_status "Attempting flash with upload speed: $speed baud"
        
        if [ -n "$port" ]; then
            if pio run -e m5stack-core2 --target upload --upload-port "$port" --upload-option="--baud=$speed"; then
                print_success "Flash completed successfully at $speed baud"
                print_status "Consider updating platformio.ini with upload_speed = $speed for future flashes"
                return 0
            fi
        else
            if pio run -e m5stack-core2 --target upload --upload-option="--baud=$speed"; then
                print_success "Flash completed successfully at $speed baud"
                print_status "Consider updating platformio.ini with upload_speed = $speed for future flashes"
                return 0
            fi
        fi
        
        print_warning "Flash failed at $speed baud, trying next speed..."
        sleep 1
    done
    
    print_error "Flash failed at all attempted speeds"
    print_error "Manual troubleshooting required - see suggestions below:"
    echo
    echo "Troubleshooting steps:"
    echo "1. Hold the RESET button while flashing"
    echo "2. Try a different USB cable (data cable, not charge-only)"
    echo "3. Try a different USB port"
    echo "4. Install CP2104 USB driver if not already installed"
    echo "5. Check that no other programs are using the serial port"
    echo
    return 1
}

# Function to open serial monitor
open_monitor() {
    local port="$1"
    
    print_status "Opening serial monitor..."
    
    if [ -n "$port" ]; then
        print_status "Serial monitor starting on $port (Ctrl+C to exit)"
        pio device monitor --port "$port" --baud 115200
    else
        print_status "Serial monitor starting with auto-detection (Ctrl+C to exit)"
        pio device monitor --baud 115200
    fi
}

# Main execution
main() {
    echo "======================================"
    echo "M5Stack Core2 Automated Flash Script"
    echo "======================================"
    echo
    
    # Parse command line arguments
    SKIP_BUILD=false
    MONITOR=false
    SPECIFIED_PORT=""
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --skip-build)
                SKIP_BUILD=true
                shift
                ;;
            --monitor)
                MONITOR=true
                shift
                ;;
            --port)
                SPECIFIED_PORT="$2"
                shift 2
                ;;
            --help)
                echo "Usage: $0 [OPTIONS]"
                echo ""
                echo "Options:"
                echo "  --skip-build    Skip the build step and only flash"
                echo "  --monitor       Open serial monitor after flashing"
                echo "  --port PORT     Specify the serial port manually"
                echo "  --help          Show this help message"
                echo ""
                echo "Examples:"
                echo "  $0                                    # Build and flash"
                echo "  $0 --monitor                          # Build, flash, and monitor"
                echo "  $0 --port /dev/tty.usbserial-1234    # Use specific port"
                echo "  $0 --skip-build --monitor             # Only flash and monitor"
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                echo "Use --help for usage information"
                exit 1
                ;;
        esac
    done
    
    # Step 1: Install PlatformIO if needed
    install_platformio
    
    # Step 2: Check project structure
    check_project_structure
    
    # Step 3: Detect port if not specified
    if [ -z "$SPECIFIED_PORT" ]; then
        if detect_port; then
            SPECIFIED_PORT="$DETECTED_PORT"
        else
            print_warning "Could not auto-detect M5Stack Core2 port. Will try auto-detection during flash."
        fi
    else
        if [ -e "$SPECIFIED_PORT" ]; then
            print_success "Using specified port: $SPECIFIED_PORT"
        else
            print_error "Specified port does not exist: $SPECIFIED_PORT"
            exit 1
        fi
    fi
    
    # Step 4: Build project (unless skipped)
    if [ "$SKIP_BUILD" = false ]; then
        if ! build_project; then
            exit 1
        fi
    else
        print_warning "Skipping build step as requested"
    fi
    
    # Step 5: Flash device
    if ! flash_device "$SPECIFIED_PORT"; then
        exit 1
    fi
    
    print_success "M5Stack Core2 flashing completed successfully!"
    
    # Step 6: Open monitor if requested
    if [ "$MONITOR" = true ]; then
        echo
        open_monitor "$SPECIFIED_PORT"
    fi
}

# Run main function with all arguments
main "$@"
