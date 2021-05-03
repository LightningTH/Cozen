#ifndef NEKISAHLOTH_LIB_BBS_STATS
#define NEKISAHLOTH_LIB_BBS_STATS

#include <unistd.h>
#include <stdint.h>
#include <BBSMenu.h>

class BBSStats : public BBSMenu
{
	public:

		BBSStats(ConnectionBase *Conn, BBSRender *Render);
		~BBSStats();
		size_t DoAction(size_t ID);

	private:
		BBSStats(ConnectionBase *Conn, BBSRender *Render, bool ShowGlobal);
		void GenerateMenu(bool ShowGlobal);
		BBSMenu::MenuEntryStruct *_StatMenu;
		BBSStats *_GlobalStatMenu;
};

#endif
