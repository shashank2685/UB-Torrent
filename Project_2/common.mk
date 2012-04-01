
RM      = rm -rf
O	= .o

BASE    = $(basename $(notdir $(@)))

UNAME = $(shell uname)

ifeq ($(UNAME), SunOS)
EXTRA_CFLAGS += $(CFLAGS)  -lresolv -lsocket -lnsl
else
EXTRA_CFLAGS += $(CFLAGS)
endif

ALLCFLAGS       = $(strip \
		$(EXTRA_CFLAGS) \
		$($(BASE)_EXTRA_CFLAGS))

ALLDFLAGS       = $(strip \
		$(EXTRA_DFLAGS)\
		$($(BASE)_EXTRA_DFLAGS))

EXTRA_LDFLAGS  +=$(LDFLAGS) \
		$($(BASE)_EXTRA_LDFLAGS)

ALL_INCLUDEDIRS = $(INCLUDEDIRS)

INCLUDES        = $(strip $(patsubst %,-I%,$(ALL_INCLUDEDIRS)))

#
# LDOBJS and LDLIBS are used to get the list of .o files and the list of librarires required
# to create the target. 
# libraries are declared as dependency with full path.
# use *_LIBS to give the path and use it as dependency as $(*_LIBS) in rule
#
LDOBJS      = $(strip $(filter %$O,$(notdir $^)))

#
# LIBDIRS is used to include library search path when libraries are given as e.g -lssl
#
ALL_LIB_INCLUDE =$(strip $($(BASE)_LIBDIRS) $(LIBDIRS))
LIB_INCLUDE     =$(strip $(patsubst %,-L%,$(ALL_LIB_INCLUDE)))

LDTARGET        = $(notdir $@)
 
BUILD_EXES      = $(EXES)

.PHONY: all
all: $(BUILD_EXES)

#
#  defination of  "<target name>_OBJS" macro used as dependency for all exes
#

$(foreach i, $(EXES), $(eval  $(basename $(i))_OBJS = \
		$(filter %$O, $($(basename $(i))_SRCFILES:.c=$O))))

ifeq ($(UNAME), SunOS)
CC_CMD = \
	$(strip $(CC) $(ALLCFLAGS) $(ALLDFLAGS) $(INCLUDES) -c $< -o $(notdir $@))
else
CC_CMD = \
	$(strip $(CC) $(ALLCFLAGS) $(ALLDFLAGS) $(INCLUDES) -c $< -o $(notdir $@))
endif

LDCMD           = \
		$(strip \
			$(CC) $(ALLCFLAGS) \
			$(EXTRA_LDFLAGS) -o $(LDTARGET) $(LDOBJS) $(LIB_INCLUDE)  $(LDLIBS) \
		)

%$O: %.c
	$(RM) $(notdir $@)
	$(CC_CMD)

$(BUILD_EXES) : $^
	$(RM) $(notdir $@)
	$(LDCMD)


clean:
	$(RM) $(EXES) *.o 

-include *_depend
