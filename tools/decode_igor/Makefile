
CXXFLAGS += -MMD

OBJS = detection.o file.o insn_x86.o main.o overlay_exe.o room.o segment_exe.o symbols.o tga.o voc.o
DEPS = $(OBJS:.o=.d)

decode_igor: $(OBJS)
	$(CXX) -o $@ $^ -lz

clean:
	rm -f *.o *.d

-include $(DEPS)
