target = ssd
obj = avlTree.o buffer.o fcl.o flash.o ftl.o initialize.o interface.o ssd.o
CC = gcc

$(target): $(obj)  
    $(CC) $(obj) -o $(target)  
​
%.o: %.c  
    $(CC) -c $< -o $@ 
	
all: ssd
.PHONY: clean  
clean:  
    rm -rf $(obj) $(target) 
