ALIGN=3
VPATH = ../../src ../../include ../../BBCSDL/src ../../BBCSDL/include
CXX = gcc -g -Wall -I ../../include -I ../../BBCSDL/include
CXXFLAGS = -DPICO_ALIGN=$(ALIGN) -march=armv7-a -mthumb -munaligned-access -mfloat-abi=hard -c

OBJ = bbmain.o bbexec2.o bbeval2.o bbasmb.o bbdata.o bbccos.o bbccon.o

all: bbcbasic

bbmain.o: bbmain.c BBC.h
	$(CXX) -Wno-attributes $(CXXFLAGS) -O2 $< -o $@

bbexec2.o: bbexec2.c BBC.h
	$(CXX) $(CXXFLAGS) -DARMHF -O2 $< -o $@

bbeval2.o: bbeval2.c BBC.h
	$(CXX) -Wno-array-bounds $(CXXFLAGS) -O2 $< -o $@

bbasmb.o: bbasmb_arm_32.c BBC.h
	$(CXX) $(CXXFLAGS) -Os $< -o $@

bbccos.o: bbccos.c bbccon.h
	$(CXX) -Wno-array-bounds -Wno-unused-result $(CXXFLAGS) -Os $< -o $@

bbccon.o: bbccon.c bbccon.h
	$(CXX) -Wno-array-bounds -Wno-unused-result $(CXXFLAGS) -O2 $< -o $@

bbdata.o: ../../src/bbdata_arm_32a.s
	as ../../src/bbdata_arm_32a.s -o bbdata.o

bbcbasic: $(OBJ)
	$(CXX) $(OBJ) -L . -L/usr/lib/ -ldl -lm -lrt -pthread \
	-o bbcbasic -Wl,-R,'$$ORIGIN'
	cp bbcbasic ../../
