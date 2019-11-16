extern  long _fbss[];
extern	long _end[];
extern	long _gp[];
extern	void main();
register long *gp __asm__("gp");

void _start()
{
	long *adr;

	for(adr = _fbss;adr!=_end;*adr++=0);
	gp = _gp;
	main();
}

void __main()
{
}
