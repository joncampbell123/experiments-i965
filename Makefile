all: ds1 ds2 ds3 ds4 ds5 ds6 ds7 ds8 ds9

clean:
	rm -f ds?

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

