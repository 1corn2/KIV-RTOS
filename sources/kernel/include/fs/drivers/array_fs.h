#include <fs/filesystem.h>
#include <stdstring.h>
#include <fs/drivers/mmc_fs.h>


// passthrough for MMC_FS
class ARRAY_FS_driver : public IFilesystem_Driver {
	public:
        virtual void On_Register() override {
            //
        }

        virtual IFile* Open_File(const char* path, NFile_Open_Mode mode) override {
			return fsMMC_FS_Driver.Open_File(path, mode);
		}
};

ARRAY_FS_driver fsARRAY_FS_driver;
