# Compiler settings
#CC := gcc
CFLAGS := -Wall  -Werror -Wextra -g
LDFLAGS := -pthread -lrt
TARGET := aesdsocket
SRCS := aesdsocket.c
OBJS := $(SRCS:.c=.o)

# Default target
all: $(TARGET)

# Compile the source file into an object file
%.o: %.c
	$(CC) $(CFLAGS) -c $< $(LDFLAGS) -o $@

# Link the object files to create the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# Clean up intermediate object files and the executable
clean:
	rm -f $(OBJS) $(TARGET)
