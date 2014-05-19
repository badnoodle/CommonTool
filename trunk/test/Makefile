
ROOT = ..
HOME = .

include .test_depend
include $(ROOT)/config.mk

LIBS = -L$(ROOT)/$(LIB_DIR) -l$(TOOL)

all:compare only-c

only-c:test.c
	$(CC) -o test test.c -I$(ROOT)/$(INC_DIR) $(LIBS) $(G_LINK_F) $(C_FLAGS)

compare:cpp-c-test.cpp
	$(CXX) -o cmp cpp-c-test.cpp -I$(ROOT)/$(INC_DIR) $(LIBS) $(G_LINK_F) $(CPP_FLAGS)

test_depend:
	$(echo "" > .test_depend)
	$(gcc -MM -I$(ROOT)/$(INC_DIR) cpp-c-test.cpp 2 > /dev/null >> .test_depend)
clean:
	rm -f bt_test.o
	rm -f test
	rm -f cmp
	rm -f .test_depend
