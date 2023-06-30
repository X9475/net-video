PROJ   = ../../bin/tracker
OBJS   = $(patsubst %.cpp, %.o, $(wildcard ../01_common/*.cpp *.cpp))
CC     = g++
LINK   = g++
RM     = rm -rf
CFLAGS = -c -Wall \
         -I/usr/include/acl-lib/acl_cpp \
         `mysql_config --cflags` \
         -I../01_common
LIBS   = -pthread -lacl_all `mysql_config --libs`

all: $(PROJ)

$(PROJ): $(OBJS)
	$(LINK) $^ $(LIBS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $^ -o $@

clean:
	$(RM) $(PROJ) $(OBJS)
