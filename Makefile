CC     = c99
CFLAGS = -Wall -Wextra -Ofast -fopenmp
LDLIBS = -lm
all: genetic render
clean:
	rm -f generic render best.txt video.ppm progress.pgm _progress.pgm
genetic: genetic.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ genetic.c $(LDLIBS)
render: render.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ render.c $(LDLIBS)
