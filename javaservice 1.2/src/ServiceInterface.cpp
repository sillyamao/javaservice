//Copyright (c) 2000, Alexandria Software Consulting
//
//All rights reserved. Redistribution and use in source 
//and binary forms, with or without modification, are permitted provided 
//that the following conditions are met:
// - Redistributions of source code must retain the above copyright notice, 
//   this list of conditions, and the following disclaimer.
// - Neither name of Alexandria Software Consulting nor the names of the 
//   contributors may be used to endorse or promote products derived from 
//   this software without specific prior written permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS 
//AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
//INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY 
//AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL 
//THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
//INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
//NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
//DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
//THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
//NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, 
//EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "jni.h"
#include "JavaInterface.h"
#include "ServiceInterface.h"
#include "Messages.h"

//Flag for whether we are trying to install a service.
bool installing = false;

//Flag for whether we are trying to uninstall a service.
bool uninstalling = false;

//The name of the service.
char *serviceName = NULL;

//The location of the jvm library.
char *jvmLibrary = NULL;

//The number of jvm options.
int jvmOptionCount=0;

//The jvm options.
char **jvmOptions = NULL;

//The start class for the service.
char *startClass = NULL;

//The start method for the service.
char *startMethod = "main";

//The number of parameters for the start method.
int startParamCount=0;

//The start method parameters.
char **startParams = NULL;

//The stop class for the service.
char *stopClass = NULL;

//The stop method for the service.
char *stopMethod = "main";

//The number of parameters for the stop method.
int stopParamCount=0;

//The stop method parameters.
char **stopParams = NULL;

//The out redirect file.
char *outFile = NULL;

//The err redirect file.
char *errFile = NULL;

///////// This section added by Lars Johanson IVF 2001-02-26
//The path extension.
char *pathExt = NULL;
///////// End of added section

//The current directory.
char *currentDirectory = NULL;

//////// This section added by John Rutter, Multiplan Consultants Ltd, 2002-11-03 (V1.2.1)

// NT service dependency
const char* dependsOn = NULL;

// Constant for number of milliseconds delay timeout when stopping the service
static const long SHUTDOWN_TIMEOUT_MSECS = 30000; // 30 seconds

// Constant for number of milliseconds delay after exit handler has been triggered
static const long EXIT_HANDLER_TIMEOUT_MSECS = 90000; // 90 seconds

///////// End of added section (John Rutter, john@multiplan.co.uk)

//////// This section added by John Rutter, Multiplan Consultants Ltd, 2003-05-14 (V1.2.2)

// Automatic (default) or manual service startup
bool autoStart = true;

///////// End of added section (John Rutter, john@multiplan.co.uk)


//
// function prototypes
//

bool ParseArguments(int argc, char* argv[]);
void PrintUsage();
void FreeGlobals();
int IsServiceInstalled(bool *result);
int InstallService();
int UninstallService();
void WINAPI ServiceMain(DWORD dwArgc, LPTSTR* lpszArgv);
void WINAPI ServiceHandler(DWORD opcode);
DWORD WINAPI StartService(LPVOID lpParam);
DWORD WINAPI StopService(LPVOID lpParam);
DWORD WINAPI TimeoutStop(LPVOID lpParam);
LONG RegQueryValueExAllocate(HKEY hKey, LPCTSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE *lplpData, LPDWORD lpcbData);

//
// program entry point
//
int main(int argc, char* argv[])
{
	//Parse the arguments into the global variables.
	bool argumentsValid = ParseArguments(argc, argv);

	//See if we are trying to install a service.
	if (installing)
	{
		//Make sure the arguments were valid.
		if (!argumentsValid)
		{
			PrintUsage();
			FreeGlobals();
			return -1;
		}

		//Make sure we have all of the necessary arguments.
		if (serviceName == NULL || jvmLibrary == NULL || startClass == NULL)
		{
			PrintUsage();
			FreeGlobals();
			return -1;
		}

		//See if the service is already installed.
		bool ret=false;
		if (IsServiceInstalled(&ret))
		{
			LPVOID lpMsgBuf;
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL);
			printf("Error while checking to see if %s service is installed: %s\n", serviceName, lpMsgBuf); //// service name added by John Rutter (V1.2.2)
			LocalFree(lpMsgBuf);
			FreeGlobals();
			return -1;
		}
		if (ret)
		{
			printf("The %s service is already installed.\n", serviceName ); //// newline and service name added by John Rutter (V1.2.1 / V1.2.2)
			FreeGlobals();
			return -1;
		}

		//Install the service.
		if (InstallService())
		{
			LPVOID lpMsgBuf;
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL);
			printf("Error while installing the %s service: %s\n", serviceName, lpMsgBuf); //// service name added by John Rutter (V1.2.2)
			LocalFree(lpMsgBuf);
			FreeGlobals();
			return -1;
		}

		//// status report message text updated by John Rutter (V1.2.1 / V1.2.2) to add dependency and other info
		if (dependsOn == NULL)
		{
			printf("The %s %s service was successfully installed.\n",
				   serviceName, (autoStart ? "automatic" : "manual"));
		}
		else
		{
			printf("The %s %s service was successfully installed, depends on %s service.\n",
				   serviceName, (autoStart ? "automatic" : "manual"), dependsOn);
		}


		FreeGlobals();
		return 0;
	}

	//See if we are trying to uninstall a service.
	else if (uninstalling)
	{
		//Make sure the arguments were valid.
		if (!argumentsValid)
		{
			PrintUsage();
			FreeGlobals();
			return -1;
		}

		//Make sure we have all of the necessary arguments.
		if (serviceName == NULL)
		{
			PrintUsage();
			FreeGlobals();
			return -1;
		}

		//Install the service.
		if (UninstallService())
		{
			LPVOID lpMsgBuf;
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL);
			printf("Error while uninstalling the %s service: %s\n", serviceName, lpMsgBuf); //// service name added by John Rutter (V1.2.2)
			LocalFree(lpMsgBuf);
			FreeGlobals();
			return -1;
		}

		printf("The %s service was successfully uninstalled.\n", serviceName); //// newline and service name added by John Rutter (V1.2.1 / V1.2.2)
		FreeGlobals();
		return 0;
	}

	//Make sure the arguments were valid.
	if (!argumentsValid)
	{
		PrintUsage();
		FreeGlobals();
		return -1;
	}

	//Register the ServiceMain function.
	SERVICE_TABLE_ENTRY st[] =
	{
		{"JavaService", ServiceMain},
		{NULL, NULL}
	};

	//Start the service dispatcher, which will call the ServiceMain function.
	if (StartServiceCtrlDispatcher(st) == 0)
	{
		//Register An event source to log this error using JavaService, since we don't yet have the service name.
		HANDLE hJavaServiceEventSource = RegisterEventSource(NULL,  "JavaService"); //NOTE - conflicts with multiple Java Services? (J.R.)
		if (hJavaServiceEventSource != NULL)
		{
			//Log the error to the event log.
			LPTSTR messages[2];
			messages[0] = "StartServiceCtrlDispatcher";
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
			ReportEvent(hJavaServiceEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
			LocalFree(messages[1]);

			//Unregister the event source.
			DeregisterEventSource (hJavaServiceEventSource);
		}
		else
		{
			//There was an error, but we have no way to report it.
		}
	}

	FreeGlobals();
	return 0;
}

