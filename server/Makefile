# Name your executable
APP = aesdsocket

# Check if CROSS_COMPILE is set, otherwise default to an empty value
CC = $(CROSS_COMPILE)gcc

# Compilation flags
CFLAGS += -Wall -Werror

FILE_PATH = "/var/tmp/aesdsocketdata"

# Define the target
all: $(APP)

default: $(APP)

# Compile the application
$(APP): aesdsocket.c
	$(CC) $(CLFAGS) -o $(APP) aesdsocket.c

# Clean target to remove the compiled file
clean:
	rm -f $(APP)
	rm -f $(FILE_PATH) 
