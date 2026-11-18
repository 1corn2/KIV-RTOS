#pragma once


#include <stdstring.h>
#include <hal/peripherals.h>
#include <hal/intdef.h>


// GCC gives false alarm notices when using chars in bit fields,
// so we use short, it doesn't matter anyway
//
// also, unions are always aligned,
// so we have to duplicate some fields to get the alignment in the main struct right
union __attribute__((packed)) CSD_versions {
		struct __attribute__((packed)) {
			const unsigned short Writeprotect_group_size: 7;
			const unsigned short Erase_sector_size: 7;
			const bool Erase_block_enable: 1;
			const unsigned short Device_size_mult: 3;
			const unsigned short _IGNORED: 4*3; // I don't feel like writing these out
			const unsigned short Device_size: 12;
			const unsigned short reserved4: 2;
			const unsigned int _INGORED2: 4*1;
			const unsigned short Read_bl_len: 4;
			const unsigned short _INGORED4: 12; // I don't feel like writing these out
			const unsigned int _INGORED3: 3*8; // I don't feel like writing these out
			const unsigned short reserved5: 6;
			const bool CSD_struct: 2;
		} v1;
		struct __attribute__((packed)) {
			const unsigned short Writeprotect_group_size: 7;
			const unsigned short Erase_sector_size: 7;
			const bool Erase_block_enable: 1;
			const bool reserved: 1;
			const unsigned int Device_size: 22;
			const unsigned short reserved4: 6;
			const unsigned int _IGNORED: 4*1 + 4 + 12;
			const unsigned int _IGNORED2: 3*8;
			const unsigned short reserved5: 6;
			const bool CSD_struct: 2;
		} v2;
};


struct __attribute__((packed)) CSD_Reg {
	const bool X: 1;
	unsigned short CRC: 7;
	unsigned short reserved: 2;
	unsigned short File_format: 2;
	bool Tmp_write_prot: 1;
	bool Perm_write_prot: 1;
	bool Copy: 1;
	bool File_format_grp: 1;
	const unsigned short reserved2: 5;
	bool Write_bl_partial: 1;
	const unsigned short Write_bl_len: 4;
	const unsigned short R2W_factor: 3;
	const unsigned short reserved3: 2;
	const bool Writeprotect_group_enable: 1;
	// fields that are different begin here
	CSD_versions versions;
};

static_assert(sizeof(CSD_versions::v1) == sizeof(CSD_versions::v2), "CSD versions have different sizes");
static_assert(sizeof(CSD_Reg) == 16, "CSD struct has the wrong size");

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

		uint32_t get_true_card_capacity(const CSD_Reg* csd) const
		{
			auto v1 = csd->versions.v1;
			auto c_size = v1.Device_size;
			auto c_size_mult = v1.Device_size_mult;
			auto read_bl_len = v1.Read_bl_len;
			auto block_len = 2 << (read_bl_len-1);
			auto mult = 2 << (c_size_mult+1);
			auto blocknr = (c_size+1)*mult;
			return blocknr * block_len;
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