bool ParseArguments(int argc, char* argv[])
{
	//See if this is an install.
	if (argc >= 2 && strcmp(argv[1], "-install") == 0)
	{
		//Mark that we are installing the service.
		installing = true;

		//Make sure we have the correct number of parameters for installing.
		if (argc >= 6)
		{
			//Use the second parameter as the service name.
			serviceName = argv[2];

			//Use the third parameter as the JVM Library.
			jvmLibrary = argv[3];

			int nextArg = 4;

			//Count the number of jvm options specified.
			for (int i=nextArg; i<argc; i++)
			{
				if (strcmp(argv[i], "-start") == 0)
				{
					break;
				}
				else
				{
					jvmOptionCount++;
				}
			}

			//Get the jvm options, if there are any.
			if (jvmOptionCount > 0)
			{
				jvmOptions = new char*[jvmOptionCount];
				for (i=0; i<jvmOptionCount && nextArg<argc; i++, nextArg++)
				{
					jvmOptions[i] = argv[nextArg];
				}
			}

			//Skip over the next option, which must be -start, if there is one.
			if (nextArg < argc)
			{
				nextArg++;

				//Use the next argument as the class to start.
				if (nextArg < argc) startClass = argv[nextArg++];
				else return false;
			}

			//See if there is a method for the start class.
			if (nextArg < argc && strcmp(argv[nextArg], "-method") == 0)
			{
				//Skip the -method.
				nextArg++;

				//Use the next argument as the method to start.
				if (nextArg < argc) startMethod = argv[nextArg++];
			}

			//See if there are start parameters.
			if (nextArg < argc && strcmp(argv[nextArg], "-params") == 0)
			{
				//Skip the -params.
				nextArg++;

				//Count the number of parameters specified.
				for (i=nextArg; i<argc; i++)
				{
					if (strcmp(argv[i], "-stop") == 0 || strcmp(argv[i], "-out") == 0 || strcmp(argv[i], "-err") == 0)
					{
						break;
					}
					else
					{
						startParamCount++;
					}
				}

				//Get the start parameters, if there are any.
				if (startParamCount > 0)
				{
					startParams = new char*[startParamCount];
					for (i=0; i<startParamCount && nextArg<argc; i++, nextArg++)
					{
						startParams[i] = argv[nextArg];
					}
				}
				else
				{
					return false;
				}
			}

			//See if the nextr option is -stop.
			if (nextArg < argc && strcmp(argv[nextArg], "-stop") == 0)
			{
				//Skip the -stop.
				nextArg++;

				//Use the next argument as the class to stop.
				if (nextArg < argc) stopClass = argv[nextArg++];
				else return false;

				//See if there is a method for the stop class.
				if (nextArg < argc && strcmp(argv[nextArg], "-method") == 0)
				{
					//Skip the -method.
					nextArg++;

					//Use the next argument as the method to stop.
					if (nextArg < argc) stopMethod = argv[nextArg++];
					else return false;
				}

				//See if there are stop parameters.
				if (nextArg < argc && strcmp(argv[nextArg], "-params") == 0)
				{
					//Skip the -params.
					nextArg++;

					//Count the number of parameters specified.
					for (i=nextArg; i<argc; i++)
					{
						if (strcmp(argv[i], "-out") == 0 || strcmp(argv[i], "-err") == 0)
						{
							break;
						}
						else
						{
							stopParamCount++;
						}
					}


					//Get the start parameters, if there are any.
					if (stopParamCount > 0)
					{
						stopParams = new char*[stopParamCount];
						for (i=0; i<stopParamCount && nextArg<argc; i++, nextArg++)
						{
							stopParams[i] = argv[nextArg];
						}
					}
					else
					{
						return false;
					}
				}
			}

			//See if there is an out file.
			if (nextArg < argc && strcmp(argv[nextArg], "-out") == 0)
			{
				//Skip the -out.
				nextArg++;

				//Use the next argument as the out file.
				if (nextArg < argc) outFile = argv[nextArg++];
				else return false;
			}

			//See if there is an err file.
			if (nextArg < argc && strcmp(argv[nextArg], "-err") == 0)
			{
				//Skip the -err.
				nextArg++;

				//Use the next argument as the err file.
				if (nextArg < argc) errFile = argv[nextArg++];
				else return false;
			}

			//See if there is a current directory.
			if (nextArg < argc && strcmp(argv[nextArg], "-current") == 0)
			{
				//Skip the -current.
				nextArg++;

				//Use the next argument as the extended path
				if (nextArg < argc) currentDirectory = argv[nextArg++];
				else return false;
			}

			///////// This section added by Lars Johanson IVF 2001-02-26
			//See if there is any path additions
			if (nextArg < argc && strcmp(argv[nextArg], "-path") == 0)
			{
				//Skip the -path.
				nextArg++;

				//Use the next argument as the extended path
				if (nextArg < argc) pathExt = argv[nextArg++];
				else return false;
			}
			///////// End of added section

			//////// This section added by John Rutter, Multiplan Consultants Ltd, 2002-11-03 (V1.2.1)
			//See if there is any dependency addition
			if (nextArg < argc && strcmp(argv[nextArg], "-depends") == 0)
			{
				//Skip the -depends.
				nextArg++;

				//Use the next argument as a single service name dependency
				if (nextArg < argc)
				{
					dependsOn = argv[nextArg++];
				}
				else return false;
			}
			///////// End of added section (John Rutter, john@multiplan.co.uk)

			//////// This section added by John Rutter, Multiplan Consultants Ltd, 2003-05-14 (V1.2.2)
			//See if automatic or manual service startup is specified (defaults to auto mode)
			if (nextArg < argc && strcmp(argv[nextArg], "-auto") == 0)
			{
				//Skip the -auto parameter
				nextArg++;
				autoStart = true; // set the flag, although this is default anyway
			}
			else if (nextArg < argc && strcmp(argv[nextArg], "-manual") == 0)
			{
				//Skip the -manual parameter
				nextArg++;
				autoStart = false; // clear the flag, overriding the default
			}
			///////// End of added section (John Rutter, john@multiplan.co.uk)

			//If there are extra parameters, return false.
			if (nextArg < argc) return false;

			return true;
		}
		return false;
	}

	//See if this is an uninstall.
	else if (argc >= 2 && strcmp(argv[1], "-uninstall") == 0)
	{
		//Mark that we are uninstalling the service.
		uninstalling = true;

		//Make sure we have the correct number of parameters for uninstalling.
		if (argc == 3)
		{
			//Use the second parameter as the service name.
			serviceName = argv[2];

			return true;
		}
		return false;
	}
	
	//See if the service is starting up.
	else if (argc == 1)
	{
		return true;
	}

	return false;
}

