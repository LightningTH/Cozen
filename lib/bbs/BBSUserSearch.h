#ifndef NEKISAHLOTH_LIB_BBS_USER_SEARCH
#define NEKISAHLOTH_LIB_BBS_USER_SEARCH

#include <unistd.h>
#include <stdint.h>
#include <BBSMenu.h>
#include <BBSUser.h>

class BBSUserSearch : public BBSMenu
{
	public:

		BBSUserSearch(ConnectionBase *Conn, BBSRender *Render);
		~BBSUserSearch();
		size_t DoAction(size_t ID);
		const uint8_t RequiredPrivilegeLevel();
		BBSUser *GetUser();

	private:
		void GenerateFindMenu();
		void DisplaySearchMenu();

		void SearchByName();
		void SearchByID();

		BBSMenu::MenuEntryStruct *_UserListMenu;
		static const int USER_MATCH_COUNT = 16;
		BBSUser *_UserMatches[USER_MATCH_COUNT];
		BBSUser *_FindUser;
		BBSUser *_SelectedUser;
};

#endif
