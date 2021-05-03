#include <string>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>

#include "CompressionBase.h"
#include "ConnectionBase.h"
#include "EncryptionBase.h"
#include "HashBase.h"
#include "BBSRender.h"
#include "BBSMenu.h"
#include "BBSLogin.h"
#include "BBSGlobalStats.h"

#include <fcntl.h>
#include <dirent.h>

using namespace std;

char *LibraryNames;
char **Libraries;
size_t LibraryCount;

#include <sys/random.h>

__attribute__((noreturn)) void DoExit()
{
	close(0);
	close(1);
	close(2);
	_exit(0);
}

#define NAME_BUFFER_SIZE 350 //300
static void LoadModules()
{
	size_t CurCount;
	size_t NameLen;
	DIR *Dir;
	struct dirent *DirEntry;
	size_t CurNameLen;
	char *NextPos;
	char LoadName[NAME_BUFFER_SIZE];

	//yes I know this is convoluted but using a set mucks with our memory too much as does
	//using the directory_iterator for some unknown reason (works in local docker images but same
	//image remotely fails)
	//have to control our memory layout early on to help control the information leak
	//as leaking libc or ld early is not fun >:)

	//first get a count of entries
	LibraryCount = 0;
	NameLen = 0;
	Dir = opendir("./lib-cozen");

	while(1)
	{
		DirEntry = readdir(Dir);
		if(!DirEntry)
			break;

		CurNameLen = strlen(DirEntry->d_name);
		if((CurNameLen > 3) && (strcasecmp(&DirEntry->d_name[CurNameLen - 3], ".so") == 0))
		{
			NameLen += CurNameLen + 1;
			LibraryCount++;
		}
	}
	closedir(Dir);

	Libraries = (char **)malloc(NameLen + (LibraryCount * sizeof(char *)));
	memset(Libraries, 0, NameLen + (LibraryCount * sizeof(char *)));
	LibraryNames = (char *)&Libraries[LibraryCount];
	NextPos = LibraryNames;

	//find all of the modules in the current folder and load them
	Dir = opendir("./lib-cozen");
	while(1)
	{
		DirEntry = readdir(Dir);
		if(!DirEntry)
			break;

		CurNameLen = strlen(DirEntry->d_name);
		if((CurNameLen > 3) && (strcasecmp(&DirEntry->d_name[CurNameLen - 3], ".so") == 0))
		{
			//find where to put the entry
			for(CurCount = 0; CurCount < LibraryCount; CurCount++)
			{
				if(!Libraries[CurCount])
				{
					Libraries[CurCount] = NextPos;

					//move next pos past us including null
					//then copy the data including the null character
					NextPos = NextPos + strlen(DirEntry->d_name) + 1;
					memcpy(Libraries[CurCount], DirEntry->d_name, strlen(DirEntry->d_name));
					break;
				}
				else
				{
					//compare them and insert at the current location if need be to maintain ordered loading
					if(strcasecmp(DirEntry->d_name, Libraries[CurCount]) < 0)
					{
						memmove(&Libraries[CurCount+1], &Libraries[CurCount], (LibraryCount - CurCount - 1) * sizeof(char *));
						Libraries[CurCount] = NextPos;

						//move next pos past us including null
						//then copy the data including the null character
						NextPos = NextPos + strlen(DirEntry->d_name) + 1;
						memcpy(Libraries[CurCount], DirEntry->d_name, strlen(DirEntry->d_name) + 1);
						break;
					}
				}
			}
		}
	}
	closedir(Dir);

	//load all files that aren't libBBS
	for(CurCount = 0; CurCount < LibraryCount; CurCount++)
	{
		if(Libraries[CurCount] && (strstr(Libraries[CurCount], "libBBS") == 0))
		{
			//load the file if it isn't a libBBS
			snprintf(LoadName, NAME_BUFFER_SIZE, "./lib-cozen/%s", Libraries[CurCount]);
			if(dlopen(LoadName, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE) == 0)
			{
				dprintf(2, "Error loading %s: %s\n", Libraries[CurCount], dlerror());
				DoExit();
			}
		}
	}
}

static void LoadBBSModules()
{
	size_t CurCount;
	char *LoadName;

	LoadName = (char *)malloc(NAME_BUFFER_SIZE);

	//load all files that start with libBBS
	for(CurCount = 0; CurCount < LibraryCount; CurCount++)
	{
		if(Libraries[CurCount] && strstr(Libraries[CurCount], "libBBS"))
		{
			//load the file if it is a libBBS
			snprintf(LoadName, NAME_BUFFER_SIZE, "./lib-cozen/%s", Libraries[CurCount]);
			if(dlopen(LoadName, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE) == 0)
			{
				dprintf(2, "Error loading %s: %s\n", Libraries[CurCount], dlerror());
				DoExit();
			}
		}
	}

	free(LoadName);
	free(Libraries);
}

