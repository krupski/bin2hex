####################
# bin2hex Makefile #
####################

TARGET = bin2hex
CC = gcc
CPPFLAGS += -Wall
CPPFLAGS += -O3
PROGSIZE = size

$(TARGET): $(TARGET).o
	@echo "$(TARGET).o -> $(TARGET)"
	@$(CC) $(TARGET).o -lm -o $(TARGET)
	@$(PROGSIZE) $(TARGET)

$(TARGET).o: $(TARGET).cpp
	@echo "$(TARGET).c -> $(TARGET).o"
	@$(CC) $(CPPFLAGS) -c $(TARGET).cpp
	@$(PROGSIZE) $(TARGET).o

clean:
	@rm -f $(TARGET) $(TARGET).o