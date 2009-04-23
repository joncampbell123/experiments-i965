all: ds1 ds2 ds3 ds4 ds5 ds6 ds7 ds8 ds9 ring1 ring2 ring3 ring4 ring5 ring6 pt1 ptdump

INTEL_OBJS=find_intel.o util.o mmap.o intelfbhw.o
RINGBUFFER=ringbuffer.o

clean:
	rm -f ds? ring? pt? *.o ptdump

.c.o:
	gcc -c -o $@ $<

pt1: pt1.o $(INTEL_OBJS)
	gcc -o $@ $< $(INTEL_OBJS) $(RINGBUFFER) -lm
ds1: ds1.o $(INTEL_OBJS)
	gcc -o $@ $< $(INTEL_OBJS)
ds2: ds2.o $(INTEL_OBJS)
	gcc -o $@ $< $(INTEL_OBJS)
ds3: ds3.o $(INTEL_OBJS)
	gcc -o $@ $< $(INTEL_OBJS)
ds4: ds4.o $(INTEL_OBJS)
	gcc -o $@ $< $(INTEL_OBJS)
ds5: ds5.o $(INTEL_OBJS)
	gcc -o $@ $< $(INTEL_OBJS)
ds6: ds6.o $(INTEL_OBJS)
	gcc -o $@ $< $(INTEL_OBJS) -lm
ds7: ds7.o $(INTEL_OBJS)
	gcc -o $@ $< $(INTEL_OBJS)
ds8: ds8.o $(INTEL_OBJS)
	gcc -o $@ $< $(INTEL_OBJS)
ds9: ds9.o $(INTEL_OBJS)
	gcc -o $@ $< $(INTEL_OBJS)
ring1: ring1.o $(INTEL_OBJS) $(RINGBUFFER)
	gcc -o $@ $< $(INTEL_OBJS) $(RINGBUFFER)
ring2: ring2.o $(INTEL_OBJS) $(RINGBUFFER)
	gcc -o $@ $< $(INTEL_OBJS) $(RINGBUFFER)
ring3: ring3.o $(INTEL_OBJS) $(RINGBUFFER)
	gcc -o $@ $< $(INTEL_OBJS) $(RINGBUFFER)
ring4: ring4.o $(INTEL_OBJS) $(RINGBUFFER)
	gcc -o $@ $< $(INTEL_OBJS) $(RINGBUFFER) -lm
ring5: ring5.o $(INTEL_OBJS) $(RINGBUFFER)
	gcc -o $@ $< $(INTEL_OBJS) $(RINGBUFFER) -lm
ring6: ring6.o $(INTEL_OBJS) $(RINGBUFFER)
	gcc -o $@ $< $(INTEL_OBJS) $(RINGBUFFER) -lm
ptdump: ptdump.o $(INTEL_OBJS)
	gcc -o $@ $< $(INTEL_OBJS) $(RINGBUFFER) -lm