void PrintUsage()
{
	printf("To install a service:\n");
	printf("\t-install service_name jvm_library (jvm_option)*\n");
	printf("\t-start start_class [-method start_method] [-params (start_parameter)+]\n");
	printf("\t[-stop start_class [-method stop_method] [-params (stop_parameter)+]]\n");
	printf("\t[-out out_log_file] [-err err_log_file]\n");
	printf("\t[-current current_dir]\n");
	printf("\t[-path extra_path]\n");	 // Modified by Lars Johanson IVF 2001-02-26
	printf("\t[-depends other_service]\n");	 // Added by John Rutter (V1.2.1)
	printf("\t[-auto | -manual]\n");	 // Added by John Rutter (V1.2.2)
	printf("\n");
	printf("To uninstall a service:\n");
	printf("\t-uninstall service_name\n");
	printf("\n");
	printf("service_name:\tThe name of the service.\n");
	printf("jvm_library:\tThe location of the JVM DLL under which to run the service.\n");
	printf("jvm_option:\tAn option to use when starting the JVM, such as:\n");
	printf("\t\t\"-Djava.class.path=c:\\classes\" or \"-Xmx128m\".\n");
	printf("start_class:\tThe class to load when starting the service.\n");
	printf("start_method:\tThe method to call in the start_class. default: main\n");
	printf("start_parameter:A parameter to pass in to the start_method.\n");
	printf("stop_class:\tThe class to load when stopping the service.\n");
	printf("stop_method:\tThe method to call in the stop_class. default: main\n");
	printf("stop_parameter:\tA parameter to pass in to the stop_method.\n");
	printf("out_log_file:\tA file to redirect System.out into.\n");
	printf("err_log_file:\tA file to redirect System.err into.\n");
	printf("current_dir:\tThe current working directory for the service.\n");
	printf("\t\tRelative paths will be relative to this directory.\n");
	printf("extra_path:\tPath additions, for native DLLs etc.\n");	 // Modified by Lars Johanson IVF 2001-02-26
	printf("other_service:\tSingle service name dependency, must start first.\n");	 // Added by John Rutter (V1.2.1)
	printf("auto / manual:\tStartup automatic (default) or manual mode.\n");	 // Added by John Rutter (V1.2.2)
}

void FreeGlobals()
{
	if (jvmOptions != NULL) delete[] jvmOptions;
	if (startParams != NULL) delete[] startParams;
	if (stopParams != NULL) delete[] stopParams;
}

int IsServiceInstalled(bool *ret)
{
	// Open the Service Control Manager
	SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if (hSCM == NULL)
	{
		return -1;
	}

	// Try to open the service
	SC_HANDLE hService = OpenService(hSCM, serviceName, SERVICE_QUERY_CONFIG);
	if (hService == NULL)
	{
		//If there was an error, see if it was because the service has not yet been installed.
		if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
		{
			//Return that the service has not yet been installed.
			*ret = false;
			CloseServiceHandle(hSCM);
			return 0;
		}
		else
		{
			//There was a genuine error.
			CloseServiceHandle(hSCM);
			return -1;
		}
	}

	//Return that the service is already installed.
	*ret = true;
	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);
	return 0;
}

