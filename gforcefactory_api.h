#pragma once

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

void send_edge_motion_message(edge_motion& msg) {
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	char broadcast = 1;
	setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
	sockaddr_in addrListen = {}; // zero-int, sin_port is 0, which picks a random port for bind.
	addrListen.sin_family = AF_INET;
	bind(sock, (sockaddr*)&addrListen, sizeof(addrListen));
	sockaddr_storage addrDest = {};
	resolvehelper("EDGE6D", AF_INET, "50001", &addrDest); //4123 is the local plugin port
	resolvehelper("255.255.255.255", AF_INET, "50001", &addrDest); //4123 is the local plugin port
	int msg_len = sizeof(edge_motion);
	sendto(sock, (const char*)&msg, msg_len, 0, (sockaddr*)&addrDest, sizeof(addrDest));
	closesocket(sock);
}