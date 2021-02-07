CXX := clang++

CXXFLAGS += -Wall -g -std=c++11

OBJS := $(patsubst %.cpp, %.o, $(wildcard srcs/*.cpp))
OBJS += $(patsubst %.cpp, %.o, $(wildcard third-party/tinyxml2/*.cpp))
dloader: clean $(OBJS)
	$(CXX) -o $@ $(OBJS)

clean:
	rm -rf srcs/*.o third-party/tinyxml2/*.o dloader
