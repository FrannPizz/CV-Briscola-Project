cmake_minimum_required(VERSION 3.10)
project(BriscolaRecognizer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# sovrascrivibile da riga di comando:  cmake -DOpenCV_DIR=<path> ..
set(OpenCV_DIR "C:/msys64/ucrt64/lib/cmake/opencv4" CACHE PATH "Cartella di OpenCVConfig.cmake")

find_package(OpenCV REQUIRED)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

# Eseguibile unico, con sotto-comandi: game | round | frame | test
add_executable(main
    src/main.cpp
    # visione: rilevamento, rettifica, riconoscimento
    src/CardDetector.cpp
    src/CardRectifier.cpp
    src/CardRecognizer.cpp
    # gioco: regole, analisi temporale del round, output, metriche
    src/Card.cpp
    src/GameRules.cpp
    src/RoundAnalyzer.cpp
    src/GameReport.cpp
    src/Metrics.cpp
    src/test_rules.cpp
)

target_link_libraries(main ${OpenCV_LIBS})
