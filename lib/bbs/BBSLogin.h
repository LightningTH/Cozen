#ifndef NEKISAHLOTH_LIB_BBS_LOGIN
#define NEKISAHLOTH_LIB_BBS_LOGIN

#include <unistd.h>
#include <stdint.h>
#include <BBSMenu.h>
#include <BBSUser.h>

using namespace std;

class BBSLogin : public BBSMenu
{
	public:

		BBSLogin(ConnectionBase *Conn, BBSRender *Render);
		size_t DoAction(size_t ID);
		void HandleMenu();
		void RenderHelpMenu();

	private:
};

#endif
