CXX = cc

CXXFLAGS = -std=c++17 -Wall -Wextra -Wall

LDFLAGS = -lstdc++ -lreadline -lncurses

SRC_DIR = src

SRC_FILES = $(SRC_DIR)/string.cc $(SRC_DIR)/main.cc

TARGET = lambda

$(TARGET): $(SRC_FILES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC_FILES) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
all: $(TARGET)

