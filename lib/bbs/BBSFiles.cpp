#include <fstream>
#include <iostream>
#include <filesystem>
#include <string>
#include <set>
#include <stdio.h>
#include <malloc.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include "BBSFiles.h"
#include "BBSUser.h"
#include "HashBase.h"
#include "BBSUtils.h"

#define MAX_FILE_COUNT 9

using namespace std;

BBSFiles::BBSFiles(ConnectionBase *Conn, BBSRender *Render) :
	BBSMenu(Conn, Render)
{
	LargeHeader = "FILE MENU";
	Header = "FILES";
	_SelectionType = SELECTION_NUMERIC;
	HeaderColor = BBSRender::COLOR_YELLOW;

	_Menu = 0;

	_CurrentPath = "/";
	_BasePath = "./files";
	_CurrentIndex = 0;

	BuildFileList();

	//add ourselves for the menu entry
	AddMenu(this);
}

#define READ_BUF_SIZE 8192
void BBSFiles::SendFile(size_t Index)
{
    int fd;
    char *Buffer;
    size_t ReadSize;
	size_t WriteSize;

	HashBase *Hash = GetHash(MAGIC_VAL('M', 'D', '5', 0))();

    _Conn->WriteFString("Sending %d raw bytes for %s\n", _FileList[Index].Size, _FileList[Index].Name.c_str());

    fd = open(_FileList[Index].Path.c_str(), O_RDONLY);
    if(fd >= 0)
    {
        Buffer = (char *)malloc(READ_BUF_SIZE);
        do
        {
            ReadSize = read(fd, Buffer, READ_BUF_SIZE);
			Hash->AddData(Buffer, ReadSize);
			WriteSize = 0;
			while(WriteSize < ReadSize)
            	WriteSize += _Conn->Write(&Buffer[WriteSize], ReadSize - WriteSize);

        } while(ReadSize == READ_BUF_SIZE);

        close(fd);
    }

    _Conn->WriteFString("==DONE== MD5 %s\n", Hash->GetHashAsString());
	delete Hash;

	//update user stats
	GlobalUser->AddDownloadTrafficStats(_FileList[Index].Size);
}

size_t BBSFiles::DoAction(size_t ID)
{
    string CurDir;

	switch(ID)
	{
		case CharActionToInt('b'):  //back
            if(_CurrentPath.compare("/"))
            {
                //chop off a full path entry
                _CurrentPath = _CurrentPath.substr(0, _CurrentPath.rfind('/'));
                if(!_CurrentPath.length())
                    _CurrentPath = "/";
                BuildFileList();
            }
            break;

		case CharActionToInt('p'):  //previous
            if(_CurrentIndex > 0)
			    FillMenuList(-1);
			break;

		case CharActionToInt('n'):  //next
            if((_CurrentIndex + MAX_FILE_COUNT) < _FileList.size())
			    FillMenuList(1);
			break;

		case CharActionToInt('u'):  //upload
            //handle upload
			HandleUpload();
			break;

		case CharActionToInt('q'):  //quit
            //quit
			return 0;

        default:
            if(ID && (ID <= MAX_FILE_COUNT))
            {
                ID--;
                if(_FileList[_CurrentIndex + ID].IsDirectory)
                {
                    //add a / if required
                    if(_CurrentPath.compare("/"))
                        _CurrentPath += "/";
                    _CurrentPath += _FileList[_CurrentIndex + ID].Name;
                    BuildFileList();
                }
                else
                    SendFile(_CurrentIndex + ID);
            }
            else if(ID == 99)
            {
                //if a hidden folder exists then go to it
                CurDir = _BasePath;
                CurDir += _CurrentPath;
                //add a / if required
                if(_CurrentPath.compare("/"))
                    CurDir += "/hidden";
                else
                    CurDir += "hidden";

                if(filesystem::is_directory(CurDir))
                {
                    //add a / if required
                    if(_CurrentPath.compare("/"))
                        _CurrentPath += "/";

                    _CurrentPath.append("hidden");
                    BuildFileList();

					//if there is a warning file then display a skull warning
					if(filesystem::is_regular_file(CurDir + "/WARNING"))
						PrintSkull(CurDir + "/WARNING");
                }
            }
	};

	return 1;
}

