TARGET_EXEC ?= tm_osched_model.out

BUILD_DIR ?= $(DESIGN_HOME)/build
SRC_DIR ?= $(DESIGN_HOME)/src

CXX := g++

CPPFLAGS ?= -ggdb -dH

SRCS := $(shell find $(SRC_DIR) -name "*.cpp")
OBJS := $(SRCS:$(SRC_DIR)/%cpp=$(BUILD_DIR)/%o)
INC_DIRS := -I$(DESIGN_HOME)/lib/sknobs
LIB_DIRS := -L$(DESIGN_HOME)/lib/sknobs
LDLIBS := -lsknobs

$(TARGET_EXEC): $(OBJS)
	echo "$(CXX) $(CPPFLAGS) $(OBJS) $(LDLIBS) $(LIB_DIRS) -o $@"
	$(CXX) $(CPPFLAGS) $(OBJS) $(LDLIBS) $(LIB_DIRS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(SRC_DIR)/%.hpp
	$(MKDIR_P) $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(INC_DIRS) -c $< -o $@

.PHONY: clean

dump:
	echo "SRC_DIR $(SRC_DIR)"
	echo "BUILD_DIR $(BUILD_DIR)"
	echo "SRCS $(SRCS)"
	echo "OBJS $(OBJS)"

clean:
	$(RM) -r $(BUILD_DIR)
	$(RM) tm_osched_model.out

MKDIR_P ?= mkdir -p
