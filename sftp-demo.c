// A demo for sftp file transfer
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "sftp.h"

static char *_server = "u38989.ssh.masterhost.ru";
static char *_user = "u38989";
static char *_password = "amitin9sti";
static char *_dstDirectory = "/home/u38989";

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char* cmdLine, int nCmdShow) {
	int status;
  long unsigned int size;
	FILE *outp;

	outp = fopen("sftp-demo.run", "w");
	if( outp == NULL ) {
		goto lab_exit;
	}
	
  if( __argc < 4 ) {
    fprintf( outp, "Argument is missing!\nUsage: sftp.exe <file-to-test-at-server> <file-to-upload> <file-to-download>\n");
    goto lab_close;
  }

  if( sftpInit() == -1 ) { // A must...
    fprintf( outp, "Error initializing SFTP!\nExiting...\n");
		goto lab_close;
  }

  if( sftpSetCredentials(_server, _user_, _password)  == -1 ) {
    fprintf( outp, "Error setting credentials!\nExiting...\n");
    goto lab_close;    
  }

  // Testing if a file exists at the server...
  status = sftpTest( __argv[1], _dstDirectory, &size );
  if( status == 1 ) {
    fprintf( outp, "The file %s exists! (length=%lu)\n", __argv[1], size );
  } else if( status == 0 ) {
    fprintf( outp, "The file %s does not exist!\n", __argv[1] );
  } else {
    int error;
    sftpGetLastError( NULL, &error, NULL ); // Getting the error code.
    if( error == CURLE_REMOTE_FILE_NOT_FOUND ) {
      fprintf( outp, "The file not found!\n");
    } else if( error == CURLE_COULDNT_RESOLVE_HOST ) {
      fprintf( outp, "Server not found!\n");      
    } else if( error == CURLE_REMOTE_ACCESS_DENIED ) {
      fprintf( outp, "Access denied!\n");      
    } else {
      fprintf( outp, "Error %d occured!\n", error );      
    }
  }

  // Uploading...
  status = sftpUpload( __argv[2], __argv[2], _dstDirectory );
  if( status == 0 ) {
    fprintf( outp, "Uploaded Ok!\n");
  } else {
    fprintf( outp, "Error uploading file!\n");    
  }

  // Downloading
  status = sftpDownload( __argv[3], __argv[3], _dstDirectory );
  if( status == 0 ) {
    fprintf( outp, "Downloaded Ok!\n");
  } else {
		int error;
		sftpGetLastError(NULL, &error, NULL);
    fprintf( outp, "Error downloading file (%d, %d)!\n", status, error);    
  }    

  sftpClose();

	lab_close:
	fclose(outp);

	lab_exit:
  exit(0);
}