int InstallService()
{
	// Open the Service Control Manager
	SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if (hSCM == NULL)
	{
		return -1;
	}

	// Get the executable file path
	char filePath[MAX_PATH];
	GetModuleFileName(NULL, filePath, sizeof(filePath));

	//// Following section modified to add dependency parameter by John Rutter (V1.2.1)

	// if service dependency specified, set up correct parameter type here
	char* dependency = NULL;
	if (dependsOn != NULL)
	{
		// set up dependency parameter with extra double null-terminator
		int dependencyLen = strlen(dependsOn) + 3;
		dependency = new char[dependencyLen];
		memset( dependency, 0, dependencyLen );
		strcpy( dependency, dependsOn );
	}

	//// Further modifications to include auto/manual option by John Rutter (V1.2.2)

	// Set up automatic or manual service startup mode

	DWORD dwStartType = autoStart ? SERVICE_AUTO_START : SERVICE_DEMAND_START;


	// Create the service
	SC_HANDLE hService = CreateService(hSCM,						// hSCManager
                                       serviceName,					// lpServiceName
                                       serviceName,					// lpDisplayName
                                       SERVICE_ALL_ACCESS,			// dwDesiredAccess
                                       SERVICE_WIN32_OWN_PROCESS,	// dwServiceType
                                       dwStartType,					// dwStartType - John Rutter (V1.2.2)
                                       SERVICE_ERROR_NORMAL,		// dwErrorControl
                                       filePath,					// lpBinaryPathName
                                       NULL,						// lpLoadOrderGroup
                                       NULL,						// lpdwTagId
                                       dependency,					// lpDependencies - John Rutter (V1.2.1)
                                       NULL,						// lpServiceStartName
                                       NULL);						// lpPassword

	// clean up any dependency parameter straight away - John Rutter (V1.2.1)
	if (dependency != NULL)
	{
		delete[] dependency;
	}

	//If the function failed, return an error.
	if (hService == NULL)
	{
		CloseServiceHandle(hSCM);
		return -1;
	}

	//Close the service handles.
	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);

	//Open the registry for this service's parameters.
	CHAR key[256];
	HKEY hKey = NULL;
	strcpy(key, "SYSTEM\\CurrentControlSet\\Services\\");
	strcat(key, serviceName);
	strcat(key, "\\Parameters");
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, key, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL) != ERROR_SUCCESS)
	{
		return -1;
	}

	//Set the JVM Library value.
	if (RegSetValueEx(hKey, "JVM Library", 0, REG_SZ,  (BYTE *)jvmLibrary, strlen(jvmLibrary)+1) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return -1;
	}

	//Set the jvm option count.
	if (RegSetValueEx(hKey, "JVM Option Count", 0, REG_DWORD,  (BYTE *)&jvmOptionCount, sizeof(DWORD)) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return -1;
	}

	//Set the jvm options.
	for (int i=0; i<jvmOptionCount; i++)
	{
		//Format the key name.
		char keyName[256];
		sprintf(keyName, "JVM Option Number %d", i);

		//Set the jvm option value.
		if (RegSetValueEx(hKey, keyName, 0, REG_SZ,  (BYTE *)jvmOptions[i], strlen(jvmOptions[i])+1) != ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			return -1;
		}
	}

	//Set the start class.
	if (RegSetValueEx(hKey, "Start Class", 0, REG_SZ,  (BYTE *)startClass, strlen(startClass)+1) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return -1;
	}

	//Set the start method.
	if (RegSetValueEx(hKey, "Start Method", 0, REG_SZ,  (BYTE *)startMethod, strlen(startMethod)+1) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return -1;
	}

	//Set the start parameter count.
	if (RegSetValueEx(hKey, "Start Param Count", 0, REG_DWORD,  (BYTE *)&startParamCount, sizeof(DWORD)) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return -1;
	}

	//Set the start parameters.
	for (i=0; i<startParamCount; i++)
	{
		//Format the key name.
		char keyName[256];
		sprintf(keyName, "Start Param Number %d", i);

		//Set the parameter.
		if (RegSetValueEx(hKey, keyName, 0, REG_SZ,  (BYTE *)startParams[i], strlen(startParams[i])+1) != ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			return -1;
		}
	}

	if (stopClass != NULL)
	{
		//Set the stop class.
		if (RegSetValueEx(hKey, "Stop Class", 0, REG_SZ,  (BYTE *)stopClass, strlen(stopClass)+1) != ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			return -1;
		}

		//Set the stop method.
		if (RegSetValueEx(hKey, "Stop Method", 0, REG_SZ,  (BYTE *)stopMethod, strlen(stopMethod)+1) != ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			return -1;
		}

		//Set the stop parameter count.
		if (RegSetValueEx(hKey, "Stop Param Count", 0, REG_DWORD,  (BYTE *)&stopParamCount, sizeof(DWORD)) != ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			return -1;
		}

		//Set the stop parameters.
		for (i=0; i<stopParamCount; i++)
		{
			//Format the key name.
			char keyName[256];
			sprintf(keyName, "Stop Param Number %d", i);

			//Set the parameter.
			if (RegSetValueEx(hKey, keyName, 0, REG_SZ,  (BYTE *)stopParams[i], strlen(stopParams[i])+1) != ERROR_SUCCESS)
			{
				RegCloseKey(hKey);
				return -1;
			}
		}
	}

	if (outFile != NULL)
	{
		//Set the out file.
		if (RegSetValueEx(hKey, "System.out File", 0, REG_SZ,  (BYTE *)outFile, strlen(outFile)+1) != ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			return -1;
		}
	}

	if (errFile != NULL)
	{
		//Set the err file.
		if (RegSetValueEx(hKey, "System.err File", 0, REG_SZ,  (BYTE *)errFile, strlen(errFile)+1) != ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			return -1;
		}
	}

	if (currentDirectory != NULL)
	{
		//Set the err file.
		if (RegSetValueEx(hKey, "Current Directory", 0, REG_SZ,  (BYTE *)currentDirectory, strlen(currentDirectory)+1) != ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			return -1;
		}
	}


	///////// This section added by Lars Johanson IVF 2001-02-26
	if (pathExt != NULL)
	{
		//Set the path extension
		if (RegSetValueEx(hKey, "Path", 0, REG_SZ,  (BYTE *)pathExt, strlen(pathExt)+1) != ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			return -1;
		}
	}
	///////// End of added section

	//Close the registry.
	RegCloseKey(hKey);

	//Make a registry key to support logging messages for JavaService before we know the service name.
	strcpy(key, "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\JavaService");
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, key, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL) != ERROR_SUCCESS)
	{
		return -1;
	}

	// Add the Event ID message-file name to the 'EventMessageFile' subkey.
	if (RegSetValueEx(hKey, "EventMessageFile", 0, REG_EXPAND_SZ,  (BYTE*)filePath, strlen(filePath)+1) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return -1;
	}

	// Set the supported types flags.
	DWORD data = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
	if (RegSetValueEx(hKey, "TypesSupported", 0, REG_DWORD, (CONST BYTE*)&data, sizeof(DWORD)) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return -1;
	}

	//Close the registry.
	RegCloseKey(hKey);

	//Make a registry key to support logging messages using the service name.
	strcpy(key, "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\");
	strcat(key, serviceName);
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, key, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hKey, NULL) != ERROR_SUCCESS)
	{
		return -1;
	}

	// Add the Event ID message-file name to the 'EventMessageFile' subkey.
	if (RegSetValueEx(hKey, "EventMessageFile", 0, REG_EXPAND_SZ,  (BYTE*)filePath, strlen(filePath)+1) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return -1;
	}

	// Set the supported types flags.
	data = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
	if (RegSetValueEx(hKey, "TypesSupported", 0, REG_DWORD, (CONST BYTE*)&data, sizeof(DWORD)) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return -1;
	}

	//Close the registry.
	RegCloseKey(hKey);

	return 0;
}

