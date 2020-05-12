
# Config
BOOSTDIR :=
WINDOWSLDFLAGS  ?= -lws2_32 -lwsock32
LINUXLDFLAGS    ?= -lboost_system -lboost_thread -lpthread -L/usr/lib/ -lstdc++fs
# End of config

CXXFLAGS ?= -std=c++1z -O2 -DRELEASE
INCDIRS  ?= $(BOOSTDIR)

TARGET_EXEC ?= server
CXX ?= g++

BUILD_DIR ?= ./bin/
OBJ_DIR   ?= ./obj/makefile
SRC_DIRS  ?= ./

ifeq ($(OS),Windows_NT)
	LDFLAGS := -s $(WINDOWSLDFLAGS)
else
	LDFLAGS := -s $(LINUXLDFLAGS)
endif

SRCS := $(wildcard *.cpp)
OBJS := $(SRCS:%=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

#INC_DIRS  := $(shell find $(SRC_DIRS) -type d)
#INC_FLAGS := $(addprefix -I,$(INC_DIRS))
INCFLAGS  := $(addprefix -I,$(INCDIRS))

CPPFLAGS ?= $(INC_FLAGS) -MMD -MP

$(BUILD_DIR)/$(TARGET_EXEC): prepare $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# c++ source
$(OBJ_DIR)/%.cpp.o: %.cpp
	$(CXX) $(CPPFLAGS) $(INCFLAGS) $(CXXFLAGS) -c $< -o $@

.PHONY: clean prepare

MD := mkdir -p
RM := rm -rf

prepare:
	$(MD) $(BUILD_DIR)
	$(MD) $(OBJ_DIR)

clean:
	#$(RM) $(BUILD_DIR)
	$(RM) $(OBJ_DIR)

-include $(DEPS)
