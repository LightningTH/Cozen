#include "BBSMessages.h"
#include "BBSUser.h"
#include "BBSUserSearch.h"

#include <fcntl.h>
#include <string.h>
#include <vector>

using namespace std;

BBSMenu::MenuEntryStruct UserMessageMenu[] = {
    {.ID = 0, .Name = "Read Messages", .Help = "Read all messages"},
    {.ID = 1, .Name = "Read New Messages", .Help = "Read only new messages"},
    {.ID = 2, .Name = "Mark Messages as Read", .Help = "Mark all unread messages as read"},
    {.ID = 3, .Name = "Delete All Messages", .Help = "Delete all messages"},
    {.ID = 4, .Name = "Send New Message", .Help = "Send a new message to a user"},
    {.ID = 5, .Name = "Back", .Help = "Previous menu"},
    {.Name = 0}
};

#define MESSAGE_FILE_PATH "./data/messages/"

BBSMessages::BBSMessages() :
	BBSMenu(0, 0)
{
	//simple initialization to know if unread messages exist
}

BBSMessages::BBSMessages(ConnectionBase *Conn, BBSRender *Render, bool ShowUnread) :
	BBSMenu(Conn, Render)
{
	LargeHeader = "MESSAGES MENU";
	Header = "MESSAGES";

	_Menu = UserMessageMenu;
	_SelectionType = SELECTION_ALPHA;
	_DisplayingUnread = false;

    HeaderColor = BBSRender::COLOR_WHITE;
    BoxLineColor = BBSRender::COLOR_BLUE;
    OptionColor = BBSRender::COLOR_GREEN;
	UserDataColor = BBSRender::COLOR_RED;

	//add ourselves as the menu handler
	AddMenu(this);
	_MessageReadMenu = 0;

	if(ShowUnread)
		DisplayNewMessages();
}

bool BBSMessages::NewSinceLastRead()
{
	int fd;
	size_t UnreadCount;

	//return if any unread messages have been added since the last
	//read occurred of unread
	fd = open(GetMessageFile().c_str(), O_RDONLY);
	if(fd < 0)
		return false;

	if(read(fd, &UnreadCount, sizeof(UnreadCount)) != sizeof(UnreadCount))
	{
		close(fd);
		return false;
	}

	//indicate if the top bit is set
	return (UnreadCount & ((size_t)1 << (UINT64_WIDTH - 1))) != 0;
}

std::string BBSMessages::GetMessageFile()
{
	return GetMessageFile(GlobalUser->GetID());
}

std::string BBSMessages::GetMessageFile(size_t ID)
{
	char Buffer[32];
	std::string MessageFilename;

	MessageFilename = MESSAGE_FILE_PATH;
	snprintf(Buffer, sizeof(Buffer), "%ld", ID);
	MessageFilename.append(Buffer);
	return MessageFilename;
}

int BBSMessages::LockMessageFile()
{
	return LockMessageFile(GlobalUser->GetID());
}

int BBSMessages::LockMessageFile(size_t ID)
{
    timespec delay;
	std::string LockFilename;
	int fd_lock;

	LockFilename = GetMessageFile(ID) + ".lock";

    while(1)
    {
        fd_lock = open(LockFilename.c_str(), O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);

        //if we got lock then exit otherwise wait a short moment
        if(fd_lock >= 0)
            break;

        delay.tv_sec = 0;
        delay.tv_nsec = 1000;
        nanosleep(&delay, 0);
    }

	return fd_lock;
}

void BBSMessages::UnlockMessageFile(int fd_lock)
{
	UnlockMessageFile(fd_lock, GlobalUser->GetID());
}

void BBSMessages::UnlockMessageFile(int fd_lock, size_t ID)
{
	std::string LockFilename;

	if(fd_lock < 0)
		return;

	LockFilename = GetMessageFile(ID) + ".lock";

	unlink(LockFilename.c_str());
	close(fd_lock);
}

void BBSMessages::DeleteAllMessages()
{
	//lock the message file then delete it
	int lock_fd = LockMessageFile();
	unlink(GetMessageFile().c_str());
	UnlockMessageFile(lock_fd);
}

