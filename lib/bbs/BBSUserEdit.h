#ifndef NEKISAHLOTH_LIB_BBS_USER_EDIT
#define NEKISAHLOTH_LIB_BBS_USER_EDIT

#include <unistd.h>
#include <stdint.h>
#include <BBSMenu.h>
#include <BBSUser.h>

class BBSUserEdit : public BBSMenu
{
	public:

		BBSUserEdit(ConnectionBase *Conn, BBSRender *Render);
		BBSUserEdit(ConnectionBase *Conn, BBSRender *Render, BBSUser *User);
		~BBSUserEdit();
		size_t DoAction(size_t ID);
		const uint8_t RequiredPrivilegeLevel();

	private:
		void Init(bool IsSysop);

		void GenerateMenu();
		void ChangePassword();
		void ChangePrivilegeLevel();

		BBSMenu::MenuEntryStruct *_UserEditMenu;
		BBSMenu::MenuEntryStruct *_UserListMenu;
		bool _IsSysop;
		BBSUser *_EditedUser;
};

#endif
