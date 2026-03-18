CXX      := g++
CXXFLAGS := -std=c++17 -O2
LDFLAGS  := -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
GLSLC    := glslc

SRC := $(wildcard src/*.cpp)
HDR := $(wildcard include/*.hpp)
TARGET := VulkanTest

VERT_SRC := shaders/shader.vert
FRAG_SRC := shaders/shader.frag
VERT_SPV := shaders/shader.vert.spv
FRAG_SPV := shaders/shader.frag.spv
SHADERS  := $(VERT_SPV) $(FRAG_SPV)

.PHONY: all test clean shaders

all: $(TARGET) shaders test

$(VERT_SPV): $(VERT_SRC)
	$(GLSLC) $< -o $@

$(FRAG_SPV): $(FRAG_SRC)
	$(GLSLC) $< -o $@

$(TARGET): $(SRC) $(HDR) $(SHADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(LDFLAGS)

shaders: $(SHADERS)

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(SHADERS)
