MODE= -O2 -DUNIX
decode : decode.c musicout.c common.c common.h decoder.h huffman.h 

HEDS =  common.h decoder.h huffman.h

OBJSD = decode.o musicout.o common.o huffman.o

LFLAG = 

PROGRAMD =  decode

.c.o: $(HEDS)
	cc -c $(MODE) $*.c

all: $(PROGRAM) $(PROGRAMD)

default: all


$(PROGRAMD) : $(OBJSD) $(HEDS)
	cc -o $(PROGRAMD) $(LFLAG) $(OBJSD) -lm

clean:
	rm $(OBJSD)

veryclean:
	rm $(PROGRAM) $(OBJS)
