INCLUDE=-I"./glslang/glslang/MachineIndependent/" \
        -I"./glslang/glslang/" \
        -I"./glslang/OGLCompilersDLL/" \
        -I"./glslang/glslang/OSDependent/Unix/" \
        -I"./glslang/glslang/OSDependent/Windows/"

LIBPATH=-L"./glslang/build/" \
        -L"./glslang/build/OSDependent/Unix/" \
        -L"./glslang/OGLCompilersDLL/"

LIBS=-lglslang -lOSDependent -lOGLCompiler -lpthread

SRC=ShaderTune.cpp ShaderParser.cpp CostCalculator.cpp

WARN=-Wall -Wno-sign-compare

all:
	g++ $(WARN) -O0 -g -std=c++11 -o shadertune $(INCLUDE) $(SRC) $(LIBPATH) $(LIBS)
