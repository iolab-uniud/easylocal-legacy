# DetectChronoFeatures.cmake
# A helper module to detect chrono features.

include(CheckCXXSourceCompiles)

# Check for `std::chrono::steady_clock`
check_cxx_source_compiles("
#include <chrono>
int main() {
    std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::high_resolution_clock::now();
    return 0;
}" HAS_STEADY_CLOCK)

# Check for `std::chrono::system_clock`
check_cxx_source_compiles("
#include <chrono>
int main() {
    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::high_resolution_clock::now();
    return 0;
}" HAS_SYSTEM_CLOCK)

# Check for `auto` usage in chrono
check_cxx_source_compiles("
#include <chrono>
int main() {
    auto start = std::chrono::high_resolution_clock::now();
    return 0;
}" HAS_AUTO_CLOCK)

# Configure the feature detection header file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/config.hh.in
    ${CMAKE_CURRENT_SOURCE_DIR}/include/config.hh
)
