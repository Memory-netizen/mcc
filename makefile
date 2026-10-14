# Directories
SRC_DIR   := ./src
BUILD_DIR := ./build

# Settings
TARGET := mcc
CFLAGS := -Wall -Wextra -Werror -std=c23 -fno-common
CFLAGS += -D_GNU_SOURCE
CFLAGS += -I$(SRC_DIR)
LDFLAGS :=

# Debug flags
DEBUG ?= 1
ifeq ($(DEBUG), 0)
CFLAGS += -O2 -DNDEBUG
else
CFLAGS += -g -O0
endif

# Source files
SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

HDRS := $(wildcard $(SRC_DIR)/*.h)

ALL_SRCS := $(SRCS) $(HDRS)

DEPS := $(OBJS:.o=.d)
override CPPFLAGS += -MMD -MP

# Main target
$(TARGET): $(OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

# C source compilation
define c_recipe
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
endef

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c ; $(c_recipe)

.PHONY: clean linecnt fmt

clean:
	-rm -rf $(TARGET) $(BUILD_DIR)

linecnt:
	@echo "Total lines of code (excluding blank lines and // comments):"
	@cat $(ALL_SRCS) | sed 's|//.*$$||' | grep -v '^[[:space:]]*$$' | wc -l

fmt:
	@command -v clang-format-21 >/dev/null 2>&1 || { echo "clang-format not found"; exit 1; }
	clang-format-21 -i $(ALL_SRCS)
	@echo "Formatted $(words $(ALL_SRCS)) files."

-include $(DEPS)
