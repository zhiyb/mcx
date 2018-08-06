# Author: Norman Zhi (normanzyb@gmail.com)

PRG	= mcx
OBJ	= main.o network.o
PKG	= libuv libcrypto

#CROSS	= mipsel-linux-
FLAGS		= -g -O0 -Wall -Werror -Wno-error=unused-variable
CFLAGS		= $(FLAGS) $(shell pkg-config --cflags $(PKG)) -std=gnu99
CXXFLAGS	= $(FLAGS) $(shell pkg-config --cflags $(PKG)) -std=c++11
LDFLAGS		= -g -O0
LIBS		= $(shell pkg-config --libs $(PKG)) -lev -lpthread -lm

CC	= $(CROSS)gcc
CXX	= $(CROSS)g++
LD	= $(CROSS)g++

.PHONY: all
all: $(PRG)

.PHONY: run
run: $(PRG)
	./$(PRG) localhost 25565 zs.yjbeetle.com.cn 25565

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
