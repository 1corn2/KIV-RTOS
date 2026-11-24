#include <drivers/uart.h>
#include <hal/peripherals.h>
#include <fs/filesystem.h>
#include <stdstring.h>
#include <drivers/mmc.h>

#include "stdfile.h"

class MMC_File final : public IFile {
	private:
		uint8_t index;
	public:
		// TODO: novy file type major::block
		MMC_File(uint8_t ind): IFile(NFile_Type_Major::Unspecified), index{ind}{};
		~MMC_File() {

		}
		// TODO: seek, tell, atd.

        // cte ze souboru do bufferu, num je velikost bufferu (maximalni pocet bytu k precteni); vraci skutecne precteny pocet znaku
        virtual uint32_t Read(char* buffer, uint32_t num) override {
			auto tmp = sMMC.read_block(this->index);
			// vracime idealne cele cislo, jinak pokud jsme dostali moc maly buffer, tak jen tolik, co muzeme
			auto realnum = num >= sizeof(tmp) ? sizeof(tmp) : num;
			memcpy(&tmp, buffer, realnum);
			return realnum;
		};
        // zapise z bufferu do souboru, num je velikost bufferu (maximalni pocet bytu k zapsani); vraci skutecne zapsany pocet znaku
        virtual uint32_t Write(const char* buffer, uint32_t num) override {
			uint32_t tmp{};
			// stejne jako nahore, zastresujeme mnozstvi napsanych bytu na velikosti uint32_t
			auto realnum = num >= sizeof(tmp) ? sizeof(tmp) : num;
			memcpy(buffer, &tmp, realnum);
			sMMC.write_block(this->index, tmp);
			return realnum;
		};

		virtual bool Close() override {
			sMMC.free_block(this->index);
			return IFile::Close();
		}
};

static void WriteChars(uint32_t str)
{
	char stack[10] = {}; // 2^32 ~= 4 000 000 000
	if (str == 0) {
		sUART0.Write('0');
		return;
	}
	int stckptr = 0;
	while (str > 0) {
		stack[stckptr++] = (str % 10) + '0';
		str /= 10;
	}
	for (int i = stckptr-1; i >= 0; i--) {
		sUART0.Write(stack[i]);
	}
}


class MMC_FS_driver : public IFilesystem_Driver {
private:
    uint32_t m_rca = 0;
    const CSD_Reg* m_csd = nullptr;
    bool m_initialized = false;



    bool Write_Block(uint32_t block_index, const uint32_t* buffer_128_words) {
    if (!m_initialized || m_csd == nullptr || buffer_128_words == nullptr) {
        return false;
    }

    // 1. Wait until both Command (bit 0) and Data (bit 1) lines are free
    while (sMMC.read_register(hal::EMMC_Reg::Status) & ((1 << 0) | (1 << 1))) {}

    // 2. Configure Block Size (512 bytes) and Block Count (1 block)
    sMMC.write_register(hal::EMMC_Reg::Blksizecnt, (1 << 16) | 512);

    // 3. Calculate address argument:
    // SDHC/SDXC (CSD v2.0) uses block addressing; SDSC (CSD v1.0) uses byte addressing
    uint32_t address = (m_csd->versions.v2.CSD_struct == 1)
                       ? block_index
                       : (block_index * 512);

    // 4. Clear all interrupt flags
    sMMC.write_register(hal::EMMC_Reg::Interrupt, 0xFFFFFFFF);

    // 5. Build CMD24 (WRITE_BLOCK):
    // - Index 24:        24 << 24
    // - CMD_ISDATA:      1 << 21  (command involves data transfer)
    // - CMD_IXCHK_EN:    1 << 20  (check response index)
    // - CMD_CRCCHK_EN:   1 << 19  (check response CRC)
    // - CMD_RSPNS_TYPE:  2 << 16  (48-bit R1 response)
    // - TM_DAT_DIR:      0 << 4   (0 = transfer from host to card)
    uint32_t cmd24 = (24 << 24) | (1 << 21) | (1 << 20) | (1 << 19) | (2 << 16);

    sMMC.write_register(hal::EMMC_Reg::Arg1, address);
    sMMC.write_register(hal::EMMC_Reg::Cmdtm, cmd24);

    // 6. Wait for Command Done (bit 0) or Error (bit 15)
    uint32_t irq = 0;
    do {
        irq = sMMC.read_register(hal::EMMC_Reg::Interrupt);
    } while (!(irq & ((1 << 0) | (1 << 15))));

    sMMC.write_register(hal::EMMC_Reg::Interrupt, (1 << 0)); // Clear CMD_DONE
    if (irq & (1 << 15)) {
        return false;
    }

    // 7. Wait for Write Ready (bit 4 = WRITE_RDY) or Error (bit 15)
    do {
        irq = sMMC.read_register(hal::EMMC_Reg::Interrupt);
    } while (!(irq & ((1 << 4) | (1 << 15))));

    sMMC.write_register(hal::EMMC_Reg::Interrupt, (1 << 4)); // Clear WRITE_RDY
    if (irq & (1 << 15)) {
        return false;
    }

    // 8. Write 512 bytes (128 x 32-bit words) into the DATA register
    for (int i = 0; i < 128; i++) {
        sMMC.write_register(hal::EMMC_Reg::Data, buffer_128_words[i]);
    }

    // 9. Wait for Data Transfer Done (bit 1 = DATA_DONE) or Error (bit 15)
    // The controller waits for the card to release its busy signal before setting DATA_DONE
    do {
        irq = sMMC.read_register(hal::EMMC_Reg::Interrupt);
    } while (!(irq & ((1 << 1) | (1 << 15))));

    sMMC.write_register(hal::EMMC_Reg::Interrupt, (1 << 1)); // Clear DATA_DONE
    if (irq & (1 << 15)) {
        return false;
    }

    return true;
}



