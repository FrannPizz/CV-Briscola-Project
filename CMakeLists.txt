cmake_minimum_required(VERSION 3.10)
project(BriscolaRecognizer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OpenCV_DIR "C:/msys64/ucrt64/lib/cmake/opencv4")

find_package(OpenCV REQUIRED)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

add_executable(main
    src/main.cpp
    src/CardDetector.cpp
    src/CardRectifier.cpp
    src/CardRecognizer.cpp
)

target_link_libraries(main ${OpenCV_LIBS})
