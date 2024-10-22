default:
	$(CROSS_COMPILE)gcc -c finder-app/writer.c -o writer.o && $(CROSS_COMPILE)gcc writer.o -o writer
clean:
	rm -f writer.o writer
