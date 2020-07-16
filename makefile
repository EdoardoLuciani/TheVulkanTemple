CXX = g++
CXXFLAGS = -I/usr/local/include -std=c++17 -Wall
LDFLAGS = -L/usr/local/lib
LDLIBS = -lglfw3 -ldl -lpthread -lstdc++fs -lX11

SDIR = src
ODIR = obj
EXE = hdr_tonemapping

release: CXXFLAGS += -O3 -D NDEBUG
release: $(EXE)
debug: CXXFLAGS += -DDEBUG -g -O0
debug: $(EXE)
$(EXE): $(ODIR)/main.o $(ODIR)/vulkan_helper.o
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(ODIR)/main.o: $(SDIR)/main.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

$(ODIR)/vulkan_helper.o: $(SDIR)/vulkan_helper.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

# Shaders compilation
SOURCE_SHADERDIR = $(SDIR)/shaders
OUT_SHADERDIR = shaders
VULKANENV = /home/edoardo/vulkan/1.2.141.0/x86_64/bin

shaders: $(SOURCE_SHADERDIR)/PBR/pbr.vert $(SOURCE_SHADERDIR)/PBR/pbr.frag \
$(SOURCE_SHADERDIR)/SMAA/smaa_edge.vert $(SOURCE_SHADERDIR)/SMAA/smaa_edge.frag \
$(SOURCE_SHADERDIR)/SMAA/smaa_weight.vert $(SOURCE_SHADERDIR)/SMAA/smaa_weight.frag \
$(SOURCE_SHADERDIR)/SMAA/smaa_blend.vert $(SOURCE_SHADERDIR)/SMAA/smaa_blend.frag \
$(SOURCE_SHADERDIR)/HDR/luminance.comp $(SOURCE_SHADERDIR)/HDR/tonemap.vert $(SOURCE_SHADERDIR)/HDR/tonemap.frag
# PBR
	$(VULKANENV)/glslc -c -o $(OUT_SHADERDIR)/pbr.vert.spv $(SOURCE_SHADERDIR)/PBR/pbr.vert
	$(VULKANENV)/glslc -c -o $(OUT_SHADERDIR)/pbr.frag.spv $(SOURCE_SHADERDIR)/PBR/pbr.frag
# SMAA
	$(VULKANENV)/glslc -c -o $(OUT_SHADERDIR)/smaa_edge.vert.spv $(SOURCE_SHADERDIR)/SMAA/smaa_edge.vert
	$(VULKANENV)/glslc -c -o $(OUT_SHADERDIR)/smaa_edge.frag.spv $(SOURCE_SHADERDIR)/SMAA/smaa_edge.frag
	$(VULKANENV)/glslc -c -o $(OUT_SHADERDIR)/smaa_weight.vert.spv $(SOURCE_SHADERDIR)/SMAA/smaa_weight.vert
	$(VULKANENV)/glslc -c -o $(OUT_SHADERDIR)/smaa_weight.frag.spv $(SOURCE_SHADERDIR)/SMAA/smaa_weight.frag
	$(VULKANENV)/glslc -c -o $(OUT_SHADERDIR)/smaa_blend.vert.spv $(SOURCE_SHADERDIR)/SMAA/smaa_blend.vert
	$(VULKANENV)/glslc -c -o $(OUT_SHADERDIR)/smaa_blend.frag.spv $(SOURCE_SHADERDIR)/SMAA/smaa_blend.frag
# HDR
	$(VULKANENV)/glslc -c -o $(OUT_SHADERDIR)/luminance.comp.spv $(SOURCE_SHADERDIR)/HDR/luminance.comp
	$(VULKANENV)/glslc -c -o $(OUT_SHADERDIR)/tonemap.vert.spv $(SOURCE_SHADERDIR)/HDR/tonemap.vert
	$(VULKANENV)/glslc -c -o $(OUT_SHADERDIR)/tonemap.frag.spv $(SOURCE_SHADERDIR)/HDR/tonemap.frag

.PHONY: clean shaders
clean:
	-rm $(ODIR)/* $(EXE)
