# Compiler settings
CC := gcc
CFLAGS := -Wall  -Werror -Wextra -g
LIB_FLAGS := -pthread -lrt
TARGET := aesdsocket
SRCS := aesdsocket.c
OBJS := $(SRCS:.c=.o)

# Default target
all: $(TARGET)

# Compile the source file into an object file
%.o: %.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $< $(LIB_FLAGS) -o $@

# Link the object files to create the executable
$(TARGET): $(OBJS)
	$(CROSS_COMPILE)$(CC) $(CFLAGS) $^ $(LIB_FLAGS) -o $@

# Clean up intermediate object files and the executable
clean:
	rm -f $(OBJS) $(TARGET)
