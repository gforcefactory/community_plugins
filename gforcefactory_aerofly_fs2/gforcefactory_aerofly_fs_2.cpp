///////////////////////////////////////////////////////////////////////////////////////////////////
//
// file aerofly_fs_2_external_dll_sample.cpp
//
// PLEASE NOTE:  THE INTERFACE IN THIS FILE AND ALL DATA TYPES COULD BE SUBJECT TO SUBSTANTIAL
//               CHANGES WHILE AEROFLY FS 2 IS STILL RECEIVING UPDATES
//
// FURTHER NOTE: This sample just shows you how to read and send messages from the simulation
//               Some sample code is provided so see how to read and send messages
//
// 2019/12/19 - th/mb
//
// ---------------------------------------------------------------------------
//
// copyright (C) 2005-2017, Dr. Torsten Hans, Dr. Marc Borchers
// All rights reserved.
//
// Redistribution  and  use  in  source  and  binary  forms,  with  or  without
// modification, are permitted provided that the following conditions are met:
//
//  - Redistributions of  source code must  retain the above  copyright notice,
//    this list of conditions and the disclaimer below.
//  - Redistributions in binary form must reproduce the above copyright notice,
//    this  list of  conditions  and  the  disclaimer (as noted below)  in  the
//    documentation and/or other materials provided with the distribution.
//  - Neither the name of the copyright holder nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT  HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR  IMPLIED WARRANTIES, INCLUDING,  BUT NOT  LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND  FITNESS FOR A PARTICULAR  PURPOSE
// ARE  DISCLAIMED. 
//
///////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(WIN32) || defined(WIN64)
#if defined(_MSC_VER)
#pragma warning ( disable : 4530 )  // C++ exception handler used, but unwind semantics are not enabled
#pragma warning ( disable : 4577 )  // 'noexcept' used with no exception handling mode specified; termination on exception is not guaranteed. Specify /EHsc
#endif
#endif

#include "tm_external_message.h"
#include "tm_message_map.h"

#pragma comment(lib, "ws2_32.lib")
#include <WS2tcpip.h>
#include <windows.h>
#include <thread>
#include <vector>
#include <mutex>

static HINSTANCE global_hDLLinstance = NULL;

#include "tm_debugwindow.h"

//////////////////////////////////////////////////////////////////////////////////////////////////
//
// the main entry point for the DLL
//
//////////////////////////////////////////////////////////////////////////////////////////////////
BOOL WINAPI DllMain(HANDLE hdll, DWORD reason, LPVOID reserved)
{
	switch (reason)
	{
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_ATTACH:
		global_hDLLinstance = (HINSTANCE)hdll;
		break;
	case DLL_PROCESS_DETACH:
		break;
	}

	return TRUE;
}


#pragma pack(1)
struct edge_motion {
	uint8_t header;
	uint8_t packet_version;
	uint8_t motion_type;
	uint8_t type;
	uint32_t timestamp;
	float tx;
	float ty;
	float tz;
	float rx;
	float ry;
	float rz;
};


//////////////////////////////////////////////////////////////////////////////////////////////////
//
// a small helper function that shows the name of a message as plain text if an ID is passed
//
//////////////////////////////////////////////////////////////////////////////////////////////////
struct tm_message_type
{
	tm_string       String;
	tm_string_hash  StringHash;
	template <tm_uint32 N> constexpr tm_message_type(const char(&str)[N]) : String{ str }, StringHash{ str } { }
};

static std::vector<tm_message_type> MessageTypeList =
{
  MESSAGE_LIST(TM_MESSAGE_NAME)
};

static tm_string GetMessageName(const tm_external_message& message)
{
	for (const auto& mt : MessageTypeList)
	{
		if (message.GetID() == mt.StringHash.GetHash()) { return mt.String; }
	}

	return tm_string("unknown");
}

#define TRACE_LOG_SIZE 30

struct debug_data {
	uint32_t counter;
	tm_double time;
	tm_vector3d tracelog[TRACE_LOG_SIZE];
};
static debug_data  dd_out;
static std::mutex  debug_data_mutex;

