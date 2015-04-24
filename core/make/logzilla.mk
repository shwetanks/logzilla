ifndef OPTFLAGS
OPTFLAGS=-O3
endif
ifndef OBJ_SUB_DIR
OBJ_DIR=../obj
else
OBJ_DIR=../obj/$(OBJ_SUB_DIR)
endif

LDFLAGS+=-L../lib
CFLAGS+=$(OPTFLAGS) -ggdb3 -W -Wall -Werror
GCCVER_GTE_4_7=$(shell expr `gcc -dumpversion` \>= 4.7)
ifeq ($(GCCVER_GTE_4_7),1)
CFLAGS+=-ftrack-macro-expansion=2
endif

ifdef UGLY_GETADDRINFO_WORKAROUND
LDFLAGS+=-lanl
RIBIFY_SYMS+=getaddrinfo
CPPFLAGS+=-DUGLY_GETADDRINFO_WORKAROUND
endif

OBJ=$(SRC:%.c=$(OBJ_DIR)/%.o) $(ASM:%.S=$(OBJ_DIR)/%.o)
DEP=$(SRC:%.c=$(OBJ_DIR)/%.d)

DIRS=$(OBJ_DIR)/.dir ../bin/.dir ../lib/.dir
ALL_DIRS=$(DIRS)
ALL_OUTPUT_FILES=$(patsubst %,$(OBJ_DIR)/%,*.o *.d)

ifeq ($(TARGET:%.a=%).a,$(TARGET))
LIB_OBJ:=$(OBJ)
TARGET_FILE=../lib/lib$(TARGET)
else
TARGET_FILE=../bin/$(TARGET)
endif

ALL_OUTPUT_FILES+=$(TARGET_FILE)

all: $(TARGET_FILE)

$(ALL_DIRS):
	@echo "  (MKDIR)  -p $(@:%/.dir=%)"
	@-mkdir -p $(@:%/.dir=%)
	@touch $@

$(OBJ_DIR)/%.o: %.c $(OBJ_DIR)/%.d
	@echo "  (C)      $*.c  [ $(CPPFLAGS) -c $(CFLAGS) $*.c -o $(OBJ_DIR)/$*.o ]"
	@$(CC) $(CPPFLAGS) -c $(CFLAGS) $*.c -o $(OBJ_DIR)/$*.o

$(OBJ_DIR)/%.d: %.c
	@echo "  (DEP)    $*.c"
	@$(CC) -MM $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $*.c | sed -e 's|.*:|$(OBJ_DIR)/$*.o:|' > $@

$(OBJ): $(DIRS)

$(DEP): $(DIRS)

../lib/%: $(LIB_OBJ)
	@echo "  (AR)     $(@:../lib/%=%)  [ rcs $@ $^ ]"
	@$(AR) rcs $@ $^

../bin/%: $(OBJ) $(EXTRA_DEPS)
	@echo "  (LD)     $(@:../bin/%=%)  [ -o $@ $(OBJ) $(LDFLAGS) ]"
	@$(CC) -o $@ $(OBJ) $(LDFLAGS)

$(ALL_OUTPUT_FILES:%=%.__clean__):
	@echo "  (RM)     $(@:%.__clean__=%)"
	@-$(RM) $(@:%.__clean__=%)

clean: $(ALL_OUTPUT_FILES:%=%.__clean__)

etags:
	@echo "  (ETAGS)"
	@find . -name "*.[ch]" | cut -c 3- | xargs etags -I

ifneq ($(MAKECMDGOALS),clean)
-include $(DEP)
endif
