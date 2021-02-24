CXX := clang++

CXXFLAGS += -Wall -g -O2 -std=c++11

objs := $(patsubst %.cpp, %.o, $(wildcard srcs/*.cpp))
objs += $(patsubst %.cpp, %.o, $(wildcard third-party/tinyxml2/*.cpp))

dloader: $(objs)
	$(CXX) $^ -o $@

clean:
	rm -rf $(objs) dloader