int UninstallService()
{
	// Open the Service Control Manager
	SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hSCM == NULL)
	{
		return -1;
	}

	SC_HANDLE hService = OpenService(hSCM, serviceName, DELETE);
	if (hService == NULL)
	{
		CloseServiceHandle(hSCM);
		return -1;
	}

	if (DeleteService(hService) == FALSE)
	{
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCM);
		return -1;
	}

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);
	return 0;
}

SERVICE_STATUS_HANDLE hServiceStatus;
SERVICE_STATUS status;
HANDLE hEventSource;
HANDLE hWaitForStart = NULL;
HANDLE hWaitForStop = NULL;
bool serviceStartedSuccessfully = false;

void WINAPI ServiceMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
	//Set the service name.
	serviceName = lpszArgv[0];

	//Create the event source.
	hEventSource = RegisterEventSource(NULL,  serviceName);
	if (hEventSource == NULL)
	{
		//There was an error, but we have no way to report it.
		return;
	}

	//Log that we have started.
	{
		LPTSTR messages[1];
		messages[0] = serviceName;
		ReportEvent(hEventSource, EVENTLOG_INFORMATION_TYPE, 0, EVENT_SERVICE_STARTED, NULL, 1, 0, (const char **)messages, NULL);
	}

	//Register the control request handler
	hServiceStatus = RegisterServiceCtrlHandler(serviceName, ServiceHandler);

	//Initialize the status structure.
	status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	status.dwCurrentState = SERVICE_STOPPED;
	status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	status.dwWin32ExitCode = 0;
	status.dwServiceSpecificExitCode = 0;
	status.dwCheckPoint = 0;
	status.dwWaitHint = 0;

	//Mark the service as running.
	status.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus(hServiceStatus, &status);

	//Start the service thread.
	DWORD id;
    HANDLE hThread; 
    hThread = CreateThread(NULL, 0, StartService, NULL, 0, &id);
	if (hThread == NULL)
	{
		//Log the error to the event log.
		LPTSTR messages[2];
		messages[0] = "CreateThread";
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
		ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
		LocalFree(messages[1]);
	}
	else
	{
		//Create the events to wait on.
		hWaitForStart = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (hWaitForStart == NULL)
		{
			LPTSTR messages[2];
			messages[0] = "CreateEvent";
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
			ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
			LocalFree(messages[1]);
		}
		else
		{
			hWaitForStop = CreateEvent(NULL, FALSE, FALSE, NULL);
			if (hWaitForStop == NULL)
			{
				LPTSTR messages[2];
				messages[0] = "CreateEvent";
				FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
				ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
				LocalFree(messages[1]);
			}
			else
			{
				HANDLE handles[2];
				handles[0] = hWaitForStart;
				handles[1] = hWaitForStop;

				//Wait until we are supposed to stop.
				WaitForMultipleObjects(2, handles, TRUE, INFINITE);

				//Close the event.
				CloseHandle(hWaitForStop);
			}
			CloseHandle(hWaitForStart);
		}
	}

	//Log that we have stopped.
	if (serviceStartedSuccessfully)
	{
		LPTSTR messages[1];
		messages[0] = serviceName;
		ReportEvent(hEventSource, EVENTLOG_INFORMATION_TYPE, 0, EVENT_SERVICE_STOPPED, NULL, 1, 0, (const char **)messages, NULL);
	}
	else
	{
		LPTSTR messages[1];
		messages[0] = serviceName;
		ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_START_FAILED, NULL, 1, 0, (const char **)messages, NULL);
	}

	//Tell the service manager we are stopped.
  	status.dwCurrentState = SERVICE_STOPPED;
	status.dwWaitHint = 0;
	SetServiceStatus(hServiceStatus, &status);

	//Deregister the event source.
	DeregisterEventSource (hEventSource);
}

void WINAPI ServiceHandler(DWORD opcode)
{
	switch (opcode)
	{
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
		//Tell the service mamanger that stop is pending.
		status.dwCurrentState = SERVICE_STOP_PENDING;
		status.dwWaitHint = 35000;
		SetServiceStatus(hServiceStatus, &status);

		//Start the stop method in another thread.
		DWORD stopId;
		HANDLE stopThread; 
		stopThread = CreateThread(NULL, 0, StopService, NULL, 0, &stopId);
		if (stopThread == NULL)
		{
			//Log the error to the event log.
			LPTSTR messages[2];
			messages[0] = "CreateThread";
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
			ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
			LocalFree(messages[1]);
		}

		//Start a thread to stop the service in a number of seconds, if it has not stopped yet.
		DWORD timeoutId;
		HANDLE timeoutThread; 
		timeoutThread = CreateThread(NULL, 0, TimeoutStop, NULL, 0, &timeoutId);
		if (timeoutThread == NULL)
		{
			//Log the error to the event log.
			LPTSTR messages[2];
			messages[0] = "CreateThread";
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
			ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
			LocalFree(messages[1]);

			//If this call failed, stop the service to be safe.
			if (hWaitForStart != NULL && hWaitForStop != NULL)
			{
				SetEvent(hWaitForStart);
				SetEvent(hWaitForStop);
			}
		}

		break;
	default:
		SetServiceStatus(hServiceStatus, &status);
	}
}

