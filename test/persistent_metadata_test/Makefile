GTEST_DIR := ../gmock/gtest
USER_DIR := ../../src/lib/persistent_metadata

CXX := g++ -std=c++11
CXXFLAGS := -g -O3 -pthread -Wall -Wextra -Weffc++ \
            -isystem $(GTEST_DIR)/include

TESTS := persistent_metadata_unittest

GTEST_HEADERS := $(GTEST_DIR)/include/gtest/*.h \
                 $(GTEST_DIR)/include/gtest/internal/*.h

.PHONY: all clean fullclean

all : $(TESTS)

GTEST_OBJS := gtest-all.o gtest_main.o
USER_OBJS := persistent_metadata_unittest.o

clean :
	rm -f $(TESTS) $(USER_OBJS)

fullclean : clean
	rm -f $(GTEST_OBJS) gtest_main.a


gtest_main.a : $(GTEST_OBJS)
	ar rcs $@ $^

GTEST_SRCS := $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)

gtest-all.o : $(GTEST_SRCS)
	$(CXX) $(CXXFLAGS) -I$(GTEST_DIR) -c $(GTEST_DIR)/src/gtest-all.cc -o $@

gtest_main.o : $(GTEST_SRCS)
	$(CXX) $(CXXFLAGS) -I$(GTEST_DIR) -c $(GTEST_DIR)/src/gtest_main.cc -o $@


persistent_metadata_unittest.o : persistent_metadata_unittest.cpp \
                              $(USER_DIR)/*.h \
                              $(GTEST_HEADERS)
	$(CXX) $(CXXFLAGS) -I$(USER_DIR)/.. -c $< -o $@

$(USER_DIR)/libpersistent_metadata.a : $(USER_DIR)/*.cpp $(USER_DIR)/*.h \
                                 $(USER_DIR)/Makefile
	make -C $(USER_DIR) libpersistent_metadata.a

persistent_metadata_unittest : $(USER_DIR)/libpersistent_metadata.a \
                            persistent_metadata_unittest.o gtest_main.a 
	$(CXX) $(CXXFLAGS) -Wl,--start-group $^ -Wl,--end-group -o $@