    bool Read_Block(uint32_t block_index, uint32_t* buffer_128_words) {
    if (!m_initialized || m_csd == nullptr || buffer_128_words == nullptr) {
        return false;
    }

    // 1. Wait until both Command (bit 0) and Data (bit 1) lines are free
    while (sMMC.read_register(hal::EMMC_Reg::Status) & ((1 << 0) | (1 << 1))) {}

    // 2. Configure Block Size (512 bytes) and Block Count (1 block)
    // BLKCNT = 1 (bits 31:16), BLKSIZE = 512 (bits 9:0)
    sMMC.write_register(hal::EMMC_Reg::Blksizecnt, (1 << 16) | 512);

    // 3. Calculate address argument:
    // SDHC/SDXC (CSD v2.0) uses block addressing; SDSC (CSD v1.0) uses byte addressing
    uint32_t address = (m_csd->versions.v2.CSD_struct == 1)
                       ? block_index
                       : (block_index * 512);

    // 4. Clear all interrupt flags
    sMMC.write_register(hal::EMMC_Reg::Interrupt, 0xFFFFFFFF);

    // 5. Build CMD17 (READ_SINGLE_BLOCK):
    // - Index 17:        17 << 24
    // - CMD_ISDATA:      1 << 21  (command involves data transfer)
    // - CMD_IXCHK_EN:    1 << 20  (check response index)
    // - CMD_CRCCHK_EN:   1 << 19  (check response CRC)
    // - CMD_RSPNS_TYPE:  2 << 16  (48-bit R1 response)
    // - TM_DAT_DIR:      1 << 4   (1 = transfer from card to host)
    uint32_t cmd17 = (17 << 24) | (1 << 21) | (1 << 20) | (1 << 19) | (2 << 16) | (1 << 4);

    sMMC.write_register(hal::EMMC_Reg::Arg1, address);
    sMMC.write_register(hal::EMMC_Reg::Cmdtm, cmd17);

    // 6. Wait for Command Done (bit 0) or Error (bit 15)
    uint32_t irq = 0;
    do {
        irq = sMMC.read_register(hal::EMMC_Reg::Interrupt);
    } while (!(irq & ((1 << 0) | (1 << 15))));

    sMMC.write_register(hal::EMMC_Reg::Interrupt, (1 << 0)); // Clear CMD_DONE
    if (irq & (1 << 15)) {
        return false;
    }

    // 7. Wait for Read Ready (bit 5 = READ_RDY) or Error (bit 15)
    do {
        irq = sMMC.read_register(hal::EMMC_Reg::Interrupt);
    } while (!(irq & ((1 << 5) | (1 << 15))));

    sMMC.write_register(hal::EMMC_Reg::Interrupt, (1 << 5)); // Clear READ_RDY
    if (irq & (1 << 15)) {
        return false;
    }

    // 8. Read 512 bytes (128 x 32-bit words) from the DATA register
    for (int i = 0; i < 128; i++) {
        buffer_128_words[i] = sMMC.read_register(hal::EMMC_Reg::Data);
    }

    // 9. Wait for Data Transfer Done (bit 1 = DATA_DONE) or Error (bit 15)
    do {
        irq = sMMC.read_register(hal::EMMC_Reg::Interrupt);
    } while (!(irq & ((1 << 1) | (1 << 15))));

    sMMC.write_register(hal::EMMC_Reg::Interrupt, (1 << 1)); // Clear DATA_DONE
    if (irq & (1 << 15)) {
        return false;
    }

    return true;
}




