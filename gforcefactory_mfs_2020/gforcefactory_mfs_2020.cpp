//------------------------------------------------------------------------------
//
//  SimConnect Data Request Sample
//  
//	Description:
//				After a flight has loaded, request the lat/lon/alt of the user 
//				aircraft
//------------------------------------------------------------------------------

#pragma comment(lib, "ws2_32.lib")
#include <WS2tcpip.h>
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <strsafe.h>
#include <stdint.h>
#include <math.h>

#include "SimConnect.h"
#pragma comment(lib, "SimConnect.lib")
#include "..\gforcefactory_api.h"

double PCFreq = 0.0;
__int64 CounterStart = 0;

void StartCounter()
{
    LARGE_INTEGER li;
    QueryPerformanceFrequency(&li);
    PCFreq = double(li.QuadPart) / 1000000.0;
    QueryPerformanceCounter(&li);
    CounterStart = li.QuadPart;
}
double GetCounter()
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return double(li.QuadPart - CounterStart) / PCFreq;
}

int     quit = 0;
HANDLE  hSimConnect = NULL;

struct Struct1
{
    double  vel_x;
    double  vel_y;
    double  vel_z;
    double  acc_x;
    double  acc_y;
    double  acc_z;
    double  orient_x;
    double  orient_y;
    double  orient_z;
    double  rotspd_x;
    double  rotspd_y;
    double  rotspd_z;
};

 enum EVENT_ID{
    EVENT_SIM_START,
};

 enum DATA_DEFINE_ID {
    DEFINITION_1,
};

 enum DATA_REQUEST_ID {
    REQUEST_1,
};
 bool is_first_msg = true;


void CALLBACK MyDispatchProcRD(SIMCONNECT_RECV* pData, DWORD cbData, void *pContext)
{
    HRESULT hr;
    
    switch(pData->dwID)
    {
        case SIMCONNECT_RECV_ID_EVENT:
        {
            SIMCONNECT_RECV_EVENT *evt = (SIMCONNECT_RECV_EVENT*)pData;

            switch(evt->uEventID)
            {
                case EVENT_SIM_START:
                    
					// Now the sim is running, request information on the user aircraft
                    hr = SimConnect_RequestDataOnSimObjectType(hSimConnect, REQUEST_1, DEFINITION_1, 0, SIMCONNECT_SIMOBJECT_TYPE_USER);
                    
					break;

                default:
                   break;
            }
            break;
        }

        case SIMCONNECT_RECV_ID_SIMOBJECT_DATA_BYTYPE:
        {
            SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE *pObjData = (SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE*)pData;
            
            switch(pObjData->dwRequestID)
            {
                case REQUEST_1:
                {
                    DWORD ObjectID = pObjData->dwObjectID;
                    Struct1 *pS = (Struct1*)&pObjData->dwData;
                    hr = SimConnect_RequestDataOnSimObjectType(hSimConnect, REQUEST_1, DEFINITION_1, 0, SIMCONNECT_SIMOBJECT_TYPE_USER);

                    // lets write it to the platform
                    edge_motion msg;
                    msg.header = 'G';
                    msg.packet_version = 0;
                    msg.motion_type = 'M';
                    msg.type = 4;
                    if (is_first_msg) {
                        msg.timestamp = 0;
                        is_first_msg = false;
                    }
                    else {
                        uint64_t time = (uint64_t)GetCounter();
                        if(time > 10000000){
                            StartCounter();
                            time = 0;
                        }
                        msg.timestamp = (uint32_t)time;
                    }
//                    msg.rx = (float)(pS->rot_x) * 0.1f;  // is pitch
//                    msg.ry = (float)(pS->rot_y) * 0.1f; // yaw
//                    msg.rz = (float)(-pS->rot_z) * 0.1f; // bank 

//                    msg.tx = (float)(pS->acc_y);// -sin(-aircraft_pitch) * hang_scale);
//                    msg.ty = (float)(pS->acc_x)*3.0f;// +sin(aircraft_bank) * hang_scale);
//                    msg.tz = (float)(pS->acc_z);

                    float vel[3], acc[3], orient[3], rotspd[3];

                    vel[0] = (float) (pS->vel_x);
                    vel[1] = (float) (pS->vel_y);
                    vel[2] = (float)-(pS->vel_z);

                    acc[0] = (float)(pS->acc_x);
                    acc[1] = (float)-(pS->acc_y);
                    acc[2] = (float)(pS->acc_z);

                    orient[0] = (float)-(pS->orient_x);
                    orient[1] = (float)(pS->orient_y);
                    orient[2] = (float)(pS->orient_z);

                    rotspd[0] = (float)-(pS->rotspd_x);
                    rotspd[1] = (float)(pS->rotspd_y);
                    rotspd[2] = (float)(pS->rotspd_z);

                    float gx, gy;
                    #define gForceFactor 9.81

                    gx = -sin(orient[1]) * gForceFactor;
                    gy =  sin(orient[0]) * gForceFactor;

                    msg.tx = acc[0] + gx;
                    msg.ty = acc[1] + 2*gy;
                    msg.tz = acc[2];

                    msg.rx = rotspd[0];
                    msg.ry = rotspd[1];
                    msg.rz = rotspd[2];

                    send_edge_motion_message(msg);
                    printf("%d %0.1f %0.1f %0.1f %0.1f %0.1f %0.1f\n", msg.timestamp, msg.tx, msg.ty, msg.tz, msg.rx, msg.ry, msg.rz);

//                    printf("%d acc_x:%0.1f acc_y:%.1f acc_z:%.1f rot_x:%.1f rot_y:%.1f rot_z:%.1f\n", msg.timestamp,  pS->acc_x, pS->acc_y, pS->acc_z, pS->rot_x, pS->rot_y, pS->rot_z);


                    break;
                }

                default:
                   break;
            }
            break;
        }


        case SIMCONNECT_RECV_ID_QUIT:
        {
            quit = 1;
            break;
        }

        default:
            printf("\nReceived:%d",pData->dwID);
            break;
    }
}