void BBSMessages::GenerateMessageList(int fd, bool OnlyUnread)
{
	//start reading from where the fd currently points and generate
	//a list of messages to display

    size_t Count;
	MessageStruct *CurMessage;
    tm CurTime;
	char Buffer[100];

    //generate the menu of found users
    if(!_MessageReadMenu)
    {
        _MessageReadMenu = (BBSMenu::MenuEntryStruct *)calloc(MESSAGE_LIST_COUNT + 1, sizeof(BBSMenu::MenuEntryStruct));
        for(Count = 0; Count < MESSAGE_LIST_COUNT; Count++)
        {
            _MessageReadMenu[Count].ID = Count + 1;
            _MessageReadMenu[Count].Color = BBSRender::COLOR_WHITE;
        }
    }

    //erase whatever existed prior
    for(Count = 0; Count < MESSAGE_LIST_COUNT; Count++)
	{
		if(_MessageReadMenu[Count].Name)
		{
			CurMessage = (MessageStruct *)_MessageReadMenu[Count].Data;
			delete CurMessage->Subject;
			free(CurMessage->Message);
			delete CurMessage->DisplayName;
			free(CurMessage);
			_MessageReadMenu[Count].Name = 0;
			_MessageReadMenu[Count].Data = 0;
			_MessageReadMenu[Count].Color = BBSRender::COLOR_WHITE;
		}
	}

    //keep adding entries to our list
    for(Count = 0; Count < MESSAGE_LIST_COUNT; Count++)
    {
		//get next message
		CurMessage = (MessageStruct *)malloc(sizeof(MessageStruct));
		while(1)
		{
			CurMessage->Offset = lseek(fd, 0, SEEK_CUR);
			if(read(fd, &CurMessage->MessageInfo, sizeof(CurMessage->MessageInfo)) != sizeof(CurMessage->MessageInfo))
			{
				free(CurMessage);
				CurMessage = 0;
				break;
			}

			//if showing all messages or message isn't read then exit
			if(!OnlyUnread || !CurMessage->MessageInfo.Read)
				break;

			//loop and try again after stepping past our subject and message
			lseek(fd, CurMessage->MessageInfo.SubjectLen + CurMessage->MessageInfo.MessageLen, SEEK_CUR);
		}
	
		//if no current message then we hit the end of the file, exit our loop
		if(!CurMessage)
			break;

		//got a message, get the subject and message
		CurMessage->Subject = new std::string();
		CurMessage->Subject->resize(CurMessage->MessageInfo.SubjectLen + 1);
		CurMessage->Message = (char *)malloc(CurMessage->MessageInfo.MessageLen + 2);

		//read the subject and message
		//c++17 says we can cast away from const
		if(read(fd, (char *)CurMessage->Subject->data(), CurMessage->MessageInfo.SubjectLen) != CurMessage->MessageInfo.SubjectLen)
		{
			delete CurMessage->Subject;
			free(CurMessage->Message);
			free(CurMessage);
			break;
		}

		//read message
		if(read(fd, CurMessage->Message, CurMessage->MessageInfo.MessageLen) != CurMessage->MessageInfo.MessageLen)
		{
			delete CurMessage->Subject;
			free(CurMessage->Message);
			free(CurMessage);
			break;
		}

		//double null terminate
		CurMessage->Message[CurMessage->MessageInfo.MessageLen] = 0;
		CurMessage->Message[CurMessage->MessageInfo.MessageLen + 1] = 0;

		//find the user
		CurMessage->FromUser = new BBSUser();
		if(!CurMessage->FromUser->FindExact(CurMessage->MessageInfo.FromID))
		{
			delete CurMessage->Subject;
			free(CurMessage->Message);
			free(CurMessage);
			break;
		}

		//setup the menu name to display
		CurMessage->DisplayName = new string();
    	gmtime_r(&CurMessage->MessageInfo.Timestamp, &CurTime);
		snprintf(Buffer, sizeof(Buffer), "%04d/%02d/%02d %02d:%02d", CurTime.tm_year + 1900, CurTime.tm_mon + 1, CurTime.tm_mday, CurTime.tm_hour, CurTime.tm_min);
		CurMessage->DisplayName->append(Buffer);
		CurMessage->DisplayName->append("  ");
		CurMessage->DisplayName->append(CurMessage->FromUser->GetName());

		//if the name is too long then shorten it and append ... to the end
		if(CurMessage->DisplayName->length() > (BBSRender::RENDER_WIDTH / 2))
		{
			CurMessage->DisplayName->resize((BBSRender::RENDER_WIDTH / 2) - 3);
			CurMessage->DisplayName->append("...");
		}

		CurMessage->DisplayName->append("  ");

		CurMessage->DisplayName->append(*CurMessage->Subject);

		//if the final string is too long then shorten it and append ... to the end
		if(CurMessage->DisplayName->length() > BBSRender::RENDER_WIDTH - 10)
		{
			CurMessage->DisplayName->resize(BBSRender::RENDER_WIDTH - 13);
			CurMessage->DisplayName->append("...");
		}

		//if unread then color it
		if(!CurMessage->MessageInfo.Read)
			_MessageReadMenu[Count].Color = BBSRender::COLOR_BLUE;
		else
			_MessageReadMenu[Count].Color = BBSRender::COLOR_WHITE;

		//now assign the name for this menu entry along with the data
		_MessageReadMenu[Count].Data = CurMessage;
		_MessageReadMenu[Count].Name = CurMessage->DisplayName->c_str();
    }

	//if no count then exit early as we have no messages to display
	if(!Count)
	{
		_SelectionType = SELECTION_ALPHA;
		_Menu = UserMessageMenu;
		return;
	}

    //update the menu entry
	_SelectionType = SELECTION_NUMERIC;
    _Menu = _MessageReadMenu;

    //set the prompt up
	_LastFileOffset = lseek(fd, 0, SEEK_CUR);
	if((size_t)lseek(fd, 0, SEEK_END) <= _LastFileOffset)	//if end of file then reset last file offset
		_LastFileOffset = 0;
    else if(OnlyUnread)
	{
		//we only want unread, see if we can find any more

		//fix our lseek to the end for seeing if we hit the end
		lseek(fd, _LastFileOffset, SEEK_SET);

		//if only displaying unread then we need to see if there are any more unread to display
		CurMessage = (MessageStruct *)malloc(sizeof(MessageStruct));
		while(1)
		{
			CurMessage->Offset = lseek(fd, 0, SEEK_CUR);
			if(read(fd, &CurMessage->MessageInfo, sizeof(CurMessage->MessageInfo)) != sizeof(CurMessage->MessageInfo))
			{
				_LastFileOffset = 0;
				free(CurMessage);
				break;
			}

			//if message isn't read then exit
			if(!CurMessage->MessageInfo.Read)
			{
				_LastFileOffset = CurMessage->Offset;
				free(CurMessage);
				break;
			}

			//loop and try again after stepping past our subject and message
			lseek(fd, CurMessage->MessageInfo.SubjectLen + CurMessage->MessageInfo.MessageLen, SEEK_CUR);
		}
	}

	//if lastfile offset is set then indicate we have more otherwise no more to display
	if(_LastFileOffset)
        Prompt = "%3~[%2~B%3~]%1~ack  %3~[%2~M%3~]%1~ore  Selection>> ";
	else
		Prompt = "%3~[%2~B%3~]%1~ack  Selection>> ";


	//remember what we were displaying
	_DisplayingUnread = OnlyUnread;
}

