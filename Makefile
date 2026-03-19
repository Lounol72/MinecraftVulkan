CXX      := g++
LIBS_INCLUDES := $(patsubst %,-I%,$(wildcard libs/*))
CXXFLAGS := -std=c++17 -O2 -Iinclude $(LIBS_INCLUDES)
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
	@echo "Compilation de vertex shader"
	@$(GLSLC) $< -o $@

$(FRAG_SPV): $(FRAG_SRC)
	@echo "Compilation de fragment shader"
	@$(GLSLC) $< -o $@

$(TARGET): $(SRC) $(HDR) $(SHADERS)
	@echo "Compilation du projet"
	@$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(LDFLAGS)

shaders: $(SHADERS)

test: $(TARGET)
	@echo "Lancement du projet"
	@./$(TARGET)

clean:
	rm -f $(TARGET) $(SHADERS)
