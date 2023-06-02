#ifndef SIMUL_BASE_FILEINTERFACE_H
#define SIMUL_BASE_FILEINTERFACE_H
#include <vector>
#include <set>
#include <string>
#include "Platform/Core/Export.h"
namespace platform
{
	namespace core
	{
		extern std::string GetExeDirectory();
		//! An interface to derive from so you can provide your own file load/save functions.
		//! Use SetFileLoader to define the object that Simul will use for file handling.
		//! The default is platform::core::DefaultFileLoader, which uses standard file handling.
		class PLATFORM_CORE_EXPORT FileLoader
		{
		public:
			//! Returns a pointer to the current file handler.
			static FileLoader *GetFileLoader();
			//! Returns true if and only if the named file exists. If it has a relative path, it is relative to the current directory.
			virtual bool FileExists(const char *filename_utf8) const=0;
			//! Set the file handling object: call this before any file operations, if at all.
			static void SetFileLoader(FileLoader *f);
			//! Put the file's entire contents into memory, by allocating sufficiently many bytes, and setting the pointer.
			//! The memory should later be freed by a call to ReleaseFileContents.
			//! The filename should be unicode UTF8-encoded.
			virtual void AcquireFileContents(void*& pointer, unsigned int& bytes, const char* filename_utf8,bool open_as_text)=0;
			//! Get the file date as a julian day number. Return zero if the file doesn't exist.
			virtual double GetFileDate(const char* filename_utf8) const=0;
			//! Free the memory allocated by AcquireFileContents.		
			virtual void ReleaseFileContents(void* pointer)=0;
			//! Save the chunk of memory to storage.
			virtual bool Save(const void* pointer, unsigned int bytes, const char* filename_utf8,bool save_as_text)=0;
			virtual std::vector<std::string> ListDirectory(const std::string &path) const;
			
			//! Load the file as an std::string.
			std::string LoadAsString(const char* filename_utf8);
			//! Load the file as an std::string.
			std::string LoadAsString(const char *filename_utf8,const std::vector<std::string>& paths);
			void AcquireFileContents(void*& pointer, unsigned int& bytes, const char* filename_utf8, const std::vector<std::string>& paths, bool open_as_text);

			static std::string FindParentFolder(const char *folder_utf8);
			//! Find the named file relative to one of a given list of paths. Searches from the top of the stack.
			std::string FindFileInPathStack(const char *filename_utf8,const std::vector<std::string> &path_stack_utf8) const;
			//! Find the named file relative to one of a given list of paths, and return the index in the list, -1 if the file was found on the general search path, or -2 if it was not found. Searches from the top of the stack.
			// If more than one exists, the newest file is used.

			int FindIndexInPathStack(const char *filename_utf8,const std::vector<std::string> &path_stack_utf8) const;
			//! Record all the files we load - useful for installers etc.
			void SetRecordFilesLoaded(bool b)
			{
				recordFilesLoaded=b;
			}
			bool GetRecordFilesLoaded() const
			{
				return recordFilesLoaded;
			}
			const std::set<std::string> &GetFilesLoaded() const
			{
				return filesLoaded;
			}
		protected:
			//! Find the named file relative to one of a given list of paths. Searches from the top of the stack.
			std::string FindFileInPathStack(const char *filename_utf8,const char * const* path_stack_utf8) const;
			//! Find the named file relative to one of a given list of paths, and return the index in the list, -1 if the file was found on the general search path, or path_stack_utf8.size() if it was not found. Searches from the top of the stack.
			int FindIndexInPathStack(const char *filename_utf8,const char * const* path_stack_utf8) const;
			bool recordFilesLoaded=false;
			std::set<std::string> filesLoaded;
		};
	}
}
#endif