PROJECT=cp-tools
CC=g++

# Compiler setup
CFLAGS=-W -Wall -Werror -std=c++17

RELEASE_CFLAGS=-O2
DEBUG_CFLAGS=-g -O0

ifeq ($(MAKECMDGOALS),release)
    CFLAGS+=$(RELEASE_CFLAGS)
else
    CFLAGS+=$(DEBUG_CFLAGS)
endif

# Tools
AR=ar
AR_FLAGS=rcs
LINKER=$(CC)

# Environment variables
INCLUDES_FLAG=-I
LIBPATH_FLAG=-L
EXT_LIBPATH_FLAG=-L
LIBS_FLAG=-l
EXT_LIBS_FLAG=-l
OBJ_OUTPUT_FLAG=-o 
AR_OUTPUT_FLAG=
OUTPUT_FLAG=-o 
GEN_OBJECT_FLAG=-c
STATIC_LIB_SUFFIX=.a
STATIC_LIB_PREFIX=lib
OBJ_EXTENSION=.o

# Directories
SRC_DIR=src
LIBS_DIR=libs
TESTS_DIR=tests
CLASSES_DIR=classes
SCRIPTS_DIR=scripts
TEMPLATE_DIR=templates

INSTALL_BIN_DIR=/usr/local/bin
INSTALL_MAN_DIR=/usr/share/man/man1
INSTALL_CLASSES_DIR=/usr/local/lib
INSTALL_TEMPLATE_DIR=/usr/local/lib
INSTALL_COMPLETION_DIR=/etc/bash_completion.d

CP_TEMP_DIR=.cp-tmp
CP_BUILD_DIR=.cp-build

# Project targets
LIBRARY=$(STATIC_LIB_PREFIX)$(PROJECT)$(STATIC_LIB_SUFFIX)
TEST_SUIT=cp-run_tests

# External libraries
LIBS=

# Internal libraries
INCLUDES_FILES=include
INCLUDES_LIBS=${shell find libs -type d}

LDFLAGS=
INCLUDES=${addprefix $(INCLUDES_FLAG), $(INCLUDES_FILES)} ${addprefix $(INCLUDES_FLAG), $(INCLUDES_LIBS)}

.PHONY: all clean

# Project source files
PROJECT_MAIN=$(SRC_DIR)/main.cpp
PROJECT_OBJECT=$(PROJECT_MAIN:.cpp=$(OBJ_EXTENSION))

SOURCES=${shell find $(SRC_DIR) -type f -name *.cpp}
SOURCES:=${filter-out $(PROJECT_MAIN), $(SOURCES)}

OBJECTS=$(SOURCES:.cpp=$(OBJ_EXTENSION))
COMPLETION_SCRIPT=$(PROJECT)-completion.sh
MAN_FILE=cp-tools.1

TEST_SOURCES=${wildcard $(TESTS_DIR)/*.cpp}
TEST_OBJECTS=$(TEST_SOURCES:.cpp=$(OBJ_EXTENSION))

# Rules
.SUFFIXES: .cpp .$(OBJ_EXTENSION) 


.cpp$(OBJ_EXTENSION):
	$(CXX) $(GEN_OBJECT_FLAG) $< $(OBJ_OUTPUT_FLAG) $@ $(CFLAGS) $(INCLUDES)


all: $(LIBRARY) $(PROJECT) $(TEST_SUIT)


$(LIBRARY): $(OBJECTS)
	$(AR) $(AR_FLAGS) $(AR_OUTPUT_FLAG) $@ $(OBJECTS) 


$(PROJECT): $(OBJECTS) $(PROJECT_OBJECT)
	$(LINKER) $(OUTPUT_FLAG)$@ $(LDFLAGS) $(PROJECT_OBJECT) $(LIBS) $(EXTRA_LIBS) $(LIBRARY) 


$(TEST_SUIT): $(LIBRARY) $(TEST_OBJECTS)
	$(LINKER) $(OUTPUT_FLAG)$@ $(LDFLAGS) $(TEST_OBJECTS) $(LIBRARY) $(LIBS) $(EXTRA_LIBS)


update_release:
	@./scripts/gen_defs.sh


release: update_release $(LIBRARY) $(PROJECT)


install: $(PROJECT)
	@cp $(PROJECT) $(INSTALL_BIN_DIR)
	@mkdir -p $(INSTALL_TEMPLATE_DIR)/$(PROJECT)
	@cp -r $(TEMPLATE_DIR) $(INSTALL_TEMPLATE_DIR)/$(PROJECT)/
	@mkdir -p $(INSTALL_CLASSES_DIR)/$(PROJECT)
	@cp -r $(CLASSES_DIR) $(INSTALL_CLASSES_DIR)/$(PROJECT)/
	@cp $(SCRIPTS_DIR)/$(COMPLETION_SCRIPT) $(INSTALL_COMPLETION_DIR)
	@cp $(MAN_FILE) $(INSTALL_MAN_DIR)


uninstall:
	@rm -f $(INSTALL_COMPLETION_DIR)/$(COMPLETION_SCRIPT)
	@rm -rf $(INSTALL_TEMPLATE_DIR)/$(PROJECT)
	@rm -f $(INSTALL_BIN_DIR)/$(PROJECT)
	@rm -f $(INSTALL_MAN_DIR)/$(MAN_FILE)


clean:
	@rm -f *~ $(LIBRARY) $(PROJECT) $(TEST_SUIT)
	@find . -name '*.o' -exec rm -f {}  \;
	@rm -rf *~ $(CP_TEMP_DIR) $(CP_BUILD_DIR)
