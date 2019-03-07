#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include "ftp.h"

struct FtpFile {
	char *filename;
	FILE *stream;
};

#define FTP_MAX_SERVER 100
static char _server[FTP_MAX_SERVER + 1];

#define FTP_MAX_USER 100
static char _user[FTP_MAX_USER + 1];

#define FTP_MAX_PASSWORD 100
static char _password[FTP_MAX_PASSWORD + 1];

#define FTP_MAX_REMOTE_ADDR 500
static char _remoteAddr[FTP_MAX_REMOTE_ADDR + 1];

static long int _ftpErrorCode = 0;
static CURLcode _curlErrorCode = CURLE_OK;
static char *_curlError = NULL;

static long int _timeOut = -1L;
 

static size_t writefunc(void *buffer, size_t size, size_t nmemb, void *stream)
{
	struct FtpFile *out = (struct FtpFile *)stream;
	if (out && !out->stream) {
		/* open file for writing */
		out->stream = fopen(out->filename, "wb");
		if (!out->stream) {
			_ftpErrorCode = FTP_ERROR_FAILED_TO_WRITE_DEST;
			return -1; /* failure, can't open file to write */
		}
	}
	return fwrite(buffer, size, nmemb, out->stream);
}


static size_t readfunc(void *ptr, size_t size, size_t nmemb, void *stream)
{
	curl_off_t nread;
	/* in real-world cases, this would probably get this data differently
     as this fread() stuff is exactly what the library already would do
     by default internally */ 
	if (ferror(f)) {
		return CURL_READFUNC_ABORT;
	}

	size_t retcode = fread(ptr, size, nmemb, stream);
 
	nread = (curl_off_t)retcode*size;
 
	// fprintf(stderr, "*** We read %" CURL_FORMAT_CURL_OFF_T " bytes from file\n", nread);
	return nread;
}



static int createRemotePath(char *fileName, char *directory,
	char *server, char *user, char *password)
{
	if (strlen(server) + strlen(user) + strlen(password) + strlen(directory) + strlen(fileName) + 7 >= FTP_MAX_REMOTE_ADDR) {
		return -1;
	}
	strcpy(_remoteAddr, "ftp://");
	strcat(_remoteAddr, user);
	strcat(_remoteAddr, ":");
	strcat(_remoteAddr, password);
	strcat(_remoteAddr, "@");
	strcat(_remoteAddr, server);
	strcat(_remoteAddr, directory);
	strcat(_remoteAddr, "/");
	strcat(_remoteAddr, fileName);
	return 0;
}


static curl_off_t getRemoteFileSize(char *remoteFile)
{
	curl_off_t remoteFileSizeByte = -1;
	CURL *curl = NULL;

	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(curl, CURLOPT_URL, remoteFile);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
	curl_easy_setopt(curl, CURLOPT_HEADER, 1);
	curl_easy_setopt(curl, CURLOPT_FILETIME, 1);

	if (_timeOut > 0) {
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, _timeOut);
	}
	_curlErrorCode = curl_easy_perform(curl);
	if (_curlErrorCode == CURLE_OK) {
		_curlErrorCode = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &remoteFileSizeByte);
		if( _curlErrorCode != CURLE_OK ) { 
			_curlError = (char *)curl_easy_strerror(_curlErrorCode);		
		}
	} else {
		_curlError = (char *)curl_easy_strerror(_curlErrorCode);				
	}
	curl_easy_cleanup(curl);

	return remoteFileSizeByte;
}

