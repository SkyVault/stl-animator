all: build

build:
    g++ -std=c++17 main.cpp -Iraylib/src/ -Iraylib/src/external -lGL -lm -pthread -ldl -lrt -lX11 -lraylib -o stl-animator
