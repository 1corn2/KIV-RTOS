#include "drivers/gpio.h"
#include "fs/filesystem.h"
#include "stdfile.h"


int main (int argc, char *argv[]) {
	auto button = open("DEV:gpio/19", NFile_Open_Mode::Read_Only);
    NGPIO_Interrupt_Type irtype = NGPIO_Interrupt_Type::Rising_Edge;
    ioctl(button, NIOCtl_Operation::Enable_Event_Detection, &irtype);
    uint32_t logpipe = pipe("log", 32);

	auto greetmsg = "Ahoj proces 1";
	auto greetmsglen = strlen(greetmsg)+1;

	char pocitadlo[4] = {13, 12, 11, 10};
	char fileres[4] = {0, 0, 0, 0};
	auto soubor = open("MNT:0", NFile_Open_Mode::Read_Write);

	auto succmsg = "Prace se zdarila";
	auto failmsg = "Prace se nezdarila";

	while (true) {
		wait(button, 1, 0x300);
		// ano, ja vim, ze se tohle delat nema
		write(soubor, pocitadlo, sizeof(pocitadlo));
		auto numread = read(soubor, fileres, sizeof(pocitadlo));

		write(logpipe, greetmsg, greetmsglen);
		if (fileres[0] == pocitadlo[0] && fileres[1] == pocitadlo[1] && fileres[2] == pocitadlo[2] && fileres[3] == pocitadlo[3] && numread == sizeof(uint32_t)) {
			write(logpipe, succmsg, strlen(succmsg)+1);
		} else {
			write(logpipe, failmsg, strlen(failmsg)+1);
		}
	}

	close(button);
	return 0;
}