void BBSMessages::DisplayErrorMessage(const char *Msg)
{
	char Buffer[2];

	//display the message
	_Render->Clear();
	_Render->DrawHeader(BBSRender::COLOR_BLACK, HeaderColor, LargeHeader);
	_Render->SetX(0);
	_Render->OffsetY(1);
	_Render->PrintF("%s. Press enter to return", BBSRender::COLOR_BLACK, TextColor, Msg);
	_Render->Render(BBSRender::COLOR_BLACK, true);

	//wait on a keypress
	_Conn->Readline(Buffer, 1);
}

void BBSMessages::DisplayNewMessages()
{
	int fd;
	size_t UnreadCount;

	//open up the message file in read mode and get a list of messages to display
	fd = open(GetMessageFile().c_str(), O_RDONLY);
	if(fd < 0)
	{
		DisplayErrorMessage("No unread messages");
		return;
	}

	//read the beginning of the file as it tells us how many unread messages there are
	if(read(fd, &UnreadCount, sizeof(UnreadCount)) != sizeof(UnreadCount))
	{
		close(fd);
		DisplayErrorMessage("No unread messages");
		return;
	}

	//if the value is 0 then return
	if(!UnreadCount)
	{
		close(fd);
		DisplayErrorMessage("No unread messages");
		return;
	}

	//everything is good, show our unread messages
	GenerateMessageList(fd, true);
	close(fd);
}

