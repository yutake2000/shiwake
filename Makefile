CXX=g++
CXXFLAGS=`pkg-config opencv --cflags`
LDLIBS=-lglut -lGL -lGLU `pkg-config opencv --libs` -pthread -lasound -lpulse

main: main.cpp pulse.cpp yswavfile.cpp
	g++ -o main main.cpp pulse.cpp yswavfile.cpp ${CXXFLAGS} ${LDLIBS}