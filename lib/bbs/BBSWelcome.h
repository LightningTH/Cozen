#ifndef NEKISAHLOTH_LIB_BBS_WELCOME
#define NEKISAHLOTH_LIB_BBS_WELCOME

#include <unistd.h>
#include <stdint.h>
#include <ConnectionBase.h>
#include <BBSMenu.h>

class BBSWelcome : public BBSMenu
{
	public:
		BBSWelcome(ConnectionBase *Conn, BBSRender *Render);
		size_t DoAction(size_t ID);
		void RenderHelpMenu();
		void RenderMenu();

};

#endif
