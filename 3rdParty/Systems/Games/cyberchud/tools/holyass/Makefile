CXX := g++
CXXFLAGS := -Wall -Wextra -pedantic -g -O2 -std=gnu++17 -fno-exceptions -fno-rtti

holyass: main.o export.o bones.o flags.o animation.o
	$(CXX) $(CXXFLAGS) -lassimp $^ -o $@

main.o: main.cpp types.h export.h bones.h animation.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

export.o: export.cpp export.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

bones.o: bones.cpp bones.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

flags.o: flags.cpp flags.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

animation.o: animation.cpp animation.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

