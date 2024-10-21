APP = writer

# Check if CROSS_COMPILE is set, otherwise default to an empty value
CC = $(CROSS_COMPILE)gcc

# Define the target
all: $(APP)

# Compile the application
$(APP): writer.c
	$(CC) -o $(APP) writer.c

# Clean target to remove the compiled file
clean:
	rm -f $(APP)
