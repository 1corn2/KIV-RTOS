#include <hal/intdef.h>

class MMC {
	public:
		static constexpr const auto MMC_FS_SIZE = 20;
	private:
		// TODO: ukladat tyhle data na SD karte
		// drzi data
		uint32_t fakefs[MMC_FS_SIZE]{};
		// drzi stav vyuziti
		bool in_use[MMC_FS_SIZE]{};
	public:
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
