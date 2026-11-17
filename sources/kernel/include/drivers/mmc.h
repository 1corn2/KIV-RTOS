#pragma once


#include <stdstring.h>
#include <hal/peripherals.h>
#include <hal/intdef.h>


// GCC gives false alarm notices when using 'char'
typedef unsigned short uc;
typedef const unsigned short cuc;
// This is version 1 only!
// TODO: v2 as a union
struct __attribute__((packed)) CSD_Reg {
	const bool X: 1;
	uc CRC: 7;
	uc reserved: 2;
	uc File_format: 2;
	bool Tmp_write_prot: 1;
	bool Perm_write_prot: 1;
	bool Copy: 1;
	bool File_format_grp: 1;
	cuc reserved2: 5;
	bool Write_bl_partial: 1;
	cuc Write_bl_len: 4;
	cuc R2W_factor: 3;
	cuc reserved3: 2;
	const bool Writeprotect_group_enable: 1;
	cuc Writeprotect_group_size: 7;
	cuc Erase_sector_size: 7;
	const bool Erase_block_enable: 1;
	cuc Device_size_mult: 3;
	const unsigned short _IGNORED: 4*3; // I don't feel like writing these out
	const unsigned short Device_size: 12;
	cuc reserved4: 2;
	const unsigned int _INGORED2: 4*1 + 4 + 12; // I don't feel like writing these out
	const unsigned int _INGORED3: 3*8; // I don't feel like writing these out
	cuc reserved5: 6;
	const bool CSD_struct: 2;

};

static_assert(sizeof(CSD_Reg) == 128/8, "CSD struct has the wrong size");

class MMC {
	public:
		static constexpr const auto MMC_FS_SIZE = 20;
		MMC(uint32_t baseptr);
	private:
		// TODO: ukladat tyhle data na SD karte
		// drzi data
		uint32_t fakefs[MMC_FS_SIZE]{};
		// drzi stav vyuziti
		bool in_use[MMC_FS_SIZE]{};

		// base offset into memory as per repo standard
		volatile uint32_t* const mmc_Base;

		// So we don't have to do dynamic allocations
		CSD_Reg CSD{};
	public:
		uint32_t read_register(hal::EMMC_Reg reg);
		void write_register(hal::EMMC_Reg reg, uint32_t value);
		// shortcut to read and parse the CSD register
		const CSD_Reg* get_CSD() {
			uint32_t regs[4] = {
				read_register(hal::EMMC_Reg::Resp0),
				read_register(hal::EMMC_Reg::Resp1),
				read_register(hal::EMMC_Reg::Resp2),
				read_register(hal::EMMC_Reg::Resp3)
			};
			static_assert(sizeof(regs) == sizeof(CSD), "Sizes do not match");
			memcpy(regs, &CSD, sizeof(CSD));
			return &CSD;
		}

		static inline bool check_bounds(int index) {
			return index >= 0 && index < MMC_FS_SIZE;
		}
		// TODO: rezervovat podle rezimu cteni/psani
		// Zarezervuje block na karte
		bool reserve_block(uint8_t index);
		// uvolni block na pouziti
		bool free_block(uint8_t index);
		// dalsi placeholdery
		// TODO: tyhle by mozna mely vracet, kolik znaku/bloku bylo zapsano/precteno?
		uint32_t read_block(uint8_t index);
		void write_block(uint8_t index, uint32_t data);
};

extern MMC sMMC;
