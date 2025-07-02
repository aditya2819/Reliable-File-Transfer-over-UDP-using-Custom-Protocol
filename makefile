CC = gcc
CFLAGS = -g -I./

IMPLEMENTATION = ./impl
INCLUDE = ./include
OBJECT = ./object

IMPLEMENTATION_FILES = $(wildcard $(IMPLEMENTATION)/*.c)
OBJECT_FILES = $(patsubst $(IMPLEMENTATION)/%.c, $(OBJECT)/%.o, $(IMPLEMENTATION_FILES))

x1: $(OBJECT_FILES)
	@mkdir -p recv
	@$(CC) $^ -o run1
	@./run1 6543 6542 
	#@gdb -args ./run1 6543 6542 
	@rm run1

x2: $(OBJECT_FILES)
	@mkdir -p recv
	@$(CC) $^ -o run2
	@./run2 6542 6543
	#@gdb -args ./run2 6542 6543
	@rm run2

$(OBJECT)/%.o : $(IMPLEMENTATION)/%.c
	@mkdir -p $(@D)
	@$(CC) -c -o $@ $< $(CFLAGS)
