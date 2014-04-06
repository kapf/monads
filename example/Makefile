SOURCES  := $(wildcard *.cpp)
CXXFLAGS := -Wall -Wextra -pedantic -std=c++11 -O3
INCLUDES := -I../include

CXX := g++

DEPS = $(patsubst %.cpp, $(BUILDDIR)/%.dep, $(SOURCES))
BUILDDIR = build
BINARIES := $(patsubst %.cpp, $(BUILDDIR)/%, $(SOURCES))

all: $(BINARIES) test

LDFLAGS_pipelines = -lpthread

.PHONY+=test
test:
	@echo "   [ TEST ]"
	@$(foreach BIN, $(BINARIES), echo "-- $(notdir $(BIN)).cpp -----" && ./$(BIN);)

$(DEPS): $(BUILDDIR)/%.dep: %.cpp $(BUILDDIR)/.tag
	@echo "   [ DP ]  " $<
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -MM $< -MT $@ -MT $(patsubst %.cpp, $(BUILDDIR)/%, $<) -o $@

-include $(DEPS)

$(BINARIES): ./$(BUILDDIR)/%: %.cpp $(DEPS) $(BUILDDIR)/.tag
	@echo "   [ CC ]   $(filter %.cpp,$^)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) $(if $(LDFLAGS_$(notdir $@)),$(LDFLAGS_$(notdir $@)),) -o $@ $(filter %.cpp,$^)

$(BUILDDIR)/.tag:
	@echo "   [ MD ]   $(BUILDDIR)"
	@mkdir -p $(BUILDDIR)
	@touch $(BUILDDIR)/.tag

clean:
	@echo "   [ RM ]   $(BUILDDIR)"
	@rm -rf build	

