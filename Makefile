CXX = g++ -std=c++14

SRC = ./src/mysh.cpp
ARGS = -Wall -Wtype-limits -Wextra
BUILD_FOLDER = ./out/
EXE_NAME = mysh

all: $(SRC)
	@ mkdir -p $(BUILD_FOLDER)
	@ $(CXX) $(SRC) -o $(BUILD_FOLDER)$(EXE_NAME) $(ARGS)

run:
	@ make all && cd $(BUILD_FOLDER) && ./$(EXE_NAME)

clean:
	@ rm -rf $(BUILD_FOLDER)
