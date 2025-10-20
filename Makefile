# Minimal Traditional Chess Engine Makefile

CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -DNDEBUG -DUSE_POPCNT
LDFLAGS = -lpthread

TARGET = engine
SRCDIR = src
OBJDIR = obj

# Source files
SOURCES = main.cpp bitboard.cpp position.cpp movegen.cpp misc.cpp evaluate.cpp search.cpp

# Object files
OBJECTS = $(SOURCES:%.cpp=$(OBJDIR)/%.o)

# Default target
all: $(TARGET)

# Create object directory
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Link
$(TARGET): $(OBJDIR) $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compile
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(OBJDIR) $(TARGET)

# Help
help:
	@echo "Minimal Traditional Chess Engine"
	@echo ""
	@echo "Targets:"
	@echo "  make          - Build the engine"
	@echo "  make clean    - Remove build files"
	@echo "  make help     - Show this help"
	@echo ""
	@echo "Usage:"
	@echo "  ./engine --analyze <FEN>"
	@echo "  ./engine --play <Game Count> <Max ply> <White Movetime(ms)> <Black Movetime(ms)>"

.PHONY: all clean help
