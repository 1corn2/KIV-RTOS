#include <drivers/mmc.h>
#include <hal/peripherals.h>

MMC::MMC(uint32_t baseptr)
	: mmc_Base(reinterpret_cast<volatile uint32_t* const>(baseptr)) {}

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

uint32_t MMC::read_register(hal::EMMC_Reg reg) {
	return this->mmc_Base[static_cast<uint32_t>(reg)];
}

void MMC::write_register(hal::EMMC_Reg reg, uint32_t value) {
	this->mmc_Base[static_cast<uint32_t>(reg)] = value;
}

MMC sMMC{hal::EMMC_BASE};