#define BUFFER_SIZE 256

int main(int argc, char **argv)
{
	ConnectionBase *Conn = 0;
	EncryptionBase *EncryptObj = 0;
	EncryptionBase *DHObj = 0;
	CompressionBase *CompressObj = 0;
	HashBase *HashObj = 0;
	uint32_t ID;
	BBSRender *Render;
	char Buffer[10];
	size_t Ret;
	EncryptionStruct *CurEncryptEntry;
	CompressionStruct *CurCompressionEntry;
	HashStruct *CurHashEntry;
	BBSMenu *WelcomeMenu;
	BBSMenu *LoginMenu;

	//load all modules
	LoadModules();

	Conn = new ConnectionBase(0, 1);

	//see if they want encryption
	Conn->WriteString("Use secure mode? [N] ");
	Ret = Conn->Readline(Buffer, BUFFER_SIZE - 1);
	Buffer[Ret] = 0;

	//see if the first character is Y or y, if so then setup the encrypted connection
	if((Buffer[0] == 'Y') || (Buffer[0] == 'y'))
	{
		//report a list of encryption objects
		Conn->WriteString("Please select encryption module:\n");
		Ret = 1;
		CurEncryptEntry = EncryptionEntries;
		while(CurEncryptEntry)
		{
			if(CurEncryptEntry->ID != MAGIC_VAL('D','H','M','N'))
			{
				Conn->WriteFString("[ %d ] %s\n", Ret, CurEncryptEntry->Name);
				CurEncryptEntry->CustomID = Ret;
				Ret++;
			}
			CurEncryptEntry = CurEncryptEntry->Next;
		}

		Conn->WriteString("[ 0 ] Skip encryption\n");
		Conn->WriteString(">> ");

		Ret = Conn->Readline(Buffer, BUFFER_SIZE - 1);
		Buffer[Ret] = 0;

		//attempt to get the ID
		ID = atoi(Buffer);
		if(ID != 0)
		{
			//go find it
			CurEncryptEntry = EncryptionEntries;
			while(CurEncryptEntry && (CurEncryptEntry->CustomID != ID))
				CurEncryptEntry = CurEncryptEntry->Next;

			if(!CurEncryptEntry)
			{
				Conn->WriteString("Error locating encryption module - goodbye\n");
				fflush(stdout);
				exit(0);
			}

			//setup diffie hellman
			DHObj = GetEncryption(MAGIC_VAL('D','H','M','N'))(Conn);
			__uint128_t P[2], G[2];

			/*
				P = 0xe18c3648785c484b9b9d60f9ffa0e46cd1c5d585b1b6fc90794147a203f65b2b
				G = 0x3d0daebfc6a112e58c1885b6d824420b73c9b689c1029ad37748c58e56febfa7
			*/

			P[1] = 0xe18c3648785c484b;
			P[1] = (P[1] << 64) | 0x9b9d60f9ffa0e46c;
			P[0] = 0xd1c5d585b1b6fc90;
			P[0] = (P[0] << 64) | 0x794147a203f65b2b;
			G[1] = 0x3d0daebfc6a112e5;
			G[1] = (G[1] << 64) | 0x8c1885b6d824420b;
			G[0] = 0x73c9b689c1029ad3;
			G[0] = (G[0] << 64) | 0x7748c58e56febfa7;

			//setup P and G
			DHObj->SetConfig(0x100, &P);
			DHObj->SetConfig(0x101, &G);

			//send the challenge
			DHObj->Write(0, 0);

			//retrieve the final key and IV
			uint8_t TempData[sizeof(__uint128_t) * 2];
			DHObj->ReadAll(TempData, sizeof(TempData));
			delete DHObj;

			//init the encryption object
			EncryptObj = CurEncryptEntry->Init(Conn);
			if(!EncryptObj)
				DoExit();

			//we got the encryption object, setup for CBC and 128 bit
			EncryptObj->SetConfig(EncryptionBase::CONFIG_MODE, EncryptionBase::ENCRYPTION_MODE_CBC);
			EncryptObj->SetConfig(EncryptionBase::CONFIG_BITS, 128);
			EncryptObj->SetConfig(EncryptionBase::CONFIG_KEY, &TempData[0]);
			EncryptObj->SetConfig(EncryptionBase::CONFIG_IV, &TempData[sizeof(__uint128_t)]);

			//tell our writer what the right thing is for communication
			Conn = EncryptObj;
			Conn->WriteString("Encryption initialized\n");
		}
	}

	//see if they want compression
	Conn->WriteString("Use compression? [N] ");
	Ret = Conn->Readline(Buffer, BUFFER_SIZE - 1);
	Buffer[Ret] = 0;

	//see if the first character is Y or y, if so then setup the encrypted connection
	if((Buffer[0] == 'Y') || (Buffer[0] == 'y'))
	{
		//report a list of encryption objects
		Conn->WriteString("Please select compression module:\n");
		Ret = 1;
		CurCompressionEntry = CompressionEntries;
		while(CurCompressionEntry)
		{
			Conn->WriteFString("[ %d ] %s\n", Ret, CurCompressionEntry->Name);
			CurCompressionEntry->CustomID = Ret;
			Ret++;
			CurCompressionEntry = CurCompressionEntry->Next;
		}

		Conn->WriteString("[ 0 ] Skip compression\n");
		Conn->WriteString(">> ");

		Ret = Conn->Readline(Buffer, BUFFER_SIZE - 1);
		Buffer[Ret] = 0;

		//attempt to get the ID
		ID = atoi(Buffer);
		if(ID != 0)
		{
			//go find it
			CurCompressionEntry = CompressionEntries;
			while(CurCompressionEntry && (CurCompressionEntry->CustomID != ID))
				CurCompressionEntry = CurCompressionEntry->Next;

			if(!CurCompressionEntry)
			{
				Conn->WriteString("Error locating compression module - goodbye\n");
				fflush(stdout);
				exit(0);
			}

			//report a list of hash objects
			Conn->WriteString("Please select hash module for validation:\n");
			Ret = 1;
			CurHashEntry = HashEntries;
			while(CurHashEntry)
			{
				Conn->WriteFString("[ %d ] %s\n", Ret, CurHashEntry->Name);
				CurHashEntry->CustomID = Ret;
				Ret++;
				CurHashEntry = CurHashEntry->Next;
			}

			Conn->WriteString("[ 0 ] Skip hash validation\n");
			Conn->WriteString(">> ");

			Ret = Conn->Readline(Buffer, BUFFER_SIZE - 1);
			Buffer[Ret] = 0;

			//attempt to get the ID
			ID = atoi(Buffer);
			if(ID != 0)
			{
				//go find it
				CurHashEntry = HashEntries;
				while(CurHashEntry && (CurHashEntry->CustomID != ID))
					CurHashEntry = CurHashEntry->Next;

				if(!CurHashEntry)
				{
					Conn->WriteString("Error locating hash module - goodbye\n");
					fflush(stdout);
					exit(0);
				}

				//init the compression object
				HashObj = CurHashEntry->Init();
				if(!HashObj)
					DoExit();
			}
			else
			{
				//setup the base hash object with 0 for the hash length
				HashObj = new HashBase(0);
			}

			//init the compression object
			CompressObj = CurCompressionEntry->Init(Conn, HashObj);
			if(!CompressObj)
				DoExit();

			//tell our writer what the right thing is for communication
			Conn = CompressObj;
			Conn->WriteString("Compression initialized\n");
		}
	}

	//setup global stats
	GlobalStats = new BBSGlobalStats();

	//setup our renderer and main menu
	Render = new BBSRender(Conn);
	MainMenu = new BBSMenu(Conn, Render);

	//load all bbs modules so they register with the main menu
	LoadBBSModules();

	//kick off the login screen
	BBSMenuPluginInit LoginInit = GetBBSMenuPlugin("login");
	if(LoginInit)
	{
		LoginMenu = LoginInit(BBSMENU_PlUGIN_TYPE::TYPE_NORMAL, Conn, Render, 0);
		LoginMenu->HandleMenu();
		delete LoginMenu;
	}

	if(GlobalUser->GetShowWelcome())
	{
		//kick off the welcome screen
		BBSMenuPluginInit WelcomeInit = GetBBSMenuPlugin("welcome");
		if(WelcomeInit)
		{
			WelcomeMenu = WelcomeInit(BBSMENU_PlUGIN_TYPE::TYPE_NORMAL, Conn, Render, 0);
			WelcomeMenu->HandleMenu();
			delete WelcomeMenu;
		}
	}

	//kick off the main menu
	MainMenu->RefreshGroup();
	MainMenu->HandleMenu();

	//all done, exit
	DoExit();
	return 0;
}
