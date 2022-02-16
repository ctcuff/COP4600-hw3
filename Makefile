CXX = g++ -std=c++14

SRC = main.cpp
ARGS = -Wall -Wtype-limits
EXE_NAME = main
OUT_DIR = ./out

all: $(SRC)
	@ mkdir -p out
	$(CXX) $(SRC) -o $(OUT_DIR)/$(EXE_NAME) $(ARGS)

run:
	@ make all
	@ ./out/$(EXE_NAME)

clean:
	@ rm -rf ./out
