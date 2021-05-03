#ifndef NEKISAHLOTH_LIB_BBS_FILES
#define NEKISAHLOTH_LIB_BBS_FILES

#include <vector>
#include <string>
#include <unistd.h>
#include <stdint.h>
#include <BBSMenu.h>

using namespace std;

class BBSFiles : public BBSMenu
{
	public:

		BBSFiles(ConnectionBase *Conn, BBSRender *Render);
		size_t DoAction(size_t ID);

	private:
		void BuildFileList();
		void FillMenuList(int Direction);
		void SendFile(size_t Index);
		void HandleUpload();
		void PrintSkull(string WarningFile);

		typedef struct FileListEntry
		{
			string Name;
			string Description;
			string Path;
			size_t Size;
			bool IsDirectory;
		} FileListEntry;

		string _CurrentPath;
		string _PromptStr;
		const char *_BasePath;
		size_t _CurrentIndex;
		vector<FileListEntry> _FileList;
};

#endif