void BBSFiles::PrintSkull(string WarningFile)
{
	char Buffer[BBSRender::RENDER_WIDTH + 1 - 9];
	ssize_t ReadLen;
	int fd;

	const char *Skull =
"                 uuuuuuu\n" \
"             uu$$$$$$$$$$$uu\n" \
"          uu$$$$$$$$$$$$$$$$$uu\n" \
"         u$$$$$$$$$$$$$$$$$$$$$u\n" \
"        u$$$$$$$$$$$$$$$$$$$$$$$u\n" \
"       u$$$$$$$$$$$$$$$$$$$$$$$$$u\n" \
"       u$$$$$$$$$$$$$$$$$$$$$$$$$u\n" \
"       u$$$$$$\"   \"$$$\"   \"$$$$$$u\n" \
"       \"$$$$\"      u$u       $$$$\"\n" \
"        $$$u       u$u       u$$$\n" \
"        $$$u      u$$$u      u$$$\n" \
"         \"$$$$uu$$$   $$$uu$$$$\"\n" \
"          \"$$$$$$$\"   \"$$$$$$$\"\n" \
"            u$$$$$$$u$$$$$$$u\n" \
"             u$\"$\"$\"$\"$\"$\"$u\n" \
"  uuu        $$u$ $ $ $ $u$$       uuu\n" \
" u$$$$        $$$$$u$u$u$$$       u$$$$\n" \
"  $$$$$uu      \"$$$$$$$$$\"     uu$$$$$$\n" \
"u$$$$$$$$$$$uu    \"\"\"\"\"    uuuu$$$$$$$$$$\n" \
"$$$$\"\"\"$$$$$$$$$$uuu   uu$$$$$$$$$\"\"\"$$$\"\n" \
" \"\"\"      \"\"$$$$$$$$$$$uu \"\"$\"\"\"\n" \
"           uuuu ""$$$$$$$$$$uuu\n" \
"  u$$$uuu$$$$$$$$$uu \"\"$$$$$$$$$$$uuu$$$\n" \
"  $$$$$$$$$$\"\"\"\"           \"\"$$$$$$$$$$$\"\n" \
"   \"$$$$$\"                      \"\"$$$$\"\"\n" \
"     $$$\"                         $$$$\"\n";

	#define SKULL_WIDTH 42

	//clear and draw the skull page
	_Render->Clear();
	//center X and draw skull
	_Render->SetX((BBSRender::RENDER_WIDTH - SKULL_WIDTH) / 2);
	_Render->PrintF("%s", BBSRender::COLOR_BLACK, BBSRender::COLOR_RED, Skull);

	//draw our text
	#define SKULL_RIGHT (((BBSRender::RENDER_WIDTH - SKULL_WIDTH) / 2) + SKULL_WIDTH)
	_Render->DrawHeaderAt(BBSRender::COLOR_BLACK, BBSRender::COLOR_MAGENTA, 1, 1, "E N T E R");
	_Render->DrawHeaderAt(BBSRender::COLOR_BLACK, BBSRender::COLOR_MAGENTA, SKULL_RIGHT + 3, 1, "O W N");
	_Render->DrawHeaderAt(BBSRender::COLOR_BLACK, BBSRender::COLOR_WHITE, 7, 5, "A T");
	_Render->DrawHeaderAt(BBSRender::COLOR_BLACK, BBSRender::COLOR_WHITE, SKULL_RIGHT + 2, 5, "R I S K");
	_Render->DrawHeaderAt(BBSRender::COLOR_BLACK, BBSRender::COLOR_RED, 2, 9, "Y O U R");
	_Render->DrawHeaderAt(BBSRender::COLOR_BLACK, BBSRender::COLOR_RED, SKULL_RIGHT + 5, 9, "X X X");

	//move to the beginning and print the warning from the file
	_Render->SetX(0);
	_Render->OffsetY(-1);
	fd = open(WarningFile.c_str(), O_RDONLY);

	memset(Buffer, ' ', sizeof(Buffer)-1);
	ReadLen = read(fd, Buffer, sizeof(Buffer) - 1);
	close(fd);
	if(Buffer[ReadLen-1] == '\n')
		Buffer[ReadLen-1] = ' ';
	_Render->PrintF("WARNING: %s\n", BBSRender::COLOR_RED_BRIGHT, BBSRender::COLOR_BLACK, Buffer);
	_Render->Render(BBSRender::COLOR_BLACK);

	//wait on key press
	_Conn->Readline(Buffer, 1);
}

