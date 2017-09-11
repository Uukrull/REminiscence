
SDL_CFLAGS = `sdl-config --cflags`
SDL_LIBS = `sdl-config --libs`

DEFINES = -DSYS_LITTLE_ENDIAN

CXX = g++
CXXFLAGS:= -g -O -Wall -Wuninitialized -Wno-unknown-pragmas -Wshadow -Wimplicit 
CXXFLAGS+= -Wundef -Wreorder -Wwrite-strings -Wnon-virtual-dtor -Wno-multichar
CXXFLAGS+= $(SDL_CFLAGS) $(DEFINES)

SRCS = collision.cpp file.cpp game.cpp main.cpp menu.cpp piege.cpp resource.cpp \
	sdlstub.cpp staticres.cpp unpack.cpp util.cpp video.cpp

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

rs: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(SDL_LIBS)

.cpp.o:
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $*.o

clean:
	rm -f *.o *.d

-include $(DEPS)