void BBSMessages::DisplayAllMessages()
{
	int fd;
	size_t Temp;

	//open up the message file in read mode and get a list of messages to display
	fd = open(GetMessageFile().c_str(), O_RDONLY);
	if(fd < 0)
	{
		DisplayErrorMessage("No messages");
		return;
	}

	//read the beginning of the file as it is a marker for unread messages
	if(read(fd, &Temp, sizeof(Temp)) != sizeof(Temp))
	{
		close(fd);
		DisplayErrorMessage("No messages");
		return;
	}

	//everything is good, start processing
	GenerateMessageList(fd, false);
	close(fd);
}

void BBSMessages::DisplayMoreMessages()
{
	int fd;

	//if no offset then no more messages to display
	if(!_LastFileOffset)
		return;

	//open up the message file in read mode and get a list of messages to display
	fd = open(GetMessageFile().c_str(), O_RDONLY);
	if(fd < 0)
	{
		_LastFileOffset = 0;
		DisplayErrorMessage("No messages");
		return;
	}

	//skip forward to our location
	if((size_t)lseek(fd, _LastFileOffset, SEEK_SET) != _LastFileOffset)
	{
		_LastFileOffset = 0;
		DisplayErrorMessage("No messages");
		return;
	}

	//everything is good, start processing
	GenerateMessageList(fd, _DisplayingUnread);
	close(fd);
}

void BBSMessages::MarkAllAsRead()
{
	int fd;
	int lock_fd;
	size_t UnreadCount;
	size_t CurPos;
	MessageFileStruct CurMessage;

	//lock our file
	lock_fd = LockMessageFile();

	//open up the message file
	fd = open(GetMessageFile().c_str(), O_RDWR);
	if(fd < 0)
	{
		UnlockMessageFile(lock_fd);
		return;
	}

	//read the beginning of the file as it tells us how many unread messages there are
	if(read(fd, &UnreadCount, sizeof(UnreadCount)) != sizeof(UnreadCount))
	{
		close(fd);
		UnlockMessageFile(lock_fd);
		return;
	}

	//if the value is 0 then return
	if(!UnreadCount)
	{
		close(fd);
		UnlockMessageFile(lock_fd);
		return;
	}

	//go to the beginning and write a 0
	UnreadCount = 0;
	lseek(fd, 0, SEEK_SET);
	write(fd, &UnreadCount, sizeof(UnreadCount));

	//go through all messages and set the read flag
	while(1)
	{
		//get next message
		CurPos = lseek(fd, 0, SEEK_CUR);
		if(read(fd, &CurMessage, sizeof(CurMessage)) != sizeof(CurMessage))
			break;

		//set read bit if need be
		if(!CurMessage.Read)
		{
			CurMessage.Read = 1;

			//write back out
			lseek(fd, CurPos, SEEK_SET);
			write(fd, &CurMessage, sizeof(CurMessage));
		}

		//seek past subject and message
		lseek(fd, (size_t)CurMessage.MessageLen + CurMessage.SubjectLen, SEEK_CUR);
	}

	//all done
	close(fd);
	UnlockMessageFile(lock_fd);
}