DWORD WINAPI StartService(LPVOID lpParam)
{
	//Open the registry for this service's parameters.
	LONG regRet;
	char key[256];
	HKEY hKey = NULL;
	strcpy(key, "SYSTEM\\CurrentControlSet\\Services\\");
	strcat(key, serviceName);
	strcat(key, "\\Parameters");
	if ((regRet=RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_QUERY_VALUE, &hKey)) != ERROR_SUCCESS)
	{
		LPTSTR messages[2];
		messages[0] = "RegOpenKeyEx";
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, regRet, MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
		ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
		LocalFree(messages[1]);
		if (hWaitForStart != NULL && hWaitForStop != NULL)
		{
			SetEvent(hWaitForStart);
			SetEvent(hWaitForStop);
		}
		return -1;
	}

	//Get the JVM Library value.
	char *regJvmLibrary;
	DWORD regJvmLibraryLength = 0;
	if ((regRet=RegQueryValueExAllocate(hKey, "JVM Library", NULL, NULL,  (BYTE **)&regJvmLibrary, &regJvmLibraryLength)) != ERROR_SUCCESS)
	{
		LPTSTR messages[2];
		messages[0] = "RegQueryValueEx";
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, regRet, MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
		ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
		LocalFree(messages[1]);
		if (hWaitForStart != NULL && hWaitForStop != NULL)
		{
			SetEvent(hWaitForStart);
			SetEvent(hWaitForStop);
		}
		return -1;
	}

	//Get the jvm option count.
	int regJvmOptionCount;
	DWORD regJvmOptionCountLength = 4;
	if ((regRet=RegQueryValueEx(hKey, "JVM Option Count", NULL, NULL,  (BYTE *)&regJvmOptionCount, &regJvmOptionCountLength)) != ERROR_SUCCESS)
	{
		LPTSTR messages[2];
		messages[0] = "RegQueryValueEx";
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, regRet, MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
		ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
		LocalFree(messages[1]);
		if (hWaitForStart != NULL && hWaitForStop != NULL)
		{
			SetEvent(hWaitForStart);
			SetEvent(hWaitForStop);
		}
		return -1;
	}

	//Allocate an array for the options.
	char **regJvmOptions;
	if (regJvmOptionCount > 0)
	{
		regJvmOptions = new char*[regJvmOptionCount];
	}

	//Get the jvm options.
	for (int i=0; i<regJvmOptionCount; i++)
	{
		//Format the key name.
		char keyName[256];
		sprintf(keyName, "JVM Option Number %d", i);

		//Get the jvm option.
		regJvmOptions[i];
		DWORD regJvmOptionsLength = 0;
		if ((regRet=RegQueryValueExAllocate(hKey, keyName, NULL, NULL,  (BYTE **)&regJvmOptions[i], &regJvmOptionsLength)) != ERROR_SUCCESS)
		{
			LPTSTR messages[2];
			messages[0] = "RegQueryValueEx";
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, regRet, MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
			ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
			LocalFree(messages[1]);
			if (hWaitForStart != NULL && hWaitForStop != NULL)
			{
				SetEvent(hWaitForStart);
				SetEvent(hWaitForStop);
			}
			return -1;
		}
	}

	//Get the Start Class.
	char *regStartClass;
	DWORD regStartClassLength = 0;
	if ((regRet=RegQueryValueExAllocate(hKey, "Start Class", NULL, NULL,  (BYTE **)&regStartClass, &regStartClassLength)) != ERROR_SUCCESS)
	{
		LPTSTR messages[2];
		messages[0] = "RegQueryValueEx";
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, regRet, MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
		ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
		LocalFree(messages[1]);
		if (hWaitForStart != NULL && hWaitForStop != NULL)
		{
			SetEvent(hWaitForStart);
			SetEvent(hWaitForStop);
		}
		return -1;
	}
	//Replace periods with slashes.
	for (unsigned int p=0; p<strlen(regStartClass); p++)
	{
		if (regStartClass[p] == '.') regStartClass[p] = '/';
	}

	//Get the Start Method.
	char *regStartMethod;
	DWORD regStartMethodLength = 0;
	if ((regRet=RegQueryValueExAllocate(hKey, "Start Method", NULL, NULL,  (BYTE **)&regStartMethod, &regStartMethodLength)) != ERROR_SUCCESS)
	{
		LPTSTR messages[2];
		messages[0] = "RegQueryValueEx";
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, regRet, MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
		ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
		LocalFree(messages[1]);
		if (hWaitForStart != NULL && hWaitForStop != NULL)
		{
			SetEvent(hWaitForStart);
			SetEvent(hWaitForStop);
		}
		return -1;
	}

	//Get the Start Param Count.
	int regStartParamCount=0;
	DWORD regStartParamCountLength = 4;
	if ((regRet=RegQueryValueEx(hKey, "Start Param Count", NULL, NULL,  (BYTE *)&regStartParamCount, &regStartParamCountLength)) != ERROR_SUCCESS)
	{
		LPTSTR messages[2];
		messages[0] = "RegQueryValueEx";
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, regRet, MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
		ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
		LocalFree(messages[1]);
		if (hWaitForStart != NULL && hWaitForStop != NULL)
		{
			SetEvent(hWaitForStart);
			SetEvent(hWaitForStop);
		}
		return -1;
	}

	//Allocate an array for the parameters
	char **regStartParams = NULL;
	if (regStartParamCount > 0)
	{
		regStartParams = new char*[regStartParamCount];
	}

	//Get the start parameters arguments.
	for (i=0; i<regStartParamCount; i++)
	{
		//Format the key name.
		char keyName[256];
		sprintf(keyName, "Start Param Number %d", i);

		//Get the service argument
		regStartParams[i];
		DWORD regStartParamsLength = 0;
		if ((regRet=RegQueryValueExAllocate(hKey, keyName, NULL, NULL,  (BYTE **)&regStartParams[i], &regStartParamsLength)) != ERROR_SUCCESS)
		{
			LPTSTR messages[2];
			messages[0] = "RegQueryValueEx";
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, regRet, MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
			ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
			LocalFree(messages[1]);
			if (hWaitForStart != NULL && hWaitForStop != NULL)
			{
				SetEvent(hWaitForStart);
				SetEvent(hWaitForStop);
			}
			return -1;
		}
	}

	//Get the System.out File.
	char *regOutFileValue = NULL;
	char *regOutFile;
	DWORD regOutFileLength = 0;
	if ((regRet=RegQueryValueExAllocate(hKey, "System.out File", NULL, NULL,  (BYTE **)&regOutFile, &regOutFileLength)) != ERROR_SUCCESS)
	{
		//No value specified.
	}
	else
	{
		regOutFileValue = regOutFile;
	}

	//Get the System.err File.
	char *regErrFileValue = NULL;
	char *regErrFile;
	DWORD regErrFileLength = 0;
	if ((regRet=RegQueryValueExAllocate(hKey, "System.err File", NULL, NULL,  (BYTE **)&regErrFile, &regErrFileLength)) != ERROR_SUCCESS)
	{
		//No value specified.
	}
	else
	{
		regErrFileValue = regErrFile;
	}

	//Get the current directory.
	char *regCurrentDirectory;
	DWORD regCurrentDirectoryLength = 0;
	if ((regRet=RegQueryValueExAllocate(hKey, "Current Directory", NULL, NULL,  (BYTE **)&regCurrentDirectory, &regCurrentDirectoryLength)) != ERROR_SUCCESS)
	{
		//No value specified.
	}
	else
	{
		if (!SetCurrentDirectory(regCurrentDirectory))
		{
			LPTSTR messages[2];
			messages[0] = "SetCurrentDirectory";
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
			ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
			LocalFree(messages[1]);
			if (hWaitForStart != NULL && hWaitForStop != NULL)
			{
				SetEvent(hWaitForStart);
				SetEvent(hWaitForStop);
			}
			return -1;
		}
	}

	///////// This section added by Lars Johanson IVF 2001-02-26 modified by Elijah Roberts 2001-04-12.
	//Get the path extension.
	char *regPathExt = NULL; ///// Modified to avoid erroneous delete below - John Rutter (V1.2.1)
	DWORD regPathExtLength = 0;
	if ((regRet=RegQueryValueExAllocate(hKey, "Path", NULL, NULL,  (BYTE **)&regPathExt, &regPathExtLength)) != ERROR_SUCCESS)
	{
		//No value specified.
	}
	else
	{
		char *currentPath = NULL;
		DWORD currentPathLength = 0;

		//Get the length of the current path.
		currentPathLength = GetEnvironmentVariable("PATH", currentPath, currentPathLength);

		//Allocate a buffer big enough for the current path and the new path.
		if ((currentPath=(char *)malloc(currentPathLength + strlen(regPathExt) + 2)) == NULL)
		{
			LPTSTR messages[2];
			messages[0] = "malloc";
			messages[1] = "Could not allocate memory.";
			ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
			if (hWaitForStart != NULL && hWaitForStop != NULL)
			{
				SetEvent(hWaitForStart);
				SetEvent(hWaitForStop);
			}
			return -1;
		}

		//Zero out the buffer so string functions will work.
		memset(currentPath, NULL, currentPathLength + strlen(regPathExt) + 2);

		//Copy the current path into the buffer.
		if (GetEnvironmentVariable("PATH", currentPath, currentPathLength) > currentPathLength)
		{
			LPTSTR messages[2];
			messages[0] = "GetEnvironmentVariable";
			messages[1] = "Path length has changed between calls.";
			ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
			if (hWaitForStart != NULL && hWaitForStop != NULL)
			{
				SetEvent(hWaitForStart);
				SetEvent(hWaitForStop);
			}
			return -1;
		}

		//If there was a path already and there is a new path, append a seperator.
		if (strlen(currentPath) > 0 && strlen(regPathExt) > 0) strcat(currentPath, ";");

		//Append the new path.
		strcat(currentPath, regPathExt);

		//Set the new path.
		if (SetEnvironmentVariable("PATH", currentPath) == 0)
		{
			//Log the error to the event log.
			LPTSTR messages[2];
			messages[0] = "SetEnvironmentVariable";
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
			ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
			LocalFree(messages[1]);
			if (hWaitForStart != NULL && hWaitForStop != NULL)
			{
				SetEvent(hWaitForStart);
				SetEvent(hWaitForStop);
			}
			return -1;
		}
	}
	///////// End of added section

	//Close the registry.
	RegCloseKey(hKey);

	//Run the java service.
	serviceStartedSuccessfully = true;
	serviceStartedSuccessfully = StartJavaService(hEventSource, regJvmLibrary, regJvmOptionCount, regJvmOptions, regStartClass, regStartMethod, regStartParamCount, regStartParams, regOutFileValue, regErrFileValue);

	//Free the buffers.
	if (regJvmLibrary != NULL) delete[] regJvmLibrary;
	if (regStartClass != NULL) delete[] regStartClass;
	if (regStartMethod != NULL) delete[] regStartMethod;
	if (regOutFile != NULL) delete[] regOutFile;
	if (regErrFile != NULL) delete[] regErrFile;
	if (regPathExt != NULL) delete[] regPathExt;

	if (regJvmOptionCount > 0)
	{
		for (i=0; i<regJvmOptionCount; i++)
		{
			delete[] regJvmOptions[i];
		}
		delete[] regJvmOptions;
	}

	if (regStartParamCount > 0)
	{
		for (i=0; i<regStartParamCount; i++)
		{
			delete[] regStartParams[i];
		}
		delete[] regStartParams;
	}

	//Tell the main thread that the service is no longer running.
	if (serviceStartedSuccessfully)
	{
		if (hWaitForStart != NULL)
		{
			SetEvent(hWaitForStart);
		}
	}
	else
	{
		if (hWaitForStart != NULL && hWaitForStop != NULL)
		{
			SetEvent(hWaitForStart);
			SetEvent(hWaitForStop);
		}
	}

	return 0;
}

