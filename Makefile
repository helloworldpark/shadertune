INCLUDE=-I"./glslang/glslang/MachineIndependent/" \
        -I"./glslang/glslang/" \
        -I"./glslang/OGLCompilersDLL/" \
        -I"./glslang/glslang/OSDependent/Linux/" \
        -I"./glslang/glslang/OSDependent/Windows/"

LIBPATH=-L"./glslang/glslang/" \
				-L"./glslang/glslang/OSDependent/Linux/" \
				-L"./glslang/OGLCompilersDLL/"

LIBS=-lglslang -lOSDependent -lOGLCompiler -lpthread

SRC=ShaderTune.cpp ShaderParser.cpp CostCalculator.cpp

WARN=-Wall -Wno-sign-compare

all:
	g++ $(WARN) -O0 -g -o shadertune $(INCLUDE) $(SRC) $(LIBPATH) $(LIBS)
