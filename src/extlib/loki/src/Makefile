include ../Makefile.common

SRC := $(wildcard *.cpp)
STATIC_OBJ := $(SRC:.cpp=.o)
SHARED_OBJ := $(SRC:.cpp=.lo)

override CPPFLAGS += -I../include -DNDEBUG

STATIC_LIB := libloki.a

ifeq ($(OS), Darwin)
  SHARED_LIB_BASE := libloki.dylib
  SHARED_LIB_VERSIONED := libloki.$(VERSION).dylib
  override LDFLAGS += -dynamiclib -single_module -install_name $(SHARED_LIB_VERSIONED) -fPIC
  LDLIBS := -lpthread
else
ifeq ($(OS), Linux)
  SHARED_LIB_BASE := libloki.so
  SHARED_LIB_VERSIONED := libloki.so.$(VERSION)
  override LDFLAGS += --shared -Wl,-soname=$(SHARED_LIB_VERSIONED) -fPIC
  LDLIBS := -lpthread
else
ifeq ($(OS), GNU/kFreeBSD)
  SHARED_LIB_BASE := libloki.so
  SHARED_LIB_VERSIONED := libloki.so.$(VERSION)
  override LDFLAGS += --shared -Wl,-soname=$(SHARED_LIB_VERSIONED) -fPIC
  LDLIBS := -lpthread
else
ifeq ($(OS), GNU)
  SHARED_LIB_BASE := libloki.so
  SHARED_LIB_VERSIONED := libloki.so.$(VERSION)
  override LDFLAGS += --shared -Wl,-soname=$(SHARED_LIB_VERSIONED) -fPIC
  LDLIBS := -lpthread
else
ifeq ($(OS), HP-UX)
  SHARED_LIB_BASE := libloki.so
  SHARED_LIB_VERSIONED := libloki.so.$(VERSION)
  override LDFLAGS += --shared -fPIC -mlp64
  override CXXFLAGS += -mlp64
  LDLIBS := -lpthread
  RESULT_DIR := ../lib/
else
  SHARED_LIB_VERSIONED := libloki.dll
  override LDFLAGS += --shared
  LDLIBS :=
endif
endif
endif
endif
endif

RESULT_DIR := ../lib/

.PHONY: all build-static build-shared clean distclean
all: build-static build-shared
build-static: $(RESULT_DIR)$(STATIC_LIB)
build-shared: $(RESULT_DIR)$(SHARED_LIB_VERSIONED)

clean:
	$(RM) $(STATIC_OBJ)
ifneq ($(OS), Windows)
	$(RM) $(SHARED_OBJ)
endif

distclean: clean cleandeps
	$(RM) $(RESULT_DIR)$(STATIC_LIB)
	$(RM) $(RESULT_DIR)$(SHARED_LIB_VERSIONED)

ifneq ($(OS),Windows)
INSTALL := install
INSTALL_DATA := $(INSTALL) -m 644

.PHONY: install install-static install-shared
install: install-static install-shared

install-static: $(RESULT_DIR)$(STATIC_LIB)
	mkdir -p $(prefix)/lib
	$(INSTALL_DATA) $(RESULT_DIR)$(STATIC_LIB) $(prefix)/lib

install-shared: $(RESULT_DIR)$(SHARED_LIB_VERSIONED)
	mkdir -p $(prefix)/lib
	$(INSTALL_DATA) $(RESULT_DIR)$(SHARED_LIB_VERSIONED) $(prefix)/lib
	cd $(prefix)/lib; ln -s $(SHARED_LIB_VERSIONED) $(SHARED_LIB_BASE)

%.lo : %.cpp
	$(CXX) -c $(CXXFLAGS) -fPIC $(CPPFLAGS) -o $@ $<

$(RESULT_DIR)$(SHARED_LIB_VERSIONED): $(SHARED_OBJ)
else
$(RESULT_DIR)$(SHARED_LIB_VERSIONED): $(STATIC_OBJ)
endif
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(RESULT_DIR)$(STATIC_LIB): $(STATIC_OBJ)
	$(AR) $(ARFLAGS) $@ $^

include ../Makefile.deps
