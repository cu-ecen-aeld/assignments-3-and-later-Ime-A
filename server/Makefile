# Name your executable
APP = aesdsocket

# Check if CROSS_COMPILE is set, otherwise default to an empty value
CC ?= $(CROSS_COMPILE)gcc
# Linker flags (set externally by Yocto)
LDFLAGS ?= -lpthread -lrt
# Compilation flags
CFLAGS ?= -g -Wall -Werror

FILE_PATH = "/var/tmp/aesdsocketdata"

# Define the target
# Define the default target
.PHONY: all clean
all: $(APP)

default: $(APP)

# Compile the application
$(APP): aesdsocket.c
	$(CC) $(CLFAGS) -o $(APP) aesdsocket.c

# Clean target to remove the compiled file
clean:
	rm -f $(APP)
	rm -f $(FILE_PATH) 
