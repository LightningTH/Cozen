#ifndef NEKISAHLOTH_LIB_BBS_MESSAGES
#define NEKISAHLOTH_LIB_BBS_MESSAGES

#include <string>
#include <unistd.h>
#include <stdint.h>
#include <BBSMenu.h>
#include <time.h>
#include "BBSUser.h"

using namespace std;

class BBSMessages : public BBSMenu
{
	public:

		BBSMessages();
		BBSMessages(ConnectionBase *Conn, BBSRender *Render, bool ShowUnread);
		size_t DoAction(size_t ID);
		const uint8_t RequiredPrivilege();			//required level to see this menu, 255 = SYSOP
		bool NewSinceLastRead();
		size_t GetValue(const char *Name, void *Data, size_t DataLen);

	private:
		void DisplayErrorMessage(const char *Msg);
		int LockMessageFile();
		int LockMessageFile(size_t ID);
		void UnlockMessageFile(int fd);
		void UnlockMessageFile(int fd, size_t ID);
		void DisplayNewMessages();
		void DisplayAllMessages();
		void MarkAllAsRead();
		void DeleteAllMessages();
		void SendNewMessage();
		void GenerateMessageList(int fd, bool OnlyUnread);
		void DisplayMoreMessages();
		void DisplayMessage(size_t ID);
		void DeleteMessage(size_t ID);
		void SendReplyMessage(BBSUser *ToUser, std::string *Subject);
		void GetAndSendMessage(BBSUser *ToUser, std::string *NewSubject);
		std::string GetMessageFile();
		std::string GetMessageFile(size_t ID);

		//maximum number of messages to display
		static const int MESSAGE_LIST_COUNT = 16;

		typedef struct MessageFileStruct
		{
			size_t FromID;
			time_t Timestamp;
			uint8_t Read : 1;
			uint8_t ReservedBits : 7;
			uint8_t SubjectLen;
			uint16_t MessageLen;
		} MessageFileStruct;

		typedef struct MessageStruct
		{
			size_t Offset;
			MessageFileStruct MessageInfo;
			std::string *Subject;
			char *Message;
			BBSUser *FromUser;
			std::string *DisplayName;
		} MessageStruct;

		BBSMenu::MenuEntryStruct *_MessageReadMenu;
		bool _DisplayingUnread;
		size_t _LastFileOffset;
};

#endif
