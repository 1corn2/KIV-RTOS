#pragma once

#include <hal/intdef.h>
#include <hal/peripherals.h>
#include <stdstring.h>

// GCC gives false alarm notices when using chars in bit fields,
// so we use short, it doesn't matter anyway
//
// also, unions are always aligned,
// so we have to duplicate some fields to get the alignment in the main struct
// right
union __attribute__((packed)) CSD_versions {
	struct __attribute__((packed)) {
		const unsigned short Writeprotect_group_size : 7;
		const unsigned short Erase_sector_size : 7;
		const bool Erase_block_enable : 1;
		const unsigned short Device_size_mult : 3;
		const unsigned short VDD_w_curr_max : 3;
		const unsigned short VDD_w_curr_min : 3;
		const unsigned short VDD_r_curr_max : 3;
		const unsigned short VDD_r_curr_min : 3;
		const unsigned short Device_size : 12;
		const unsigned short reserved4 : 2;
		const bool DSR_impl : 1;
		const bool Read_blk_misalign : 1;
		const bool Write_blk_misalign : 1;
		const bool Read_bl_partial : 1;
		const unsigned short Read_bl_len : 4;
		const unsigned short Card_comm_classes : 12;
		const unsigned int Tran_speed : 8;
		const unsigned int NSAC : 8;
		const unsigned int TAAC : 8;
		const unsigned short reserved5 : 6;
		const unsigned short CSD_struct : 2;
	} v1;
	struct __attribute__((packed)) {
		const unsigned short Writeprotect_group_size : 7;
		const unsigned short Erase_sector_size : 7;
		const bool Erase_block_enable : 1;
		const bool reserved : 1;
		const unsigned int Device_size : 22;
		const unsigned short reserved4 : 6;
		const bool DSR_impl : 1;
		const bool Read_blk_misalign : 1;
		const bool Write_blk_misalign : 1;
		const bool Read_bl_partial : 1;
		const unsigned int Read_bl_len : 4;
		const unsigned int Card_comm_classes : 12;
		const unsigned int Tran_speed : 8;
		const unsigned int NSAC : 8;
		const unsigned int TAAC : 8;
		const unsigned short reserved5 : 6;
		const unsigned short CSD_struct : 2;
	} v2;
};

struct __attribute__((packed)) CSD_Reg {
	const bool X : 1;
	unsigned short CRC : 7;
	unsigned short reserved : 2;
	unsigned short File_format : 2;
	bool Tmp_write_prot : 1;
	bool Perm_write_prot : 1;
	bool Copy : 1;
	bool File_format_grp : 1;
	const unsigned short reserved2 : 5;
	bool Write_bl_partial : 1;
	const unsigned short Write_bl_len : 4;
	const unsigned short R2W_factor : 3;
	const unsigned short reserved3 : 2;
	const bool Writeprotect_group_enable : 1;
	// fields that are different begin here
	CSD_versions versions;
};

static_assert(sizeof(CSD_versions::v1) == sizeof(CSD_versions::v2),
			  "CSD versions have different sizes");
static_assert(sizeof(CSD_Reg) == 16, "CSD struct has the wrong size");

class MMC {
  public:
	// TODO: actually track the card's block size
	static constexpr const auto BLOCK_SIZE = 512;
	// TODO: to bitmap to save some space
	MMC(uint32_t baseptr);

  private:
	// base offset into memory as per repo standard
	volatile uint32_t *const mmc_Base;

	// So we don't have to do dynamic allocations
	CSD_Reg CSD{};

	uint32_t m_rca = 0;
	const CSD_Reg *m_csd = nullptr;
	bool m_initialized = false;

  public:
	uint32_t m_blk_cnt = 0;
	uint32_t read_register(hal::EMMC_Reg reg);
	void write_register(hal::EMMC_Reg reg, uint32_t value);
	// shortcut to read and parse the CSD register
	const CSD_Reg *get_CSD() {
		const uint32_t r0 = read_register(hal::EMMC_Reg::Resp0);
		const uint32_t r1 = read_register(hal::EMMC_Reg::Resp1);
		const uint32_t r2 = read_register(hal::EMMC_Reg::Resp2);
		const uint32_t r3 = read_register(hal::EMMC_Reg::Resp3);

		const uint32_t words[4] = {
			r0 << 8,				// CSD bits 31:0
			(r1 << 8) | (r0 >> 24), // CSD bits 63:32
			(r2 << 8) | (r1 >> 24), // CSD bits 95:64
			(r3 << 8) | (r2 >> 24), // CSD bits 127:96
		};

		static_assert(sizeof(CSD_Reg) == 16, "Unexpected CSD layout");
		static_assert(sizeof(words) == sizeof(CSD), "Sizes do not match");

		memcpy(words, &CSD, sizeof(words));
		return &CSD;
	}

	uint32_t get_true_card_capacity(const CSD_Reg *csd) const {
		if (csd->versions.v1.CSD_struct == 0) {
			auto v1 = csd->versions.v1;
			auto c_size = v1.Device_size;
			auto c_size_mult = v1.Device_size_mult;
			auto read_bl_len = v1.Read_bl_len;
			auto block_len = 2 << (read_bl_len - 1);
			auto mult = 2 << (c_size_mult + 1);
			auto blocknr = (c_size + 1) * mult;
			return blocknr * block_len;
		} else if (csd->versions.v2.CSD_struct == 1) {
			auto v2 = csd->versions.v2;
			return v2.Device_size;
		} else {
			// v3 card, womp womp
			return 0;
		}
	}

	uint32_t get_block_count(const CSD_Reg *csd) const {
		if (csd->versions.v1.CSD_struct == 0) {
			// V1
			auto v1 = csd->versions.v1;
			auto c_size_mult = v1.Device_size_mult;
			auto mult = 2 << (c_size_mult + 1);
			auto c_size = v1.Device_size;
			auto blocknr = (c_size + 1) * mult;
			return blocknr;
		} else if (csd->versions.v2.CSD_struct == 1) {
			// V2
			return (csd->versions.v2.Device_size + 1) * 1024;
		} else {
			// V3, womp womp
			return 0;
		}
	}

	// for userspace, check we are within blocks we care to manage
	inline bool check_bounds(int index) const {
		return index >= 0 && index < m_blk_cnt;
	}

	// TODO: rezervovat podle rezimu cteni/psani
	// Zarezervuje block na karte
	bool reserve_block(uint32_t index);
	// uvolni block na pouziti
	bool free_block(uint32_t index);

	// realne funkce
	bool Write_Block(uint32_t block_index, const uint32_t *buffer_128_words);
	bool Read_Block(uint32_t block_index, uint32_t *buffer_128_words);

	bool Send_Command(uint32_t cmdtm, uint32_t arg, uint32_t *resp0 = nullptr);
	bool Init_SD_Card();

	bool initialized() const;
};

extern MMC sMMC;
