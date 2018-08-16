# Author: Norman Zhi (normanzyb@gmail.com)

PRG	= mcx
ARGS	= :: 25565 zs.yjbeetle.com.cn 25565
OBJ	= main.o mutex.o network.o networkrequests.o networkclient.o client.o \
	  parsers.o parser/parser.o
PARSERS	= mcx
OBJ	+= $(PARSERS:%=parser/%.o)
PKG	= libuv libcrypto

#CROSS	= mipsel-linux-
FLAGS		= -g -O0 -Wall -Werror -Wno-error=unused-variable
CFLAGS		= $(FLAGS) $(shell pkg-config --cflags $(PKG)) -std=gnu99
CXXFLAGS	= $(FLAGS) $(shell pkg-config --cflags $(PKG)) -std=gnu++14
LDFLAGS		= -g -O0
LIBS		= $(shell pkg-config --libs $(PKG)) -lev -lpthread -lm

CC	= $(CROSS)gcc
CXX	= $(CROSS)g++
LD	= $(CROSS)g++

.PHONY: all
all: $(PRG)

.PHONY: run
run: $(PRG)
	./$(PRG) $(ARGS)

.PHONY: gdb
gdb: $(PRG)
	gdb ./$(PRG) -ex 'run $(ARGS)'

.PHONY: valgrind
valgrind: $(PRG)
	valgrind ./$(PRG) $(ARGS)

$(PRG): $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) -o $@ $<

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

# Dependency:
-include $(OBJ:.o=.d)
%.d: %.cpp
	@set -e; rm -f $@; \
	$(CXX) -MM $(CXXFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.d: %.c
	@set -e; rm -f $@; \
	$(CC) -MM $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

# Clean up
.PHONY: clean
clean:
	rm -f $(OBJ) $(OBJ:%.o=%.d) $(OBJ:%.o=%.d.*) $(PRG)