void testDataRequest()
{
    HRESULT hr;

    if (SUCCEEDED(SimConnect_Open(&hSimConnect, "Request Data", NULL, 0, 0, 0)))
    {
        printf("\nConnected to Flight Simulator!");   

        // Set up the data definition, but do not yet do anything with it

//        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1, "acceleration Body X", "meters per second");
//        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1, "acceleration Body Y", "meters per second");
//        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1, "acceleration Body Z", "meters per second");
//        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1, "Rotation velocity Body X", "radians per second");
//        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1, "Rotation velocity Body Y", "radians per second");
//        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1, "Rotation velocity Body Z", "radians per second");

        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1, "VELOCITY BODY Z", "meters per second squared");
        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1, "VELOCITY BODY X", "meters per second squared");
        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1, "VELOCITY BODY Y", "meters per second squared");

        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1, "ACCELERATION BODY Z", "meters per second squared");
        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1, "ACCELERATION BODY X", "meters per second squared");
        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1, "ACCELERATION BODY Y", "meters per second squared");

        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1, "PLANE BANK DEGREES", "radians");
        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1, "PLANE PITCH DEGREES", "radians");
        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1, "PLANE HEADING DEGREES TRUE", "radians");

        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1, "ROTATION VELOCITY BODY Z", "radians per second");
        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1, "ROTATION VELOCITY BODY X", "radians per second");
        hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1, "ROTATION VELOCITY BODY Y", "radians per second");
        
        // Request an event when the simulation starts
        hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_SIM_START, "SimStart");
        hr = SimConnect_RequestDataOnSimObjectType(hSimConnect, REQUEST_1, DEFINITION_1, 0, SIMCONNECT_SIMOBJECT_TYPE_USER);


        printf("\nLaunch a flight.");

        while( 0 == quit )
        {
            SimConnect_CallDispatch(hSimConnect, MyDispatchProcRD, NULL);
            Sleep(1);
        } 

        hr = SimConnect_Close(hSimConnect);
    }
}


int __cdecl _tmain(int argc, _TCHAR* argv[])
{   
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    StartCounter();
    while (1) {
        testDataRequest();
        Sleep(1000);
    }

	return 0;
}