static int upload(CURL *curl, char *srcPath, char *remotePath)
{
	int returnStatus;
	FILE *f = NULL;
	struct stat file_info;
	curl_off_t fsize;
	struct curl_slist *headerlist = NULL;
	char rnfr[FTP_MAX_REMOTE_ADDR+1+5];
	char rnto[FTP_MAX_REMOTE_ADDR+1+5];
	char srcFileName[FTP_MAX_REMOTE_ADDR+1];

	// Searching for the '/' symbol and extracting the file name from the path
	int fileNameStart = 0;
	int pathLen = strlen(srcPath); 
	for( int i = pathLen - 2 ; i >= 0 ; i-- ) {
		if( srcPath[i] == '\\' || srcPath[i] == '/' ) {
			fileNameStart = i+1;
			break;
		}
	}
	if( pathLen - iFileNameStart > FTP_MAX_REMOTE_ADDR ) {
		return -2;
	}
	strcpy( srcFileName, &srcPath[ fileNameStart ] );

	// Getting the file size of the local file...
	if( stat(srcPath, &file_info) != 0 ) { 	
		returnStatus = -2;
	} else {
		fsize = (curl_off_t)file_info.st_size;
		//printf("Local file size: %" CURL_FORMAT_CURL_OFF_T " bytes.\n", fsize);

		f = fopen(srcPath, "rb");
		if( f == NULL ) {
			returnStatus = -2;
		} else {
			strcpy( rnfr, "RNFR " );
			strcat( rnfr, srcFileName );
			strcpy( rnto, "RNTO " );
			strcat( rnto, srcFileName )

			/* Building a list of commands to pass to libcurl */ 
			headerlist = curl_slist_append(headerlist, rnfr);
			headerlist = curl_slist_append(headerlist, rnto);

			/* we want to use our own read function */ 
			curl_easy_setopt(curl, CURLOPT_READFUNCTION, readfunc);

			/* enable uploading */ 
			curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

			/* specify target */ 
			curl_easy_setopt(curl, CURLOPT_URL, _remoteAddr);

			/* pass in that last of FTP commands to run after the transfer */ 
			curl_easy_setopt(curl, CURLOPT_POSTQUOTE, headerlist);

			/* now specify which file to upload */ 
			curl_easy_setopt(curl, CURLOPT_READDATA, f);

			/* Set the size of the file to upload (optional).  If you give a *_LARGE
			option you MUST make sure that the type of the passed-in argument is a
			curl_off_t. If you use CURLOPT_INFILESIZE (without _LARGE) you must
			make sure that to pass in a type 'long' argument. */ 
			curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
			     (curl_off_t)fsize);

			if (_timeOut > 0) {
				curl_easy_setopt(curl, CURLOPT_TIMEOUT, _timeOut);
			}
			_curlErrorCode = curl_easy_perform(curl);
			if( _curlErrorCode != CURLE_OK ) {
				returnStatus = -1;
				_curlError = (char *)curl_easy_strerror(_curlErrorCode);		
			} else {			
				returnStatus = 0;
			}

			/* clean up the FTP commands list */ 
			curl_slist_free_all(headerlist);

			fclose(f); /* close the local file */ 
		}
	}
}


static int download(CURL *curl, char *remotePath, char *dstFileName) {
	int returnStatus;

	struct FtpFile ftpfile = {
		dstFileName,
		NULL
	};

	curl_easy_setopt(curl, CURLOPT_URL, remotePath);
	/* Define our callback to get called when there's data to be written */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
	/* Set a pointer to our struct to pass to the callback */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ftpfile);
	/* Switch on full protocol/debug output */
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

	if (_timeOut > 0) {
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, _timeOut);
	}
	_curlErrorCode = curl_easy_perform(curl);
	if (_curlErrorCode != CURLE_OK) {
		_curlError = (char *)curl_easy_strerror(_curlErrorCode);		
		if( _curlError == CURLE_REMOTE_FILE_NOT_FOUND ) {
			returnStatus = -2;
		} else {
			returnStatus = -1;
		}
	} else {
		returnStatus = 0;		
	}
	return returnStatus;
}


int ftpTest(char *fileName,		// A file name to test
	char *directory,				// A server directory to test in
	unsigned long int *size) 
{
	int status = ftpTest( fileName, directory, _server, _user, _password, size );
	return status;
}


