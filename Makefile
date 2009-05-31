all: ds1 ds2 ds3 ds4 ds5 ds6 ds7 ds8 ds9 ring1 ring2 ring3 ring4 ring5 ring6 ring7 ring8 ring9 ring10 ring11

INTEL_OBJS=find_intel.o util.o mmap.o intelfbhw.o pgtable.o uvma.o
RINGBUFFER=ringbuffer.o

clean:
	rm -f ds? ring? ring1{0,1} *.o

.c.o:
	gcc -mmmx -msse -msse2 -c -o $@ $<

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
ring7: ring7.o $(INTEL_OBJS) $(RINGBUFFER)
	gcc -o $@ $< $(INTEL_OBJS) $(RINGBUFFER) -lm
ring8: ring8.o $(INTEL_OBJS) $(RINGBUFFER)
	gcc -o $@ $< $(INTEL_OBJS) $(RINGBUFFER) -lm
ring9: ring9.o $(INTEL_OBJS) $(RINGBUFFER)
	gcc -o $@ $< $(INTEL_OBJS) $(RINGBUFFER) -lm
ring10: ring10.o $(INTEL_OBJS) $(RINGBUFFER)
	gcc -o $@ $< $(INTEL_OBJS) $(RINGBUFFER) -lm
ring11: ring11.o $(INTEL_OBJS) $(RINGBUFFER)
	gcc -o $@ $< $(INTEL_OBJS) $(RINGBUFFER) -lm

