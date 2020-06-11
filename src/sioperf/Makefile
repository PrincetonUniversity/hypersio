CXX = g++
CXXFLAGS = -Wall -g -O2 -std=c++11 -pthread
DEBUG ?= NODEBUG

SRCDIR = src
OBJDIR = obj
INCDIR = ./include

SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
INCLUDES = $(wildcard $(INCDIR)/*.h)
TEMPLATES = $(wildcard $(INCDIR)/*.tcc)

.PHONY: clean build rebuild

all: build

build: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o hypersio-perf $(OBJECTS)

$(OBJECTS): $(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(INCLUDES) $(TEMPLATES)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -D$(DEBUG) -c -o $@ $<

run:
	./hypersio-perf

clean:
	rm -f obj/*
	rm -f hypersio-perf

rebuild: clean build