void BBSMessages::SendNewMessage()
{
	BBSUserSearch *Search;
	BBSUser *ToUser;
	std::string Subject;
	char Buffer[100];

	//find a new user to send a message to
	Search = new BBSUserSearch(_Conn, _Render);
	Search->HandleMenu();
	ToUser = Search->GetUser();
	delete Search;

	//if no user then fail
	if(!ToUser)
		return;

	//we have a user, get a subject
	_Render->Clear();
	_Render->DrawHeader(BBSRender::COLOR_BLACK, HeaderColor, "SEND MESSAGE");

	_Render->OffsetY(1);
	_Render->SetX(0);

	//indicate who it is from
	_Render->PrintF("To: %2`s\n", BBSRender::COLOR_BLACK, BBSRender::COLOR_GREY, TextColor, ToUser->GetName());

	//get the subject
	_Render->PrintF("Subject: ", BBSRender::COLOR_BLACK, BBSRender::COLOR_GREY);
	_Render->Render(UserDataColor);
	if(_Conn->Readline(Buffer, BBSRender::RENDER_WIDTH - _Render->GetX()) == 0)
	{
		//no subject, fail
		delete ToUser;
		return;
	}

	//now get the rest of the message and send it
	Subject = Buffer;
	GetAndSendMessage(ToUser, &Subject);

	//cleanup
	delete ToUser;
}

void BBSMessages::DisplayMessage(size_t ID)
{
	MessageStruct *CurMessage;
	char *CurLine;
	char Buffer[3];
	int NullsSeen = 0;
	bool Done;
	int fd;
	int lock_fd;
	size_t UnreadCount;

	//display an actual message
	Done = false;
	while(!Done)
	{
		//first, see if the message is valid
		if(!_MessageReadMenu || !_MessageReadMenu[ID].Data)
			return;

		//get the message to render
		CurMessage = (MessageStruct *)_MessageReadMenu[ID].Data;

		//if the message is unread then mark it as read in the file
		if(!CurMessage->MessageInfo.Read)
		{
			CurMessage->MessageInfo.Read = 1;
			lock_fd = LockMessageFile();
			fd = open(GetMessageFile().c_str(), O_RDWR);
			if(fd >= 0)
			{
				//adjust unread count by 1
				read(fd, &UnreadCount, sizeof(UnreadCount));
				UnreadCount = UnreadCount & ~((size_t)1 << (UINT64_WIDTH - 1));
				UnreadCount--;
				if(UnreadCount == UINT64_MAX)
					UnreadCount = 0;
				lseek(fd, 0, SEEK_SET);
				write(fd, &UnreadCount, sizeof(UnreadCount));

				//rewrite our message header
				lseek(fd, CurMessage->Offset, SEEK_SET);
				write(fd, &CurMessage->MessageInfo, sizeof(CurMessage->MessageInfo));
				close(fd);
			}
			UnlockMessageFile(lock_fd);

			//update the display color to indicate read
			_MessageReadMenu[ID].Color = BBSRender::COLOR_WHITE;
		}

		_Render->Clear();
		_Render->DrawHeader(BBSRender::COLOR_BLACK, HeaderColor, "READ MESSAGE");

		_Render->OffsetY(1);
		_Render->SetX(0);

		//indicate who it is from
		_Render->PrintF("From: %2`s\n", BBSRender::COLOR_BLACK, BBSRender::COLOR_GREY, TextColor, CurMessage->FromUser->GetName());

		//display the subject
		_Render->PrintF("Subject: %2`s\n", BBSRender::COLOR_BLACK, BBSRender::COLOR_GREY, TextColor, CurMessage->Subject->c_str());

		_Render->OffsetY(1);

		//now display the message, messages end with a double null character
		CurLine = CurMessage->Message;
		NullsSeen = 0;
		while(NullsSeen < 2)
		{
			if(!*CurLine)
				NullsSeen++;
			else
				NullsSeen = 0;

			//render it and advance
			_Render->PrintF("%s\n", BBSRender::COLOR_BLACK, TextColor, CurLine);
			CurLine = CurLine + strlen(CurLine) + 1;

			//if we have too many lines then output a "more" and wait for a keypress
			if(_Render->GetY() == (BBSRender::RENDER_HEIGHT - 2))
			{
				_Render->OffsetY(1);
				_Render->PrintF("--more--", BBSRender::COLOR_BLACK, BBSRender::COLOR_YELLOW);
				_Render->Render(UserDataColor);
				_Conn->ReadAll(Buffer, 1);

				//reset our screen and continue
				_Render->Clear();
			}
		}

		//render and wait for a response of what to do
		_Render->OffsetY(1);
		_Render->PrintF("%3~[%2~B%3~]%1~ack  %3~[%2~R%3~]%1~eply  %3~[%2~D%3~]%1~elete  ", BBSRender::COLOR_BLACK, TextColor, OptionColor, BoxLineColor);
		if(((ID+1) < MESSAGE_LIST_COUNT) && _MessageReadMenu[ID+1].Data)
			_Render->PrintF("%3~[%2~N%3~]%1~ext Message>>", BBSRender::COLOR_BLACK, TextColor, OptionColor, BoxLineColor);
		else
			_Render->PrintF(">>", BBSRender::COLOR_BLACK, TextColor);
		_Render->Render(UserDataColor);

		while(1)
		{
			//get response
			_Conn->Readline(Buffer, 2);

			//make lower case
			Buffer[0] |= 0x20;
			if(Buffer[0] == 'd')
			{
				DeleteMessage(ID);
				Done = true;
				break;
			}
			else if(Buffer[0] == 'r')
			{
				SendReplyMessage(CurMessage->FromUser, CurMessage->Subject);
				Done = true;
				break;
			}
			else if((Buffer[0] == 'n') && ((ID+1) < MESSAGE_LIST_COUNT) && _MessageReadMenu[ID+1].Data)
			{
				ID++; //next message
				break;
			}
			else if(Buffer[0] == 'b')
			{
				Done = true;
				break;	//back
			}
		}
	};
}