DWORD WINAPI StopService(LPVOID lpParam)
{
	//Open the registry for this service's parameters.
	LONG regRet;
	char key[256];
	HKEY hKey = NULL;
	strcpy(key, "SYSTEM\\CurrentControlSet\\Services\\");
	strcat(key, serviceName);
	strcat(key, "\\Parameters");
	if ((regRet=RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_QUERY_VALUE, &hKey)) != ERROR_SUCCESS)
	{
		LPTSTR messages[2];
		messages[0] = "RegOpenKeyEx";
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, regRet, MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
		ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
		LocalFree(messages[1]);
		if (hWaitForStart != NULL && hWaitForStop != NULL)
		{
			SetEvent(hWaitForStart);
			SetEvent(hWaitForStop);
		}
		return -1;
	}

	//Get the Stop Class.
	char *regStopClass;
	DWORD regStopClassLength = 0;
	if ((regRet=RegQueryValueExAllocate(hKey, "Stop Class", NULL, NULL,  (BYTE **)&regStopClass, &regStopClassLength)) == ERROR_SUCCESS)
	{
		//Replace periods with slashes.
		for (unsigned int p=0; p<strlen(regStopClass); p++)
		{
			if (regStopClass[p] == '.') regStopClass[p] = '/';
		}

		//Get the Stop Method.
		char *regStopMethod;
		DWORD regStopMethodLength = 0;
		if ((regRet=RegQueryValueExAllocate(hKey, "Stop Method", NULL, NULL,  (BYTE **)&regStopMethod, &regStopMethodLength)) != ERROR_SUCCESS)
		{
			LPTSTR messages[2];
			messages[0] = "RegQueryValueEx";
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, regRet, MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
			ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
			LocalFree(messages[1]);
			if (hWaitForStart != NULL && hWaitForStop != NULL)
			{
				SetEvent(hWaitForStart);
				SetEvent(hWaitForStop);
			}
			return -1;
		}

		//Get the Stop Param Count.
		int regStopParamCount=0;
		DWORD regStopParamCountLength = 4;
		if ((regRet=RegQueryValueEx(hKey, "Stop Param Count", NULL, NULL,  (BYTE *)&regStopParamCount, &regStopParamCountLength)) != ERROR_SUCCESS)
		{
			LPTSTR messages[2];
			messages[0] = "RegQueryValueEx";
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, regRet, MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
			ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
			LocalFree(messages[1]);
			if (hWaitForStart != NULL && hWaitForStop != NULL)
			{
				SetEvent(hWaitForStart);
				SetEvent(hWaitForStop);
			}
			return -1;
		}

		//Allocate an array for the parameters
		char **regStopParams = NULL;
		if (regStopParamCount > 0)
		{
			regStopParams = new char*[regStopParamCount];
		}

		//Get the stop parameters arguments.
		for (int i=0; i<regStopParamCount; i++)
		{
			//Format the key name.
			char keyName[256];
			sprintf(keyName, "Stop Param Number %d", i);

			//Get the service argument
			regStopParams[i];
			DWORD regStopParamsLength = 0;
			if ((regRet=RegQueryValueExAllocate(hKey, keyName, NULL, NULL,  (BYTE **)&regStopParams[i], &regStopParamsLength)) != ERROR_SUCCESS)
			{
				LPTSTR messages[2];
				messages[0] = "RegQueryValueEx";
				FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS, NULL, regRet, MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), (LPTSTR)&messages[1], 0, NULL);
				ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_FUNCTION_FAILED, NULL, 2, 0, (const char **)messages, NULL);
				LocalFree(messages[1]);
				if (hWaitForStart != NULL && hWaitForStop != NULL)
				{
					SetEvent(hWaitForStart);
					SetEvent(hWaitForStop);
				}
				return -1;
			}
		}

		//Close the registry.
		RegCloseKey(hKey);

		//Stop the java service.
		if (StopJavaService(hEventSource,  regStopClass, regStopMethod, regStopParamCount, regStopParams))
		{
			if (hWaitForStop != NULL)
			{
				SetEvent(hWaitForStop);
			}
		}
		else
		{
			//If there was an error running the stop method, stop the service.
			LPTSTR messages[1];
			messages[0] = serviceName;
			ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_STOP_FAILED, NULL, 1, 0, (const char **)messages, NULL);
			if (hWaitForStart != NULL && hWaitForStop != NULL)
			{
				SetEvent(hWaitForStart);
				SetEvent(hWaitForStop);
			}
		}

		//Free any memory used for the parameters.
		if (regStopClass != NULL) delete[] regStopClass;
		if (regStopMethod != NULL) delete[] regStopMethod;
		if (regStopParamCount > 0)
		{
			for (i=0; i<regStopParamCount; i++)
			{
				delete[] regStopParams[i];
			}
			delete[] regStopParams;
		}
	}
	else
	{
		//There is no stop class, tell the service to stop.
		if (hWaitForStart != NULL && hWaitForStop != NULL)
		{
			SetEvent(hWaitForStart);
			SetEvent(hWaitForStop);
		}
	}

	return 0;
}

