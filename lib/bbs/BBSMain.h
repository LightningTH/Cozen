#ifndef NEKISAHLOTH_LIB_BBS_MAIN
#define NEKISAHLOTH_LIB_BBS_MAIN

#include <unistd.h>
#include <stdint.h>
#include <BBSMenu.h>

class BBSMain : public BBSMenu
{
	public:

		BBSMain(ConnectionBase *Conn, BBSRender *Render);
		size_t DoAction(size_t ID);
		const uint8_t RequiredPrivilege();			//required level to see this menu, 255 = SYSOP
		void Refresh();

	private:
		void InitMain();

		BBSMenu::MenuEntryStruct *SystemMenu;
		size_t AnnouncementID;
		size_t FileID;
		size_t MessageID;
};

#endif
