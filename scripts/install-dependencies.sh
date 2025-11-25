#!/bin/bash
#
# MXRC Project - Dependency Installation Script
#
# This script installs all required and optional dependencies for the MXRC project.
# Tested on: Ubuntu 24.04 LTS
#
# Usage:
#   sudo ./scripts/install-dependencies.sh [options]
#
# Options:
#   --all           Install all dependencies (required + optional)
#   --required      Install only required dependencies
#   --optional      Install only optional dependencies
#   --help          Show this help message
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print functions
print_header() {
    echo -e "${BLUE}================================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}================================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    print_error "Please run as root (use sudo)"
    exit 1
fi

# Parse arguments
INSTALL_REQUIRED=false
INSTALL_OPTIONAL=false

if [ $# -eq 0 ]; then
    print_warning "No options specified. Use --help for usage information."
    exit 1
fi

while [[ $# -gt 0 ]]; do
    case $1 in
        --all)
            INSTALL_REQUIRED=true
            INSTALL_OPTIONAL=true
            shift
            ;;
        --required)
            INSTALL_REQUIRED=true
            shift
            ;;
        --optional)
            INSTALL_OPTIONAL=true
            shift
            ;;
        --help)
            grep "^#" "$0" | grep -v "#!/bin/bash" | sed 's/^# //g' | sed 's/^#//g'
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            echo "Use --help for usage information."
            exit 1
            ;;
    esac
done

# Update package list
print_header "Updating package list"
apt-get update
print_success "Package list updated"

# ============================================================================
# REQUIRED DEPENDENCIES
# ============================================================================

if [ "$INSTALL_REQUIRED" = true ]; then
    print_header "Installing REQUIRED dependencies"

    print_info "Installing build tools..."
    apt-get install -y \
        build-essential \
        g++ \
        cmake \
        git \
        pkg-config \
        python3 \
        python3-pip
    print_success "Build tools installed"

    print_info "Installing C++ libraries..."
    apt-get install -y \
        libspdlog-dev \
        libgtest-dev \
        libtbb-dev \
        nlohmann-json3-dev \
        libyaml-cpp-dev \
        libsystemd-dev
    print_success "C++ libraries installed"

    print_info "Installing Python packages..."
    pip3 install --break-system-packages jinja2 pyyaml
    print_success "Python packages installed"

    print_success "All REQUIRED dependencies installed successfully!"
fi

# ============================================================================
# OPTIONAL DEPENDENCIES
# ============================================================================

if [ "$INSTALL_OPTIONAL" = true ]; then
    print_header "Installing OPTIONAL dependencies"

    # Boost (for lock-free data structures)
    print_info "Installing Boost..."
    apt-get install -y libboost-all-dev
    print_success "Boost installed"

    # NUMA (for performance optimization)
    print_info "Installing NUMA..."
    apt-get install -y libnuma-dev
    print_success "NUMA installed"

    # Google Benchmark (from source - not in Ubuntu 24.04 repos)
    print_info "Installing Google Benchmark from source..."
    if [ ! -d "/tmp/benchmark" ]; then
        cd /tmp
        git clone https://github.com/google/benchmark.git
        cd benchmark
        cmake -E make_directory "build"
        cmake -E chdir "build" cmake -DBENCHMARK_DOWNLOAD_DEPENDENCIES=on -DCMAKE_BUILD_TYPE=Release ../
        cmake --build "build" --config Release
        cmake --build "build" --config Release --target install
        cd /tmp
        rm -rf benchmark
        print_success "Google Benchmark installed"
    else
        print_warning "Google Benchmark directory already exists, skipping..."
    fi

    # Folly dependencies
    print_info "Installing Folly dependencies..."
    apt-get install -y \
        libevent-dev \
        libdouble-conversion-dev \
        libgoogle-glog-dev \
        libgflags-dev \
        libiberty-dev \
        liblz4-dev \
        liblzma-dev \
        libsnappy-dev \
        zlib1g-dev \
        binutils-dev \
        libjemalloc-dev \
        libssl-dev \
        libunwind-dev \
        libfmt-dev \
        libsodium-dev
    print_success "Folly dependencies installed"

    # Folly (from source)
    print_info "Installing Folly from source (this may take 10-20 minutes)..."
    if [ ! -d "/tmp/folly" ]; then
        cd /tmp
        git clone https://github.com/facebook/folly.git
        cd folly
        mkdir -p _build && cd _build
        cmake ..
        make -j$(nproc)
        make install
        ldconfig
        cd /tmp
        rm -rf folly
        print_success "Folly installed"
    else
        print_warning "Folly directory already exists, skipping..."
    fi

    # prometheus-cpp (from source)
    print_info "Installing prometheus-cpp from source..."
    if [ ! -d "/tmp/prometheus-cpp" ]; then
        cd /tmp
        git clone --recursive https://github.com/jupp0r/prometheus-cpp.git
        cd prometheus-cpp
        mkdir -p _build && cd _build
        cmake .. -DBUILD_SHARED_LIBS=ON
        make -j$(nproc)
        make install
        ldconfig
        cd /tmp
        rm -rf prometheus-cpp
        print_success "prometheus-cpp installed"
    else
        print_warning "prometheus-cpp directory already exists, skipping..."
    fi

    # CivetWeb
    print_info "Installing CivetWeb..."
    apt-get install -y libcivetweb-dev || {
        print_warning "CivetWeb not in repos, installing from source..."
        cd /tmp
        git clone https://github.com/civetweb/civetweb.git
        cd civetweb
        make build
        make install
        ldconfig
        cd /tmp
        rm -rf civetweb
        print_success "CivetWeb installed from source"
    }

    print_success "All OPTIONAL dependencies installed successfully!"
fi

# ============================================================================
# POST-INSTALLATION
# ============================================================================

print_header "Post-installation cleanup"
ldconfig
print_success "Library cache updated"

# Print summary
print_header "Installation Summary"
if [ "$INSTALL_REQUIRED" = true ]; then
    print_success "Required dependencies: INSTALLED"
fi
if [ "$INSTALL_OPTIONAL" = true ]; then
    print_success "Optional dependencies: INSTALLED"
fi

echo ""
print_info "Next steps:"
echo "  1. Navigate to your build directory: cd build"
echo "  2. Reconfigure CMake: cmake .."
echo "  3. Build the project: make -j\$(nproc)"
echo "  4. Run tests: ./run_tests"
echo ""
print_success "Installation complete!"
