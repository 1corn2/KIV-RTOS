#include "drivers/mmc.h"

bool MMC::reserve_block(uint8_t index) {
	if (!check_bounds(index)){
		return false;
	}
	return in_use[index] ? false : (in_use[index] = true);
}

bool MMC::free_block(uint8_t index) {
	if (!check_bounds(index)) {
		return false;
	}
	return in_use[index] ? !(in_use[index] = false) : false;
}

uint32_t MMC::read_block(uint8_t index) {
	return fakefs[index];
}

void MMC::write_block(uint8_t index, uint32_t data) {
	fakefs[index] = data;
}

MMC sMMC{};
