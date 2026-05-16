cmake_minimum_required(VERSION 3.14)
project(FastTFM VERSION 3.0.0)

# The code requires C++17 (for std::filesystem, std::optional, etc.)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Automatically fetch the latest FTXUI (v5+)
include(FetchContent)
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
  GIT_TAG main 
)
FetchContent_MakeAvailable(ftxui)

# Target the exact file name provided
add_executable(fast-tfm fast-tfm.cpp)

# Link the required FTXUI libraries
target_link_libraries(fast-tfm
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE ftxui::component
)
