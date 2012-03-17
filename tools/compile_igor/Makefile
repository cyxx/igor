
CXXFLAGS += -MMD

compile_igor: detect.o main.o
	$(CXX) -o $@ $^

clean:
	rm -f *.o *.d
