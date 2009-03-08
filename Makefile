all: ds1 ds2 ds3 ds4 ds5 ds6 ds7 ds8 ds9 ring1 ring2 ring3 ring4

clean:
	rm -f ds? ring?

ds1: ds1.c
	gcc -o $@ $<
ds2: ds2.c
	gcc -o $@ $<
ds3: ds3.c
	gcc -o $@ $<
ds4: ds4.c
	gcc -o $@ $<
ds5: ds5.c
	gcc -o $@ $<
ds6: ds6.c
	gcc -o $@ $< -lm
ds7: ds7.c
	gcc -o $@ $<
ds8: ds8.c
	gcc -o $@ $<
ds9: ds9.c
	gcc -o $@ $<

ring1: ring1.c
	gcc -o $@ $<
ring2: ring2.c
	gcc -o $@ $<
ring3: ring3.c
	gcc -o $@ $<
ring4: ring4.c
	gcc -o $@ $< -lm

