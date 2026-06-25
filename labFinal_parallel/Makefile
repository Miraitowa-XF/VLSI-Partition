SRC = main.cpp Graph.cpp Net.cpp Node.cpp solution.cpp evaluate.cpp
OBJ = $(SRC:.cpp=.o)

CXX = g++
# CFLAGS = -Wall -Wextra -Werror -std=c++17 -pedantic
# CFLAGS += -g

CXXFLAGS = -Wall -Wextra -std=c++17 -O2 -g -pthread

all: main

main: $(OBJ)
	$(CXX) $(CXXFLAGS) -o main $(OBJ)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f main *.o