    bool Send_Command(uint32_t cmdtm, uint32_t arg, uint32_t* resp0 = nullptr) {
        // Čekání na uvolnění příkazové linky (CMD_INHIBIT == 0)
        while (sMMC.read_register(hal::EMMC_Reg::Status) & (1 << 0)) {}

        // Vymazání předchozích příznaků přerušení
        sMMC.write_register(hal::EMMC_Reg::Interrupt, 0xFFFFFFFF);

        // Nastavení argumentu a odeslání příkazu
        sMMC.write_register(hal::EMMC_Reg::Arg1, arg);
        sMMC.write_register(hal::EMMC_Reg::Cmdtm, cmdtm);

        // Čekání na CMD_DONE (bit 0) nebo ERR (bit 15)
        uint32_t irq = 0;
        do {
            irq = sMMC.read_register(hal::EMMC_Reg::Interrupt);
        } while (!(irq & ((1 << 0) | (1 << 15))));

        // Potvrzení a vymazání příznaků
        sMMC.write_register(hal::EMMC_Reg::Interrupt, irq);

        // Kontrola chybového bitu ERR (bit 15)
        if (irq & (1 << 15)) {
            return false;
        }

        if (resp0 != nullptr) {
            *resp0 = sMMC.read_register(hal::EMMC_Reg::Resp0);
        }
        return true;
    }