void BBSMessages::SendReplyMessage(BBSUser *ToUser, std::string *Subject)
{
	//we need to craft a new subject then send a message normally
	std::string NewSubject;

	if(strncasecmp(Subject->c_str(), "re:", 3) != 0)
	{
		NewSubject = "Re:";
		NewSubject.append(*Subject);
	}
	else
		NewSubject.append(*Subject);

	//make sure our new subject isn't too long
	if(NewSubject.length() > BBSRender::RENDER_WIDTH - 9)
		NewSubject.resize(BBSRender::RENDER_WIDTH - 9);

	GetAndSendMessage(ToUser, &NewSubject);
}

void BBSMessages::GetAndSendMessage(BBSUser *ToUser, std::string *Subject)
{
	//get an actual message to send to someone
	vector<char *> Message;
	char *CurLine;
	bool SawBlank;
	MessageStruct NewMessage;
	size_t MessageLen;
	size_t LineLen;
	size_t Count;
	int fd, lock_fd;
	size_t UnreadCount;

	_Render->Clear();
	_Render->DrawHeader(BBSRender::COLOR_BLACK, HeaderColor, "SEND MESSAGE");

	_Render->OffsetY(1);
	_Render->SetX(0);

	//indicate who it is from
	_Render->PrintF("To: %2`s\n", BBSRender::COLOR_BLACK, BBSRender::COLOR_GREY, TextColor, ToUser->GetName());

	//display the subject
	_Render->PrintF("Subject: %2`s\n", BBSRender::COLOR_BLACK, BBSRender::COLOR_GREY, TextColor, Subject->c_str());
	_Render->PrintF("--Type in message to send, finish message with 2 blank lines at the end--\n", BBSRender::COLOR_BLACK, BBSRender::COLOR_GREEN);

	_Render->OffsetY(1);
	_Render->Render(BBSRender::COLOR_WHITE);

	SawBlank = false;
	MessageLen = 0;
	while(1)
	{
		CurLine = (char *)calloc(BBSRender::RENDER_WIDTH + 1, 1);
		LineLen = _Conn->Readline(CurLine, BBSRender::RENDER_WIDTH);

		//if a blank line and already saw a blank line then exit
		if(LineLen == 0)
		{
			if(SawBlank)
			{
				//we saw a blank line already so all done
				free(CurLine);
				break;
			}
			else
				SawBlank = true;
		}
		else
			SawBlank = false;

		//add this line to our vector and increment our length
		if((MessageLen + LineLen + 1) >= UINT16_MAX)
		{
			//message is too long, cut it off
			LineLen = UINT16_MAX - MessageLen - 2;
			CurLine[LineLen] = 0;
		}

		MessageLen += LineLen + 1;
		Message.push_back(CurLine);

		//if we are within a few bytes of our max then exit
		if(MessageLen >= (UINT16_MAX - 2))
			break;
	};

	//generate a new entry
	memset(&NewMessage, 0, sizeof(NewMessage));
	NewMessage.MessageInfo.FromID = GlobalUser->GetID();
	NewMessage.MessageInfo.Timestamp = time(0);
	NewMessage.MessageInfo.SubjectLen = Subject->length();
	NewMessage.MessageInfo.MessageLen = MessageLen;
	NewMessage.Subject = Subject;

	//allocate memory for the new message
	NewMessage.Message = (char *)malloc(MessageLen);

	//write each of our strings to the message buffer
	CurLine = NewMessage.Message;
	for(Count = 0; Count < Message.size(); Count++)
	{
		//+1 for null byte
		memcpy(CurLine, Message[Count], strlen(Message[Count]) + 1);
		CurLine += strlen(Message[Count]) + 1;
	}

	//write the whole thing out
	lock_fd = LockMessageFile(ToUser->GetID());
	fd = open(GetMessageFile(ToUser->GetID()).c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if(fd < 0)
	{
		//clean up memory
		for(Count = 0; Count < Message.size(); Count++)
			free(Message[Count]);
		free(NewMessage.Message);

		//unlock the remote user file
		UnlockMessageFile(lock_fd, ToUser->GetID());

		//alert to the error in opening the file
		DisplayErrorMessage("Unable to save message");
		return;
	}

	//read the beginning marker that tells how many unread messages exist
	if(read(fd, &UnreadCount, sizeof(UnreadCount)) != sizeof(UnreadCount))
		UnreadCount = 0;

	lseek(fd, 0, SEEK_SET);

	//increment the unread count and set the bit indicating new messages exist
	UnreadCount++;
	UnreadCount |= ((size_t)1 << (UINT64_WIDTH-1));

	//write the unread count back out
	write(fd, &UnreadCount, sizeof(UnreadCount));

	//move to the end of the file
	lseek(fd, 0, SEEK_END);

	//write out our struct, subject, and message
	write(fd, &NewMessage.MessageInfo, sizeof(NewMessage.MessageInfo));
	write(fd, NewMessage.Subject->c_str(), NewMessage.MessageInfo.SubjectLen);
	write(fd, NewMessage.Message, NewMessage.MessageInfo.MessageLen);

	//close the file
	close(fd);

	//unlock the remote user file
	UnlockMessageFile(lock_fd, ToUser->GetID());

	//update global
	GlobalStats->IncrementTotalMessages();

	//clean up memory
	for(Count = 0; Count < Message.size(); Count++)
		free(Message[Count]);

	free(NewMessage.Message);
}

void BBSMessages::DeleteMessage(size_t ID)
{
	//delete a message from the file
	MessageStruct *CurMessage;
	int fd;
	int lock_fd;
	char *Buffer;
	size_t MoveAmount;
	ssize_t ReadSize;
	size_t StartPos;

	//first make sure the ID is valid
	if((ID >= MESSAGE_LIST_COUNT) || !_MessageReadMenu[ID].Data)
		return;

	CurMessage = (MessageStruct *)_MessageReadMenu[ID].Data;

	//lock our file
	lock_fd = LockMessageFile();

	fd = open(GetMessageFile().c_str(), O_RDWR);
	if(fd < 0)
	{
		UnlockMessageFile(lock_fd);
		DisplayErrorMessage("Error deleting message");
		return;
	}

	//see how much we have to move everything by
	MoveAmount = CurMessage->MessageInfo.MessageLen + CurMessage->MessageInfo.SubjectLen + sizeof(CurMessage->MessageInfo);

	//allocate a large buffer to shuffle the file data with
	Buffer = (char *)malloc(64 * 1024);

	//move to the next record that we need to move
	lseek(fd, CurMessage->Offset + MoveAmount, SEEK_SET);
	while(1)
	{
		//move things around
		StartPos = lseek(fd, 0, SEEK_CUR);
		ReadSize = read(fd, Buffer, 64*1024);
		if(ReadSize <= 0)
			break;

		//seek backwards from where we read and write the buffer
		lseek(fd, StartPos - MoveAmount, SEEK_SET);
		write(fd, Buffer, ReadSize);

		//reposition to be after the block we read initially
		lseek(fd, StartPos + ReadSize, SEEK_SET);
	}

	//free memory
	free(Buffer);

	//truncate the file to remove the extra amount
	StartPos = lseek(fd, 0, SEEK_END);
	StartPos -= MoveAmount;
	ftruncate(fd, StartPos);

	//close the file
	close(fd);
	UnlockMessageFile(lock_fd);

	//if our last file offset was larger than our deleted file
	//then move backwards otherwise it was before us so we are fine
	if(_LastFileOffset > CurMessage->Offset)
		_LastFileOffset -= MoveAmount;

	//re-open in read-only and re-generate our menu
	fd = open(GetMessageFile().c_str(), O_RDONLY);
	lseek(fd, _LastFileOffset, SEEK_SET);
	GenerateMessageList(fd, _DisplayingUnread);
	close(fd);
}

size_t BBSMessages::DoAction(size_t ID)
{
	if(_Menu == UserMessageMenu)
	{
		switch(ID)
		{
			case 0:		//read all
				DisplayAllMessages();
				break;

			case 1:		//read new
				DisplayNewMessages();
				break;

			case 2:		//mark all read
				MarkAllAsRead();
				break;

			case 3:		//delete all
				DeleteAllMessages();
				break;

			case 4:		//send new message
				SendNewMessage();
				break;

			case 5:		//back
				return 0;

			default:
				break;
		};
	}
	else if(_Menu == _MessageReadMenu)
	{
		if(ID && ID <= MESSAGE_LIST_COUNT)
			DisplayMessage(ID - 1);
		else if(ID == CharActionToInt('b')) //back
		{
				_SelectionType = SELECTION_ALPHA;
				_Menu = UserMessageMenu;
		}
		else if(ID == CharActionToInt('m')) //more
		{
			DisplayMoreMessages();
		}
	}

	return 1;
}

const uint8_t BBSMessages::RequiredPrivilege()
{
	return 0;
}


size_t BBSMessages::GetValue(const char *Name, void *Data, size_t DataLen)
{
	bool Ret;

    //if lasttime then return last announcement time
    if(strcasecmp(Name, "NewSinceLastRead") == 0)
    {
        if((DataLen < sizeof(Ret)) || !Data)
            return 0;

		Ret = NewSinceLastRead();
        memcpy(Data, &Ret, sizeof(Ret));
        return sizeof(Ret);
    }

    //default
    return 0;
}

BBSMenu *CreateMessageMenu(BBSMENU_PlUGIN_TYPE Type, ConnectionBase *Conn, BBSRender *Render, void *Value)
{
    switch(Type)
    {
        case BBSMENU_PlUGIN_TYPE::TYPE_NORMAL_WITH_VALUE:
            return new BBSMessages(Conn, Render, (bool)Value);

        case BBSMENU_PlUGIN_TYPE::TYPE_NO_PARAMS:
            return new BBSMessages();

        default:
            return new BBSMessages(Conn, Render, 0);
    }
}

__attribute__((constructor)) static void init()
{
    RegisterBBSMenuPlugin(CreateMessageMenu, "messages");
}