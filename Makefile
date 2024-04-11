CC    := gcc
DIR   := $(PWD)
BDIR := $(DIR)/build
LDIR := $(DIR)/log
LAUNCHER_SRC := $(wildcard src/launcher/*.c src/launcher/*.h)
LAUNCHER_SINGLE_SRC := $(wildcard src/launcher_single/*.c src/launcher_single/*.h)
LIB := $(wildcard src/lib/*.c src/lib/*.h)
NAME_RUNTIME_DAEMON_LIB := runtime_daemon.so
NAME_FUNC_LIB := main.so
RUNTIME_DAEMON_LIB_SRC := $(wildcard src/runtime_daemon/*.c src/runtime_daemon/*.h)
FUNC_LIB_SRC := $(wildcard src/func/*.c src/func/*.h)
CFLAGS += -Wall -Wno-implicit-function-declaration
CFLAGS_DEBUG := -g $(CFLAGS) 
LFLAGS := -lpthread -lm -ldl -rdynamic

# multi-containers test
all: $(BDIR) $(LDIR) runtime_daemon func proxy

proxy:
	@$(CC) $(CFLAGS) $(LAUNCHER_SRC) $(LIB) -o $(BDIR)/proxy $(LFLAGS) 
func:
	@$(CC) $(CFLAGS) -shared -fPIC $(FUNC_LIB_SRC) $(LIB) $(LFLAGS) -o $(BDIR)/$(NAME_FUNC_LIB)
runtime_daemon:
	@$(CC) $(CFLAGS) -shared -fPIC $(RUNTIME_DAEMON_LIB_SRC) $(LIB) $(LFLAGS) -o $(BDIR)/$(NAME_RUNTIME_DAEMON_LIB)

# multi-containers debug
debug: $(BDIR) $(LDIR) runtime_daemon_debug func_debug proxy_debug

proxy_debug:
	@$(CC) $(CFLAGS_DEBUG) $(LAUNCHER_SRC) $(LIB) -o $(BDIR)/proxy_debug $(LFLAGS)
func_debug:
	@$(CC) $(CFLAGS_DEBUG) -shared -fPIC $(FUNC_LIB_SRC) $(LIB) $(LFLAGS) -o $(BDIR)/$(NAME_FUNC_LIB)
runtime_daemon_debug:
	@$(CC) $(CFLAGS_DEBUG) -shared -fPIC $(RUNTIME_DAEMON_LIB_SRC) $(LIB) $(LFLAGS) -o $(BDIR)/$(NAME_RUNTIME_DAEMON_LIB)

# single-container test
single: $(BDIR) $(LDIR) runtime_daemon func launcher_single

launcher_single:
	@$(CC) $(CFLAGS) $(LAUNCHER_SINGLE_SRC) $(LIB) -o $(BDIR)/launcher_single $(LFLAGS) 

# single-container debug
single_debug: $(BDIR) $(LDIR) runtime_daemon_debug func_debug launcher_single_debug

launcher_single_debug:
	@$(CC) $(CFLAGS_DEBUG) $(LAUNCHER_SINGLE_SRC) $(LIB) -o $(BDIR)/launcher_single_debug $(LFLAGS) 

runtests:
	@chmod a+x ./run.sh
	@./run.sh

clean_output:
	$(RM) -r $(DIR)/out*

clean:
	$(RM) -r $(BDIR) *.o

$(BDIR):
	@mkdir -p $@

$(LDIR):
	@mkdir -p $@