    bool Init_SD_Card() {
        // 1. Vypnutí SPI módu a reset hostitelského obvodu (SRST_HC, bit 24)
        sMMC.write_register(hal::EMMC_Reg::Control0, 0);
        sMMC.write_register(hal::EMMC_Reg::Control1, (1 << 24));
        while (sMMC.read_register(hal::EMMC_Reg::Control1) & (1 << 24)) {}

        // 2. Nastavení hodin (~400 kHz pro identifikaci) a timeoutu
        uint32_t ctrl1 = (0xE << 16) | (125 << 8) | (1 << 0);
        sMMC.write_register(hal::EMMC_Reg::Control1, ctrl1);
        while (!(sMMC.read_register(hal::EMMC_Reg::Control1) & (1 << 1))) {}

        ctrl1 |= (1 << 2); // CLK_EN
        sMMC.write_register(hal::EMMC_Reg::Control1, ctrl1);

        // 3. Povolení stavových příznaků v registru INTERRUPT
        sMMC.write_register(hal::EMMC_Reg::Irpt_mask, 0xFFFFFFFF);

        // 4. CMD0 (GO_IDLE_STATE) - reset karty do Idle stavu
        if (!Send_Command(0 << 24, 0)) {
            return false;
        }

        // 5. CMD8 (SEND_IF_COND) - ověření napětí 2.7-3.6 V
        uint32_t resp = 0;
        uint32_t cmd8 = (8 << 24) | (1 << 20) | (1 << 19) | (2 << 16);
        if (!Send_Command(cmd8, 0x000001AA, &resp) || (resp & 0xFFF) != 0x1AA) {
            return false;
        }

        // 6. ACMD41 (CMD55 + CMD41) - inicializace karty (HCS = 1)
        uint32_t cmd55 = (55 << 24) | (1 << 20) | (1 << 19) | (2 << 16);
        uint32_t acmd41 = (41 << 24) | (2 << 16);
        uint32_t acmd41_arg = (1 << 30) | 0x00FF8000;

        do {
            if (!Send_Command(cmd55, 0)) {
                return false;
            }
            if (!Send_Command(acmd41, acmd41_arg, &resp)) {
                return false;
            }
            for (volatile int i = 0; i < 100000; i++) {}
        } while (!(resp & (1U << 31)));

        // 7. CMD2 (ALL_SEND_CID) - čtení CID
        uint32_t cmd2 = (2 << 24) | (1 << 19) | (1 << 16);
        if (!Send_Command(cmd2, 0)) {
            return false;
        }

        // 8. CMD3 (SEND_RELATIVE_ADDR) - získání RCA adresy karty
        uint32_t cmd3 = (3 << 24) | (1 << 20) | (1 << 19) | (2 << 16);
        if (!Send_Command(cmd3, 0, &resp)) {
            return false;
        }
        m_rca = resp >> 16;

        // 9. CMD9 (SEND_CSD) - přečtení CSD registru (karta je ve stavu Stand-by)
        uint32_t cmd9 = (9 << 24) | (1 << 19) | (1 << 16);
        if (!Send_Command(cmd9, m_rca << 16)) {
            return false;
        }

        // Uložit CSD hned po CMD9, než další příkaz (CMD7) přepíše RESP0..3
        m_csd = sMMC.get_CSD();

        // 10. CMD7 (SELECT_CARD) - přepnutí karty do Transfer stavu pro datové přenosy
        uint32_t cmd7 = (7 << 24) | (1 << 20) | (1 << 19) | (3 << 16);
        if (!Send_Command(cmd7, m_rca << 16)) {
            return false;
        }

        return true;
    }

public:
    virtual void On_Register() override {
        m_initialized = Init_SD_Card();
    }

    virtual IFile* Open_File(const char* path, NFile_Open_Mode mode) override {
        if (!m_initialized || m_csd == nullptr) {
            return nullptr;
        }


        sUART0.Write("POCET BLOKU\n");

        uint32_t pocet_bloku = sMMC.get_block_count(m_csd);

        WriteChars(pocet_bloku);
        sUART0.Write("\n");
        sUART0.Write("POSLEDNI BLOK=\n");




        uint32_t block0[128]; // 128 * 4 = 512 bytes
        if (Read_Block(pocet_bloku-1, block0)) {
            // Check the standard boot sector signature at bytes 510-511 (top 16 bits of word 127)
            uint16_t boot_signature = (block0[127] >> 16) & 0xFFFF;
            // If valid MBR/FAT, boot_signature will be 0xAA55
            WriteChars(boot_signature);
        }

        for (int i = 0; i < 128; i++) {
            WriteChars(block0[i]);
        }





        ///WRITE TO BLOCK
        ///
        ///

        block0[127]=block0[127]+1;
        Write_Block(pocet_bloku-1, block0);

        sUART0.Write("ZAPIS HOTOV\n");








            // sUART0.Write("V2 Device size: ");
            // WriteChars(m_csd->versions.v2.Device_size);
            // sUART0.Write("\nV1 Multiplier: ");
            // WriteChars(m_csd->versions.v1.Device_size_mult);
            // sUART0.Write("\n");
            // sUART0.Write("\ntrue_card_capacity: ");
            // WriteChars(sMMC.get_true_card_capacity(m_csd));
            // sUART0.Write("\n");
            // sUART0.Write("\nCSD_struct NUMBER: ");
            // WriteChars(m_csd->versions.v1.CSD_struct);


        // nase FS cesty jsou prozatim jen ciselne indexy
        auto filepath = atoi(path);
        if (filepath < 0 || filepath > MMC::MMC_FS_SIZE) {
            return nullptr;
        }

        if (!sMMC.reserve_block(filepath)) {
            return nullptr;
        }

        auto res = new MMC_File(filepath);
        return res;
    }
};
MMC_FS_driver fsMMC_FS_Driver;
