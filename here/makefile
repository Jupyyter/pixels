# --------------------------------------------------------------------------------
# --- Compiler and Executable ---
CXX = g++
EXECUTABLE = main

# --------------------------------------------------------------------------------
# --- Source Files and Object Files ---
SRC_DIR = src
OBJ_DIR = obj

SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))

# --------------------------------------------------------------------------------
# --- Paths ---
SFML_DIR = ../SFML-3.0.0
GLM_DIR = ../glm/glm-master
BOX2D_DIR = ../2dbox
BOX2D_LIB_DIR = $(BOX2D_DIR)/build/src  # <-- Where libbox2d.a lives
LOCAL_INCLUDE_DIR = include

# --------------------------------------------------------------------------------
# --- Compiler and Linker Flags ---
CPP_STANDARD = -std=c++17

INCLUDE_DIRS = \
	-I$(SFML_DIR)/include \
	-I$(GLM_DIR) \
	-I$(LOCAL_INCLUDE_DIR) \
	-I$(BOX2D_DIR)/include \
	-I.

DEFINES = -DSFML_STATIC

CXXFLAGS_COMMON = $(CPP_STANDARD) $(INCLUDE_DIRS) $(DEFINES)
CXXFLAGS_RELEASE = $(CXXFLAGS_COMMON) -O2
CXXFLAGS_DEBUG = $(CXXFLAGS_COMMON) -g

LDFLAGS = -L$(SFML_DIR)/lib -L$(BOX2D_LIB_DIR) -B"C:/mingwsfml/bin"

# --------------------------------------------------------------------------------
# --- Libraries ---
LIBS_RELEASE = \
	-lsfml-graphics-s -lsfml-window-s -lsfml-system-s \
	-lopengl32 -lfreetype -lwinmm -lgdi32 -lpthread \
	-lbox2d                             # -mwindows

LIBS_DEBUG = \
	-lsfml-graphics-s-d -lsfml-window-s-d -lsfml-system-s-d \
	-lopengl32 -lfreetype -lwinmm -lgdi32 -lpthread \
	-lbox2d                             # -mwindows

# --------------------------------------------------------------------------------
# --- Build Rules ---
all: run

run: release
	@echo "--- Cleaning up object files ---"
	rm -rf $(OBJ_DIR)
	@echo "--- Running executable ---"
	./$(EXECUTABLE)

release: CXXFLAGS = $(CXXFLAGS_RELEASE)
release: LIBS = $(LIBS_RELEASE)
release: $(EXECUTABLE)

debug: CXXFLAGS = $(CXXFLAGS_DEBUG)
debug: LIBS = $(LIBS_DEBUG)
debug: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	@echo "--- Linking executable ---"
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(EXECUTABLE) $(OBJ_DIR)

.PHONY: all run release debug clean
