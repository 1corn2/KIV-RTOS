#include <fs/filesystem.h>
#include <stdstring.h>
#include <fs/drivers/mmc_fs.h>


// passthrough for MMC_FS
class ARRAY_FS_driver : public IFilesystem_Driver {
	uint32_t block_count;
	public:
        virtual void On_Register() override {
            block_count = fsMMC_FS_Driver.Get_Block_Count();
        }

        virtual IFile* Open_File(const char* path, NFile_Open_Mode mode) override {
			char tmpbuf[32];
			uint32_t block_index = atoi(path);

			// go from the back
			block_index = block_count - block_index - 1;

			itoa(block_index, tmpbuf, 10);

			return fsMMC_FS_Driver.Open_File(tmpbuf, mode);
		}
};

ARRAY_FS_driver fsARRAY_FS_driver;
