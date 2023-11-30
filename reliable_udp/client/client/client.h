#pragma once
#ifndef __DEFINE_H__
#define __DEFINE_H__
#include "pkg.h"

class Packet {
public:
	uint32_t FLAG;  // ��־λ��HEAD, ACK��SYN��FIN
	uint32_t seq;  // ���к�
	uint32_t ack;  // ȷ�Ϻ�
	uint32_t len;  // ���ݲ��ֳ���
	uint32_t checksum;  // У���
	uint32_t window;  // ����
	char data[1024];  // ���ݳ���
public:
	Packet() : FLAG(0), seq(0), ack(0), len(0), checksum(0), window(0) { memset(data, 0, sizeof(data)); };
	Packet(uint32_t FLAG, uint32_t seq, uint32_t ack, uint32_t len, uint32_t checksum, uint32_t window);
	void setACK();
	void setSYN();
	void setSYNACK();
	void setFIN();
	void setFINACK();
	void setHEAD(int seq, int fileSize, char* fileName);
	void fillData(int seq, int size, char* data);
};

Packet::Packet(uint32_t FLAG, uint32_t seq, uint32_t ack, uint32_t len, uint32_t checksum, uint32_t window) {
	this->FLAG = FLAG;
	this->seq = seq;
	this->ack = ack;
	this->len = len;
	this->checksum = checksum;
	this->window = window;
	memset(this->data, 0, sizeof(this->data));
}
void Packet::setHEAD(int seq, int fileSize, char* fileName)
{
	// ����HEADλΪ1    
	this->FLAG = 0x8;

	// ����head��������һ�����ļ�����Ϣ�����ļ���С���ļ�����
	this->len = fileSize;
	this->seq = seq;
	memcpy(this->data, fileName, strlen(fileName) + 1);
}

void Packet::setACK() {
	// ����ACKλΪ1
	this->FLAG = 0x4;  // FLAG -> 00000100
}

void Packet::setSYN() {
	// ����SYNλΪ1
	this->FLAG = 0x2;  // FLAG -> 00000010
}

void Packet::setSYNACK() {
	// ����SYN, ACK = 1
	this->FLAG = 0x4 + 0x2;  // FLAG -> 00000110
}

void Packet::setFIN() {
	// ����FINλΪ1
	this->FLAG = 0x1;  // FLAG -> 00000001
}

void Packet::setFINACK() {
	// ����FIN, ACK = 1    
	this->FLAG = 0x4 + 0x1;  // FLAG -> 00000101
}



void Packet::fillData(int seq, int size, char* data) {
	// ���ļ������������ݰ�data����
	this->seq = seq;
	this->len = size;
	memcpy(this->data, data, size);
}

uint16_t checksum(uint32_t* pkt) {
	int size = sizeof(pkt);
	int count = (size + 1) / 2;
	uint16_t* buf = (uint16_t*)malloc(size);  
	memset(buf, 0, size);
	memcpy(buf, pkt, size);
	u_long sum = 0;
	while (count--) {
		sum += *buf++;
		if (sum & 0xFFFF0000) {
			sum &= 0xFFFF;
			sum++;
		}
	}
	return ~(sum & 0xFFFF);
}

bool isCorrupt(Packet* pkt) {
	if (pkt->checksum == checksum((uint32_t*)pkt))return false;
	return true;
}


#endif