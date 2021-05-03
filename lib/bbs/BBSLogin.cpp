#include <stdio.h>
#include <string.h>
#include "BBSLogin.h"

using namespace std;

__attribute__((noreturn)) void DoExit()
{
	close(0);
	close(1);
	close(2);
	_exit(0);
}

BBSLogin::BBSLogin(ConnectionBase *Conn, BBSRender *Render) :
	BBSMenu(Conn, Render)
{
}

size_t BBSLogin::DoAction(size_t ID)
{
	return 1;
}

void BBSLogin::RenderHelpMenu()
{
}

void BBSLogin::HandleMenu()
{
	char Username[100];
	char Password[100];
	BBSUser *UserLogin;
	int PasswordAttempt;
	BBSUser::ColorChoice ColorChoice;

    _Render->Clear();

	//draw our login screen
	_Render->DrawHeader(BBSRender::COLOR_BLACK, BBSRender::COLOR_YELLOW, "LOGIN");
	_Render->OffsetY(1);
	_Render->PrintF("Username: ", BBSRender::COLOR_BLACK, BBSRender::COLOR_RED);
	_Render->Render(BBSRender::COLOR_WHITE);

	memset(Username, 0, sizeof(Username));
	if(_Conn->Readline(Username, sizeof(Username) - 1) == 0)
		DoExit();

	//add the username to the render screen
	_Render->PrintF("%s\n", BBSRender::COLOR_BLACK, BBSRender::COLOR_BLUE, Username);
	_Render->SetX(0);

	//see if the username exists
	UserLogin = new BBSUser(Username);
	if(UserLogin->IsNewUser())
	{
		_Render->PrintF("User does not exist, create new user [Y/n]? ", BBSRender::COLOR_BLACK, BBSRender::COLOR_MAGENTA);
		_Render->Render(BBSRender::COLOR_YELLOW);

		if(_Conn->Readline(Password, 2) == 0)
			DoExit();

		if((Password[0] == 'n') || (Password[0] == 'N'))
			DoExit();

		//user doesn't exist, create a new one
		Password[1] = 0;
		_Render->PrintF("%s\n", BBSRender::COLOR_BLACK, BBSRender::COLOR_BLUE, Password);
		_Render->SetX(0);
		_Render->PrintF("Creating new user\n", BBSRender::COLOR_BLACK, BBSRender::COLOR_MAGENTA);
	}

	for(PasswordAttempt = 0; PasswordAttempt < 3; PasswordAttempt++)
	{
		_Render->PrintF("Password: ", BBSRender::COLOR_BLACK, BBSRender::COLOR_RED);
		_Render->Render(BBSRender::COLOR_BLACK, true);

		memset(Password, 0, sizeof(Password));
		_Conn->Readline(Password, sizeof(Password)-1);
		_Render->Render(BBSRender::COLOR_BLACK);

		_Render->OffsetY(1);
		_Render->SetX(0);

		if(UserLogin->IsNewUser())
		{
			UserLogin->SetPassword(0, Password);
			break;
		}
		else
		{
			//see if the password is good
			if(UserLogin->ValidatePassword(Password))
				break;
			else
			{
				_Render->PrintF("Invalid password!\n", BBSRender::COLOR_BLACK, BBSRender::COLOR_RED_BRIGHT);
				_Render->SetX(0);
			}
		}
	}

	//too many failures, hang up
	if(PasswordAttempt >= 3)
		DoExit();

	//see if the account is locked
	if(UserLogin->IsLocked())
	{
		_Render->PrintF("USER ACCOUNT LOCKED\n", BBSRender::COLOR_RED, BBSRender::COLOR_BLACK);
		_Render->Render(UserDataColor);
		fflush(stdout);
		DoExit();
	}

	//save off our user object for being returned
	GlobalUser = UserLogin;

	//see if they want colors
	ColorChoice = GlobalUser->GetColorChoice();
	if(ColorChoice == BBSUser::COLOR_UNSET)
	{
		_Render->PrintF("Show Colors? [Y]es/[N]o/[A]lways/N[e]ver]: ", BBSRender::COLOR_BLACK, BBSRender::COLOR_RED);
		_Render->Render(BBSRender::COLOR_WHITE);

		memset(Password, 0, sizeof(Password));
		if(_Conn->Readline(Password, 2) == 0)
			DoExit();

		//update render based on the input
		Password[0] |= 0x20;
		if(Password[0] == 'a')
		{
			_Render->DrawColors(true);
			GlobalUser->SetColorChoice(BBSUser::COLOR_YES);
		}
		else if(Password[0] == 'y')
		{
			_Render->DrawColors(true);
		}
		else if(Password[0] == 'e')
		{
			GlobalUser->SetColorChoice(BBSUser::COLOR_NO);
		}

		//if none hit then assume no
	}
	else if(ColorChoice == BBSUser::COLOR_YES)
		_Render->DrawColors(true);
}

BBSMenu *CreateLoginMenu(BBSMENU_PlUGIN_TYPE Type, ConnectionBase *Conn, BBSRender *Render, void *Value)
{
    return new BBSLogin(Conn, Render);
}

__attribute__((constructor)) static void init()
{
    RegisterBBSMenuPlugin(CreateLoginMenu, "login");
}