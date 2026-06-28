SRC = main.cpp Graph.cpp Net.cpp Node.cpp solution.cpp evaluate.cpp
OBJ = $(SRC:.cpp=.o)

CXX = g++
# CFLAGS = -Wall -Wextra -Werror -std=c++17 -pedantic
# CFLAGS += -g

# OpenMP 支持：-fopenmp 用于链接 OpenMP 库，-D_OPENMP 定义宏
CXXFLAGS = -Wall -Wextra -std=c++17 -O2 -g -fopenmp 

all: main

main: $(OBJ)
	$(CXX) $(CXXFLAGS) -o main $(OBJ)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f main *.o