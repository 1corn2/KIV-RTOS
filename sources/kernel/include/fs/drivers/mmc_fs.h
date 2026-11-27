#include <drivers/mmc.h>
#include <drivers/uart.h>
#include <fs/filesystem.h>
#include <hal/peripherals.h>
#include <stdstring.h>

class MMC_File final : public IFile {
  private:
	const uint32_t blocknumber;
	uint16_t index = 0;
	const uint32_t filesize = MMC::BLOCK_SIZE; // TODO: get real file size
	const NFile_Open_Mode mode;

  public:
	MMC_File(uint32_t ind, NFile_Open_Mode opmod)
		: IFile(NFile_Type_Major::Block), blocknumber{ind}, mode{opmod} {};
	~MMC_File() {}

	// cte ze souboru do bufferu, num je velikost bufferu (maximalni pocet bytu
	// k precteni); vraci skutecne precteny pocet znaku
	virtual uint32_t Read(char *buffer, uint32_t num) override {
		if (mode == NFile_Open_Mode::Write_Only) {
			return 0;
		}
		char ourbuf[512];
		sMMC.Read_Block(this->blocknumber, reinterpret_cast<uint32_t *>(ourbuf));

		auto realnum = 0;
		if (index + num > MMC::BLOCK_SIZE) {
			realnum = MMC::BLOCK_SIZE - index;
		} else {
			realnum = num;
		}
		memcpy(ourbuf + index, buffer, realnum);
		index += realnum;
		return realnum;
	};

	// zapise z bufferu do souboru, num je velikost bufferu (maximalni pocet
	// bytu k zapsani); vraci skutecne zapsany pocet znaku
	virtual uint32_t Write(const char *buffer, uint32_t num) override {
		if (mode == NFile_Open_Mode::Read_Only) {
			return 0;
		}
		char ourbuf[512];
		sMMC.Read_Block(this->blocknumber, reinterpret_cast<uint32_t *>(ourbuf));

		auto realnum = 0;
		if (index + num > MMC::BLOCK_SIZE) {
			realnum = MMC::BLOCK_SIZE - index;
		} else {
			realnum = num;
		}

		memcpy(buffer, ourbuf + index, realnum);
		index += realnum;
		sMMC.Write_Block(this->blocknumber, reinterpret_cast<uint32_t *>(ourbuf));
		return realnum;
	};

	virtual bool Close() override {
		sMMC.free_block(this->blocknumber);
		return IFile::Close();
	}

	virtual bool Seek(uint32_t offset) override {
		if (offset > this->filesize) {
			return false;
		} else {
			index = offset;
			return true;
		}
	}

	virtual uint32_t Tell() const override {
		return this->index;
	}
};

class MMC_FS_driver : public IFilesystem_Driver {
  public:
	virtual void On_Register() override {
		sMMC.Init_SD_Card();
	}

	virtual IFile *Open_File(const char *path, NFile_Open_Mode mode) override {
		if (!sMMC.initialized()) {
			return nullptr;
		}

		// nase FS cesty jsou prozatim jen ciselne indexy
		auto filepath = atoi(path);
		if (filepath < 0) {
			return nullptr;
		}

		if (!sMMC.reserve_block(filepath)) {
			return nullptr;
		}

		auto res = new MMC_File(filepath, mode);
		return res;
	}

	uint32_t Get_Block_Count() const {
		return sMMC.m_blk_cnt;
	}
};

MMC_FS_driver fsMMC_FS_Driver;