void DebugDrawFloat(float &y, Gdiplus::Graphics& graphics, WCHAR* title, float value)
{
	Gdiplus::FontFamily fontFamily(L"Courier New");
	Gdiplus::Font       font(&fontFamily, 20, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	Gdiplus::SolidBrush black(Gdiplus::Color(255, 0, 0, 0));

	WCHAR buf[256];

	_snwprintf_s(buf, 255, _TRUNCATE, L"%s: %f", title, value);
	graphics.DrawString(buf, -1, &font, Gdiplus::PointF(0, y), &black);

	y += 16.0;
}

void DebugDrawVector(float& y, Gdiplus::Graphics& graphics, WCHAR* title, tm_vector3d value)
{
	Gdiplus::FontFamily fontFamily(L"Courier New");
	Gdiplus::Font       font(&fontFamily, 20, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	Gdiplus::SolidBrush black(Gdiplus::Color(255, 0, 0, 0));

	WCHAR buf[256];

	_snwprintf_s(buf, 255, _TRUNCATE, L"%s: x:%f y:%f z:%f", title, value.x, value.y, value.z);
	graphics.DrawString(buf, -1, &font, Gdiplus::PointF(0, y), &black);

	y += 16.0;
}

// the debug window
void DebugOutput_Draw(HDC hDC)
{
	// clear and draw to a bitmap instead to the hdc directly to avoid flicker
	Gdiplus::Bitmap     backbuffer(SAMPLE_WINDOW_WIDTH, SAMPLE_WINDOW_HEIGHT, PixelFormat24bppRGB);
	Gdiplus::Graphics   graphics(&backbuffer);
	Gdiplus::Color      clearcolor(255, 220, 232, 244);

	graphics.Clear(clearcolor);
	debug_data dd;
	{
		std::lock_guard<std::mutex> lock_guard{ debug_data_mutex };
		dd = dd_out;
	}

	float y = 4;
	DebugDrawFloat(y, graphics, L"counter: ", dd.counter);
	DebugDrawFloat(y, graphics, L"time: ", dd.time);
	for (int i = 0; i < TRACE_LOG_SIZE; i++) {
		DebugDrawVector(y, graphics, L"trace: ", dd.tracelog[i]);
	}

	// copy 'backbuffer' image to screen
	Gdiplus::Graphics graphics_final(hDC);
	graphics_final.DrawImage(&backbuffer, 0, 0);
}

tm_vector3d last_aircraft_velocity;

//////////////////////////////////////////////////////////////////////////////////////////////////
//
// interface functions to Aerofly FS 2
//
//////////////////////////////////////////////////////////////////////////////////////////////////
extern "C"
{
	__declspec(dllexport) int Aerofly_FS_2_External_DLL_GetInterfaceVersion()
	{
		return TM_DLL_INTERFACE_VERSION;
	}

	__declspec(dllexport) bool Aerofly_FS_2_External_DLL_Init(const HINSTANCE Aerofly_FS_2_hInstance)
	{
		DebugOutput_WindowOpen();
		memset(&dd_out, 0, sizeof(dd_out));
		return true;
	}

	__declspec(dllexport) void Aerofly_FS_2_External_DLL_Shutdown()
	{
		DebugOutput_WindowClose();
	}

	int resolvehelper(const char* hostname, int family, const char* service, sockaddr_storage* pAddr)
	{
		int result = 0;
		addrinfo* result_list = NULL;
		addrinfo hints = {};
		hints.ai_family = family;
		hints.ai_socktype = SOCK_DGRAM; // without this flag, getaddrinfo will return 3x the number of addresses (one for each socket type).
		result = getaddrinfo(hostname, service, &hints, &result_list);
		if (result == 0)
		{
			//ASSERT(result_list->ai_addrlen <= sizeof(sockaddr_in));
			memcpy(pAddr, result_list->ai_addr, result_list->ai_addrlen);
			freeaddrinfo(result_list);
		}

		return result;
	}

	__declspec(dllexport) void Aerofly_FS_2_External_DLL_Update(const tm_double         delta_time,
		const tm_uint8* const  message_list_received_byte_stream,
		const tm_uint32         message_list_received_byte_stream_size,
		const tm_uint32         message_list_received_num_messages,
		tm_uint8* message_list_sent_byte_stream,
		tm_uint32& message_list_sent_byte_stream_size,
		tm_uint32& message_list_sent_num_messages,
		const tm_uint32         message_list_sent_byte_stream_size_max)
	{
		//////////////////////////////////////////////////////////////////////////////////////////////
		//
		// build a list of messages that the simulation is sending
		//
		std::vector<tm_external_message>  MessageListReceive;

		tm_uint32 message_list_received_byte_stream_pos = 0;
		for (tm_uint32 i = 0; i < message_list_received_num_messages; ++i)
		{
			auto edm = tm_external_message::GetFromByteStream(message_list_received_byte_stream, message_list_received_byte_stream_pos);
			MessageListReceive.emplace_back(edm);
		}

		//////////////////////////////////////////////////////////////////////////////////////////////
		//
		// parse the message list
		//

		tm_double aircraft_pitch = 0;
		tm_double aircraft_bank = 0;
		tm_double aircraft_rateofturn = 0;
		tm_double simulation_time = 0;
		tm_vector3d aircraft_angularvelocity;
		tm_vector3d aircraft_velocity;

		for (const auto& message : MessageListReceive) {
			if (message.GetID() == MessageAircraftPitch.GetID()) { aircraft_pitch = message.GetDouble(); }
			else if (message.GetID() == MessageAircraftBank.GetID()) { aircraft_bank = message.GetDouble();  if (aircraft_bank > 3) aircraft_bank -= 6; }
			else if (message.GetID() == MessageAircraftRateOfTurn.GetID()) { aircraft_rateofturn = message.GetDouble(); }
			else if (message.GetID() == MessageAircraftAngularVelocity.GetID()) { aircraft_angularvelocity = message.GetVector3d(); } //Aircraft.Acceleration would be a better information, but the api gives only 1 value per secound
			else if (message.GetID() == MessageSimulationTime.GetID()) { simulation_time = message.GetDouble(); } //Aircraft.Acceleration would be a better information, but the api gives only 1 value per secound
			else if (message.GetID() == MessageAircraftVelocity.GetID()) { aircraft_velocity = message.GetVector3d(); } //Aircraft.Acceleration would be a better information, but the api gives only 1 value per secound
			// for possible values see list of messages at line 73 and following ...
		}

		if (!MessageListReceive.empty()) {

			int result = 0;
			SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
			char szIP[100] = "";
			sockaddr_in addrListen = {}; // zero-int, sin_port is 0, which picks a random port for bind.
			addrListen.sin_family = AF_INET;
			result = bind(sock, (sockaddr*)&addrListen, sizeof(addrListen));
			if (result == -1) { int lasterror = errno; }
			sockaddr_storage addrDest = {};
			result = resolvehelper("192.168.0.17", AF_INET, "50001", &addrDest); //4123 is the local plugin port
			if (result != 0) { int lasterror = errno; }
			tm_double altitude = -1;

			edge_motion msg;
			msg.header = 'G';
			msg.packet_version = 0;
			msg.motion_type = 'M';
			msg.type = 5;

			tm_vector3d delta_velocity;
			delta_velocity.x = aircraft_velocity.x - last_aircraft_velocity.x;
			delta_velocity.y = aircraft_velocity.y - last_aircraft_velocity.y;
			delta_velocity.z = aircraft_velocity.z - last_aircraft_velocity.z;
			last_aircraft_velocity = aircraft_velocity;

			//msg.timestamp = (uint32_t)(simulation_time * 1000000.0);
			msg.rx = (float)aircraft_bank;
			msg.ry = (float)aircraft_rateofturn;
			msg.rz = (float)aircraft_pitch;
			msg.tx = 0.0;
			msg.ty = 0.0;
			msg.tz = 0.0;

			int msg_len = sizeof(edge_motion);
			result = sendto(sock, (const char*)&msg, msg_len, 0, (sockaddr*)&addrDest, sizeof(addrDest));

			// send the debug info
			{
				std::lock_guard<std::mutex> lock_guard{ debug_data_mutex };
				dd_out.time = simulation_time;
				dd_out.counter++;
				for (int i = TRACE_LOG_SIZE - 1; i > 0; i--) {
					dd_out.tracelog[i] = dd_out.tracelog[i-1];
				}
				dd_out.tracelog[0] = delta_velocity;
			}

		}
	}
}