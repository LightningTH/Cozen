#ifndef NEKISAHLOTH_LIB_BBS_SYSOP
#define NEKISAHLOTH_LIB_BBS_SYSOP

#include <unistd.h>
#include <stdint.h>
#include <BBSMenu.h>

class BBSSysop : public BBSMenu
{
	public:

		BBSSysop(ConnectionBase *Conn, BBSRender *Render);
		size_t DoAction(size_t ID);
		const uint8_t RequiredPrivilegeLevel();			//required level to see this menu, 255 = SYSOP

	private:
		void InitSysop();
		size_t AnnouncementID;
		size_t FileID;
};

#endif
