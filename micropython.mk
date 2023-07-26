THISMOD_DIR := $(USERMOD_DIR)

# Add our source files to the respective variables.
SRC_USERMOD_CXX += $(THISMOD_DIR)/experimental.cc

# Add our module directory to the include path.
CXXFLAGS_USERMOD += -I$(THISMOD_DIR) -std=c++17

# We use C++ features so have to link against the standard library.
LDFLAGS_USERMOD += -lstdc++