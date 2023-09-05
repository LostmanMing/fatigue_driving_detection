CURRENT_DIR = $(PWD)

INCLUDE += -I.

			
LIBPATH = 
LIB= -lssl -lcrypto -lcurl               
CC = g++
CFLAGS = -Wall -std=c++11 -O3
LDFLAGS =


SAMDIR = $(CURRENT_DIR)


SRC = $(shell find $(SAMDIR) -name '*.cpp')

OBJS = $(SRC:.cpp=.o)


main:$(OBJS)
	$(CC) $(CFLAGS) -o  $@ $(OBJS)  $(LIBPATH) $(LIB) $(LDFLAGS)

$(OBJS): %.o: %.cpp
	$(CC) -c $(CFLAGS) $< -o  $@  $(INCLUDE)


clean:
	rm -fr $(OBJS) main