#define FILE_BUFFER_SIZE (1024*50)

void BBSFiles::HandleUpload()
{
	char Filename[256];
	char FileDescription[256];
	char FileSizeStr[10];
	size_t FileSize;
	HashBase *Hash;
	int fd;
	string FilePath;
	string TempFilePath;
	string NFOFilePath;
	char *CharPos;
	char *FileBuffer;
	size_t ReadLen;
	bool FilenameValid;

	//handle a file upload
	_Render->Clear();
	_Render->DrawHeader(BBSRender::COLOR_BLACK, BBSRender::COLOR_YELLOW, "File Upload");
	_Render->OffsetY(1);
	_Render->PrintF("Filename: ", BBSRender::COLOR_BLACK, BBSRender::COLOR_RED);
	_Render->Render(BBSRender::COLOR_WHITE);

	//get the filename
	memset(Filename, 0, sizeof(Filename));
	_Conn->Readline(Filename, sizeof(Filename)-1);

	//update the screen with the data and get the description
	_Render->PrintF("%s\n", BBSRender::COLOR_BLACK, BBSRender::COLOR_WHITE, Filename);
	_Render->SetX(0);

	//see if .. or / is in the filename, if so then fail
	FilenameValid = (strchr(Filename, '/') != 0);
	FilenameValid |= (strstr(Filename, "..") != 0);

	//see if the end of the filename is .tmp
	if(strlen(Filename) > 4)
		FilenameValid |= (strcasecmp(&Filename[strlen(Filename) - 4], ".tmp") == 0);

	if(FilenameValid)
	{
		//invalid, alert them and wait for input before exiting
		_Render->OffsetY(1);
		_Render->PrintF("ERROR - Filename is invalid! %2~Press enter to exit", BBSRender::COLOR_BLACK, BBSRender::COLOR_RED_BRIGHT, BBSRender::COLOR_YELLOW);
		_Render->Render(BBSRender::COLOR_WHITE);
		_Conn->Readline(Filename, 1);
		return;
	}

	//get the description
	_Render->PrintF("Description: ", BBSRender::COLOR_BLACK, BBSRender::COLOR_RED);
	_Render->Render(BBSRender::COLOR_WHITE);
	memset(FileDescription, 0, sizeof(FileDescription));
	_Conn->Readline(FileDescription, sizeof(FileDescription)-1);

	//update the screen with the data and get the size
	_Render->PrintF("%s\n", BBSRender::COLOR_BLACK, BBSRender::COLOR_WHITE, FileDescription);
	_Render->SetX(0);
	_Render->PrintF("Size in bytes: ", BBSRender::COLOR_BLACK, BBSRender::COLOR_RED);
	_Render->Render(BBSRender::COLOR_WHITE);
	memset(FileSizeStr, 0, sizeof(FileSizeStr));
	_Conn->Readline(FileSizeStr, sizeof(FileSizeStr)-1);

	//update the screen with the info and then validate that the size is appropriate
	_Render->PrintF("%s\n", BBSRender::COLOR_BLACK, BBSRender::COLOR_WHITE, FileSizeStr);
	_Render->SetX(0);
	FileSize = atol(FileSizeStr);

	if(!FileSize || (FileSize > (1024*100)))
	{
		//invalid, alert them and wait for input before exiting
		_Render->OffsetY(1);
		_Render->PrintF("ERROR - File size is invalid! %2~Press enter to exit", BBSRender::COLOR_BLACK, BBSRender::COLOR_RED_BRIGHT, BBSRender::COLOR_YELLOW);
		_Render->Render(BBSRender::COLOR_WHITE);
		_Conn->Readline(Filename, 1);
		return;
	}

	//see if a file already exists with this name, if so then someone else is uploading or already uploaded
	FilePath = "./files/upload/";
	FilePath += Filename;
	fd = open(FilePath.c_str(), O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if(fd < 0)
	{
		_Render->PrintF("File already exists, unable to upload %2~Press enter to exit", BBSRender::COLOR_BLACK, BBSRender::COLOR_RED, BBSRender::COLOR_YELLOW);
		_Render->Render(BBSRender::COLOR_WHITE);
		_Conn->Readline(Filename, 1);
		return;
	}	

	//close the file but leave it and set our temporary filename
	close(fd);
	TempFilePath = FilePath + ".tmp";

	//size seems good, tell them to send the data and what to end with
	_Render->PrintF("Send file, end data with %2~==DONE== MD5%1~ followed by the MD5 hash and newline\n", BBSRender::COLOR_BLACK, BBSRender::COLOR_YELLOW, BBSRender::COLOR_GREEN);
	_Render->Render(BBSRender::COLOR_WHITE);

	//prep for the final message we send them later
	_Render->OffsetY(1);
	_Render->SetX(0);

	Hash = GetHash(MAGIC_VAL('M', 'D', '5', 0))();

	//open up a temp file and store into it
	fd = open(TempFilePath.c_str(), O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	FileBuffer = (char *)malloc(FILE_BUFFER_SIZE);

	//update the user stats
	GlobalUser->AddUploadTrafficStats(FileSize);

	//regardless if the temp file fails go ahead and read data
	while(FileSize)
	{
		//read up to 50k of data in 1 read
		ReadLen = FILE_BUFFER_SIZE;
		if(ReadLen > FileSize)
			ReadLen = FileSize;

		ReadLen = _Conn->ReadAll(FileBuffer, ReadLen);
		if(ReadLen)
		{
			//write it out
			if(fd >= 0)
				write(fd, FileBuffer, ReadLen);

			//update our hash
			Hash->AddData(FileBuffer, ReadLen);
			FileSize -= ReadLen;
		}
	};

	//close the file
	if(fd >= 0)
		close(fd);

	//get the final line for the hash
	_Conn->Readline(FileBuffer, FILE_BUFFER_SIZE);

	//confirm we got ==DONE== MD5
	if(strncmp(FileBuffer, "==DONE== MD5 ", 13) != 0)
	{
		unlink(FilePath.c_str());
		unlink(TempFilePath.c_str());
		free(FileBuffer);
		delete Hash;

		_Render->PrintF("ERROR - Failed to find end of data marker! %2~Press enter to exit", BBSRender::COLOR_BLACK, BBSRender::COLOR_RED_BRIGHT, BBSRender::COLOR_YELLOW);
		_Render->Render(BBSRender::COLOR_WHITE);
		_Conn->Readline(Filename, 1);
		return;
	}

	//confirm the hash
	if(!Hash->CompareHashAsString(&FileBuffer[13]))
	{
		unlink(FilePath.c_str());
		unlink(TempFilePath.c_str());
		free(FileBuffer);
		delete Hash;

		_Render->PrintF("ERROR - Hash validation failed! %2~Press enter to exit", BBSRender::COLOR_BLACK, BBSRender::COLOR_RED_BRIGHT, BBSRender::COLOR_YELLOW);
		_Render->Render(BBSRender::COLOR_WHITE);
		_Conn->Readline(Filename, 1);
		return;
	}

	//erase the objects
	free(FileBuffer);
	delete Hash;

	//write out the description
	if((fd >= 0) && strlen(FileDescription))
	{
		NFOFilePath = FilePath + ".nfo";
		fd = creat(NFOFilePath.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

		//replace all newlines
		CharPos = strchr(FileDescription, '\n');
		while(CharPos)
		{
			*CharPos=' ';
			CharPos = strchr(CharPos, '\n');
		}

		write(fd, FileDescription, strlen(FileDescription));
		close(fd);
	}

	if(fd >= 0)
	{
		//rename the temporary file to the real file
		rename(TempFilePath.c_str(), FilePath.c_str());
		_Render->PrintF("File Uploaded! Press enter to exit", BBSRender::COLOR_BLACK, BBSRender::COLOR_YELLOW);
	}
	else
	{
		//had issues, delete files
		unlink(FilePath.c_str());
		unlink(TempFilePath.c_str());
		_Render->PrintF("File failed to upload! Press enter to exit", BBSRender::COLOR_BLACK, BBSRender::COLOR_RED);
	}

	_Render->Render(BBSRender::COLOR_WHITE);
	_Conn->Readline(Filename, 1);
	return;
}

void BBSFiles::BuildFileList()
{
	int fd;
	size_t count;
	string nfo;
	string CurDir;
	char nfo_buffer[100];
	size_t read_len;
	set<filesystem::path> sorted;
	string TempFilename;

	//wipe out the current file list and rebuild it

	//erase the menu we have
	if(_Menu)
	{
		//erase the menu itself
		for(count = 0; _Menu[count].Name; count++)
			free(_Menu[count].Data);
		free(_Menu);
		_Menu = 0;
	}

	_FileList.clear();
	_CurrentIndex = 0;

	//get a list of directories
	CurDir = _BasePath;
	CurDir += _CurrentPath;

    //set the box title to our directory
    Header = _CurrentPath.c_str();
	for(int DoFiles = 0; DoFiles < 2; DoFiles++)
	{
		sorted.clear();
		for (const auto & entry : filesystem::directory_iterator(CurDir))
		{
			//if not an nfo file then add a file entry and see if an nfo exists for the file
			if(entry.path().extension().empty() || (entry.path().extension().string().compare(".nfo") != 0))
			{
				//skip . .. and hidden
				if((entry.path().filename().compare(".") == 0) || (entry.path().filename().compare("..") == 0) ||
                   (entry.path().filename().compare("hidden") == 0) || (entry.path().filename().compare("WARNING") == 0))
					continue;

				//if in the upload folder and we have a .tmp name then skip this entry
				if(_CurrentPath.compare("/upload") == 0)
				{
					//is a .tmp file
					if(entry.path().extension().string().compare(".tmp") == 0)
						continue;

					//not a tmp file, see if a .tmp file exists, if so then don't display this entry
					TempFilename = entry.path().string() + ".tmp";
					if(filesystem::exists(TempFilename))
						continue;
				}

				//add to the sorted list
				if((!DoFiles && entry.is_directory()) ||
					(DoFiles && !entry.is_directory()))
					sorted.insert(entry.path());
			}
		}

		//now setup our specific file list that is now in a sorted order
		for(const auto & entry : sorted)
		{
			//add an entry to the list
			_FileList.push_back(FileListEntry());
			_FileList[_FileList.size() - 1].Name = entry.filename();
			_FileList[_FileList.size() - 1].Path = entry;
			_FileList[_FileList.size() - 1].IsDirectory = filesystem::is_directory(entry);
			if(!_FileList[_FileList.size() - 1].IsDirectory)
			{
				_FileList[_FileList.size() - 1].Size = filesystem::file_size(entry);
			}
			else
				_FileList[_FileList.size() - 1].Size = 0;

			//see if the .nfo file exists for this entry
			nfo = entry;
			nfo += ".nfo";
			if(filesystem::exists(nfo))
			{
				fd = open(nfo.c_str(), O_RDONLY);
				if(fd >= 0)
				{
					read_len = read(fd, nfo_buffer, sizeof(nfo_buffer) - 1);
					nfo_buffer[read_len] = 0;
					close(fd);

					//if the last char is a newline get rid of it
					if((nfo_buffer[read_len-1] == '\n') || (nfo_buffer[read_len-1] == ' '))
						nfo_buffer[read_len-1] = 0;
					_FileList[_FileList.size() - 1].Description = nfo_buffer;
				}

			}
		}
	}

    //make sure the menu is rebuilt
	FillMenuList(0);
}

void BBSFiles::FillMenuList(int Direction)
{
	size_t count;
	size_t num_entries;
	size_t MaxNameLen, CurLen;
	string *NewName;
	size_t FileSize[2];
    char FileSizeFmt;
	char SizeBuffer[100];

	//Direction of -1 indicates a group backwards
	//Direction of +1 indicates a group forward
	//Direction of 0 is read from where we are

	//erase the menu we have
	if(_Menu)
	{
		//erase the menu itself
		for(count = 0; _Menu[count].Name; count++)
			delete (string *)_Menu[count].Data;
		free(_Menu);
		_Menu = 0;
	}

	//adjust our index based on direction and if there is room
	if((Direction < 0) && (_CurrentIndex > 0))
		_CurrentIndex -= MAX_FILE_COUNT;
	else if((Direction > 0) && ((_CurrentIndex + MAX_FILE_COUNT) < _FileList.size()))
		_CurrentIndex += MAX_FILE_COUNT;
	else
		_CurrentIndex = 0;

	//allocate enough spots for the data we need to fill
	num_entries = MAX_FILE_COUNT;
	if((_CurrentIndex + num_entries) > _FileList.size())
		num_entries = _FileList.size() - _CurrentIndex;

	//get the maximum name length
	MaxNameLen = _Render->GetStringLen(_FileList[_CurrentIndex].Name.c_str());
	for(count = 0; count < num_entries; count++)
	{
		CurLen = _Render->GetStringLen(_FileList[_CurrentIndex + count].Name.c_str());
		if(CurLen > MaxNameLen)
			MaxNameLen = CurLen;
	}

	//fill in the menu
	_Menu = (MenuEntryStruct *)calloc((num_entries + 1), sizeof(MenuEntryStruct));
	for(count = 0; count < num_entries; count++)
	{
		//create the new name
		NewName = new string();

		//setup the name with padding
		NewName->append(_FileList[_CurrentIndex + count].Name);
		CurLen = _Render->GetStringLen(NewName->c_str());
		while(CurLen < MaxNameLen)
		{
			NewName->append(" ");
			CurLen++;
		}

		if(_FileList[_CurrentIndex + count].IsDirectory)
		{
			_Menu[count].Color = BBSRender::COLOR_GREEN;
			NewName->append("        ");
		}
		else
		{
			//a file, determine what size to report
			GetAmountAndMarker(_FileList[_CurrentIndex + count].Size, &FileSizeFmt, FileSize);

			_Menu[count].Color = BBSRender::COLOR_NOCHANGE;
			snprintf(SizeBuffer, sizeof(SizeBuffer) - 1, "  %3ld.%02ld %cb  ", FileSize[0], FileSize[1], FileSizeFmt);
			NewName->append(SizeBuffer);
		}

		NewName->append(_FileList[_CurrentIndex + count].Description);

		_Menu[count].ID = count + 1;
		_Menu[count].Help = 0;
		_Menu[count].Data = NewName;
		_Menu[count].Name = NewName->c_str();
	}

	//alter the prompt as needed
    _PromptStr = "%3~[%2~Q%3~]%1~uit";

    //if we are in a sub directory allow back
    if(_CurrentPath.compare("/") != 0)
        _PromptStr += " %3~[%2~B%3~]%1~ack";

    //if we can do previous then allow it
	if(_CurrentIndex)
		_PromptStr += " %3~[%2~P%3~]%1~revious";

    //if not at the end
    if((_CurrentIndex + MAX_FILE_COUNT) <= _FileList.size())
        _PromptStr += " %3~[%2~N%3~]%1~ext";

    //upload and download prompt        
    _PromptStr += " %3~[%2~U%3~]%1~pload or File to download >>";
    Prompt = _PromptStr.c_str();
}

BBSMenu *CreateFilesMenu(BBSMENU_PlUGIN_TYPE Type, ConnectionBase *Conn, BBSRender *Render, void *Value)
{
	return new BBSFiles(Conn, Render);
}

__attribute__((constructor)) static void init()
{
    RegisterBBSMenuPlugin(CreateFilesMenu, "files");
}