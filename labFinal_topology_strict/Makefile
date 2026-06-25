SRC = main.cpp Graph.cpp Net.cpp Node.cpp solution.cpp evaluate.cpp
OBJ = $(SRC:.cpp=.o)

CXX = g++
# CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -pedantic -O3
CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -pedantic -O3 -pthread

all: main

main: $(OBJ)
	$(CXX) $(CXXFLAGS) -o main $(OBJ)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f main *.o fm_all_logs.csv fm_trial_results.csv best_trial_id.txt

.PHONY: all clean
