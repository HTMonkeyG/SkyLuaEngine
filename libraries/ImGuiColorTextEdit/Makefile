CXX = g++
AR = ar

CFLAGS = -std=c++11 -g -Wall -Wformat -Wno-sign-compare -I. -I../htmodloader/includes/imgui-1.92.2b

all: libTextEditor.a

libTextEditor.a: ./TextEditor.o
	$(AR) rcs $@ $^

./TextEditor.o: ./TextEditor.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

clean:
	del *.o
	del *.a
