DEBUG=#-DCOZEN_DEBUG -ggdb -g3

INCLUDE_DIRS = $(patsubst %,-I./%, $(wildcard lib/*))
CC=g++
CCFLAGS=-fPIC -pie -Wno-unused-result -O2 -m64 -Wall $(INCLUDE_DIRS) -fstack-protector-all -std=gnu++17 $(DEBUG)
LDFLAGS=-fPIC -pie -L./lib-cozen -ldl -z now -z relro -Wl,--dynamic-linker=./ld-linux-cozen.so

LIB_CCFLAGS=-fPIC -pie -Wno-unused-result -O2 -m64 -Wall $(INCLUDE_DIRS) -fstack-protector-all -std=gnu++17 $(DEBUG)
LIB_LDFLAGS=-shared -fPIC -z now -z relro -L./lib-cozen

CPPFILES = $(wildcard *.cpp)
OBJECTS = $(patsubst %.cpp,%.o,$(CPPFILES))

LIB_CPPFILES = $(wildcard lib/*/*.cpp)
LIB_OBJECTS = $(patsubst %.cpp,%.o,$(LIB_CPPFILES))
BBS_LIB_OBJECTS = $(filter lib/bbs/%.o,$(LIB_OBJECTS))
BBS_LIB_OBJECTS_NOUSER = $(filter-out lib/bbs/BBSUser.o,$(BBS_LIB_OBJECTS))
NON_BBS_LIB_OBJECTS = $(filter-out lib/bbs/%.o,$(LIB_OBJECTS)) lib/bbs/BBSUser.o lib/bbs/BBSUserSearch.o
LIB_OBJECTS_LDFLAGS = $(wildcard lib/*/*.ldflags)
LIBS = $(patsubst %.cpp,lib-cozen/lib%.so,$(notdir $(LIB_CPPFILES)))
BBS_LIBS = $(filter lib-cozen/libBBS%.so,$(LIBS))
BBS_LIBS_NOUSER = $(filter-out lib-cozen/libBBSUser.so,$(BBS_LIBS))
NON_BBSLIBS = $(filter-out lib-cozen/libBBS%.so,$(LIBS)) lib-cozen/libBBSUser.so lib-cozen/libBBSUserSearch.so lib-cozen/libBBSStats.so
SORTED_LIBS = $(NON_BBSLIBS) $(BBS_LIBS_NOUSER)

BASELIBS = $(filter lib-cozen/lib%Base.so,$(LIBS))
LDFLAGS += $(patsubst lib-cozen/lib%.so,-l%,$(BASELIBS))

.PHONY: all clean docker

all: cozen

lib/%.o: lib/%.cpp
	$(CC) $(LIB_CCFLAGS) -c $< -o $@

$(BBS_LIBS_NOUSER): lib-cozen/lib%.so: $(BBS_LIB_OBJECTS_NOUSER)
	$(eval OBJ_FILE = $(filter %/$*.o, $^))
	$(eval OBJ_FILE_LDFLAGS = $(patsubst %.o,%.ldflags,$(OBJ_FILE)))
	$(eval OBJ_FILE_LD_FLAGS = $(shell echo -n `if test -f "$(OBJ_FILE_LDFLAGS)"; then cat $(OBJ_FILE_LDFLAGS); fi`))
	$(CC) -Wl,-soname,$@ -o $@ $(OBJ_FILE) $(LIB_LDFLAGS) $(OBJ_FILE_LD_FLAGS)
ifeq ($(DEBUG),) 
	strip -s -x -X $@
endif

$(NON_BBSLIBS): lib-cozen/lib%.so: $(NON_BBS_LIB_OBJECTS)
	$(eval OBJ_FILE = $(filter %/$*.o, $^))
	$(eval OBJ_FILE_LDFLAGS = $(patsubst %.o,%.ldflags,$(OBJ_FILE)))
	$(eval OBJ_FILE_LD_FLAGS = $(shell echo -n `if test -f "$(OBJ_FILE_LDFLAGS)"; then cat $(OBJ_FILE_LDFLAGS); fi`))
	$(CC) -Wl,-soname,$@ -o $@ $(OBJ_FILE) $(LIB_LDFLAGS) $(OBJ_FILE_LD_FLAGS)
ifeq ($(DEBUG),) 
	strip -s -x -X $@
endif

cozen : $(OBJECTS) | $(SORTED_LIBS)
	$(eval OBJ_FILE_LD_FLAGS = $(shell echo -n `if test -f "cozen.ldflags"; then cat cozen.ldflags; fi`))
	$(CC) $(CCFLAGS) -o cozen $(OBJECTS) $(LDFLAGS) $(OBJ_FILE_LD_FLAGS)
ifeq ($(DEBUG),) 
	strip -s -x -X cozen
endif

%.o: %.cpp
	$(CC) $(CCFLAGS) -c $<

docker :
	./copy-docker-files.sh
	cd docker && ./tester build

clean:
	rm -f cozen $(OBJECTS) $(LIB_OBJECTS) $(LIBS)