// Tests if a file exists at a server
int ftpTest(char *fileName,					// A file name to test
	char *directory,							// A server directory to test in
	char *server, char *user, char *password,	// Connection requisites: server, login, password
	unsigned long int *size)
{
	_ftpErrorCode = 0;
	_curlErrorCode = CURLE_OK;

	if (createRemotePath(fileName, directory, server, user, password) == -1) {
		return -1;
	}

	CURL *curl = curl_easy_init();
	if (curl == NULL) {
		return -1;
	}

	curl_off_t remoteFileSizeByte = getRemoteFileSize(_remoteAddr);
	if (remoteFileSizeByte == -1) {
		if (_curlErrorCode == CURLE_REMOTE_FILE_NOT_FOUND) {
			return 0;
		}
		else {
			return -1;
		}
	}

	if (size != NULL) {
		*size = remoteFileSizeByte;
	}

	return 1;
}


int ftpUpload(char *srcFileName, char *dstFileName, char *dstDirectory ) {
	int status = ftpUploadP(srcFileName, dstFileName, dstDirectory, server, user, password);
	return status;
}


int ftpUploadP(char *srcFileName, char *dstFileName, char *dstDirectory,
	char *server, char *user, char *password)
{
	_curlErrorCode = CURLE_OK;

	if (createRemotePath(dstFileName, dstDirectory, server, user, password) == -1) {
		_ftpErrorCode = -1;
	} else {
		CURL *curl = curl_easy_init();
		if (curl == NULL) {
			_ftpErrorCode = -1;
		} else {
			int status = upload(curl, srcFileName, _remoteAddr);
			if( status == 0 ) {
				_ftpErrorCode = 0;
			} else {
				if( status == -2 ) {
					_ftpErrorCode = FTP_ERROR_FAILED_TO_READ_LOCAL;
				} else {
					_ftpErrorCode = -1;
				}
			}
			curl_easy_cleanup(curl);		
		}
	}

	return _ftpErrorCode;
}


int ftpDownload(char *dstFileName, char *srcFileName, char *srcDirectory ) {
	int status = ftpDownloadP(dstFileName, srcFileName, srcDirectory, _server, _user, _password);
	return status;
}


int ftpDownloadP(char *dstFileName, char *srcFileName, char *srcDirectory,
	char *server, char *user, char *password)
{
	_curlErrorCode = CURLE_OK;

	if (createRemotePath(srcFileName, srcDirectory, server, user, password) == -1) {
		_ftpErrorCode = -1;
	} else {
		CURL *curl = curl_easy_init();
		if (curl == NULL) {
			_ftpErrorCode = -1;
		} else {
			int status = download(curl, _remoteAddr, dstFileName);
			if( status != 0 ) {
				if( _curlErrorCode != CURLE_REMOTE_FILE_NOT_FOUND ) {
					_ftpErrorCode = FTP_ERROR_FAILED_TO_READ_REMOTE;
				} else {
					_ftpErrorCode = -1;				
				}
			} else {
				_ftpErrorCode = 0;
			}
			curl_easy_cleanup(curl);		
		}
	}
	return _ftpErrorCode;
}


void ftpSetTimeOut(unsigned long int timeOut) {
	_ftpErrorCode = 0;
	_timeOut = timeOut;
}


void ftpSetCredentials(char *server, char *user, char *password) {
	_ftpErrorCode = 0;

	if( strlen(server) > FTP_MAX_SERVER || strlen(user) > FTP_MAX_USER || strlen(password) > FTP_MAX_PASSWORD ) {
		_ftpErrorCode = -1;
	} else {
		strcpy( _server, server );
		strcpy( _user, user );
		strcpy( _password, password );		
	}
	return _ftpErrorCode;
}


int ftpInit(void) {
	_ftpErrorCode = 0;
	_curlErrorCode = CURLE_OK;

	if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
		return -1;
	}
	return 0;
}


void ftpClose(void) {
	_ftpErrorCode = 0;
	_curlErrorCode = CURLE_OK;

	curl_global_cleanup();
}


int ftpGetLastError(int *ftpErrorCode, int *curlErrorCode, char *curlErrorText) {
	if (ftpErrorCode != NULL) {
		*ftpErrorCode = _ftpErrorCode;
	}
	if (curlErrorCode != NULL) {
		*curlErrorCode = _curlErrorCode;
	}
	if (curlErrorCode != NULL) {
		curlErrorText = (char *)curl_easy_strerror(_curlErrorCode);
	}
	return 0;
}