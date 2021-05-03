#ifndef NEKISAHLOTH_LIB_BBS_UTILS
#define NEKISAHLOTH_LIB_BBS_UTILS

#include <unistd.h>
#include <stdint.h>
#include <BBSMenu.h>

class BBSUtils : public BBSMenu
{
	public:

		BBSUtils(ConnectionBase *Conn, BBSRender *Render);
		size_t DoAction(size_t ID);

	private:
		void PrintStats();
};

#endif
