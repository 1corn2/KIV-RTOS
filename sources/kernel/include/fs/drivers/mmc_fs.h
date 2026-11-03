#include <fs/filesystem.h>
#include <stdstring.h>
#include <drivers/mmc.h>

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

class MMC_FS_driver : public IFilesystem_Driver {
	public:
        virtual void On_Register() override {
            //
        }

        virtual IFile* Open_File(const char* path, NFile_Open_Mode mode) override {

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