DWORD WINAPI TimeoutStop(LPVOID lpParam)
{
	//Sleep for required number of seconds.
	Sleep(SHUTDOWN_TIMEOUT_MSECS); // modified to use constant definition here - John Rutter (V1.2.1)

	//Tell the main thread that the service is no longer running.
	LPTSTR messages[1];
	messages[0] = serviceName;
	ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_STOP_TIMEDOUT, NULL, 1, 0, (const char **)messages, NULL);
	if (hWaitForStart != NULL && hWaitForStop != NULL)
	{
		SetEvent(hWaitForStart);
		SetEvent(hWaitForStop);
	}

	return 0;
}

void ExitHandler(int code)
{
	//Log the code to the event log.
	char message[256];
	sprintf(message, "The Java Virtual Machine has exited with a code of %d, the service is being stopped.", code);
	LPTSTR messages[1];
	messages[0] = message;
	if (code == 0)
	{
		ReportEvent(hEventSource, EVENTLOG_INFORMATION_TYPE, 0, EVENT_GENERIC_INFORMATION, NULL, 1, 0, (const char **)messages, NULL);
	}
	else
	{
		ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, EVENT_GENERIC_ERROR, NULL, 1, 0, (const char **)messages, NULL);
	}

	//Tell the main thread that the service is no longer running.
	if (hWaitForStart != NULL)
	{
		SetEvent(hWaitForStart);
	}
	if (hWaitForStop != NULL)
	{
		SetEvent(hWaitForStop);
	}
	Sleep(EXIT_HANDLER_TIMEOUT_MSECS); // modified to use constant definition here - John Rutter (V1.2.1)
}

LONG RegQueryValueExAllocate(HKEY hKey, LPCTSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE *lplpData, LPDWORD lpcbData)
{
	LONG ret;

	//See how long the value is.
	if ((ret=RegQueryValueEx(hKey, lpValueName, lpReserved, lpType, NULL, lpcbData)) != ERROR_SUCCESS)
	{
		return ret;
	}

	//Allocate a buffer for the value.
	*lplpData = new BYTE[*lpcbData];
	if (*lplpData == NULL)
	{
		return ERROR_MORE_DATA;
	}

	//Get the value.
	if ((ret=RegQueryValueEx(hKey, lpValueName, lpReserved, lpType, *lplpData, lpcbData)) != ERROR_SUCCESS)
	{
		return ret;
	}

	return ERROR_SUCCESS;
}