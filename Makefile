CXX = g++ -std=c++14

SRC = main.cpp
ARGS = -Wall -Wtype-limits -Wextra
EXE_NAME = main

all: $(SRC)
	@ $(CXX) $(SRC) -o $(EXE_NAME) $(ARGS)

run:
	@ make all
	@ ./$(EXE_NAME)

clean:
	@ rm $(EXE_NAME)
