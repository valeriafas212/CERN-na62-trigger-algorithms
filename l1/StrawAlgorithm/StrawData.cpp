/*
 * StrawData.cpp
 *
 *  Created on: Jul 29, 2014
 *	Author: Thomas Hancock (Thomas.hancock.2012@my.bristol.ac.uk)
 *	Modified: Laura Rogers (lr12078@my.bristol.ac.uk)
 *
 *	Please Note: Thomas and Laura were both summer students, who finished on the 
 *	12/09/14. They are no longer working on the code, however, can still answer questions.
 */

#include "StrawData.h"

namespace na62 {
// Public Functions
StrawData::StrawData(char* pointerToData) {
	dataPointer = pointerToData;
	readPacketLength();
	m_numberOfHits = (int) (m_strawDataHdr.packetLength - 24) / 2.0;
	readL0TriggerDecision();
	readChamber();
	readView();
	readHalfView();
	readCoarseTime();
	readNEdgesSlots();
	loadHit(0); // Loads the first hit automatically
}

/* 
 * This function reads in the data for a specific hit, indicated by an arbitrary number 
 * assigned to it based on it's position in the MEP (begins at 0 and counts up). 
 * The data is stored in the member variables m_error, m_strawID, m_planeID, 
 * m_edgeType and m_fineTime.
 * 
 * @param hitNum		the number of the hit in the MEP which is to be accessed
 */
void StrawData::loadHit(int hitNum) {
	initialiseHitVariables();
	if (hitNum > m_numberOfHits - 1) {
		m_error = 255; // Used to determine if the hit was successfully loaded or not. The is encoded in the SRB in 2 bits, so setting all 8 bits of m_error to 1 clearly indicates something has gone wrong (in this case, the hit number accessed is greater than the number of hit stored).
	} else {
		readDataWord(hitNum);
		separateStrawAndPlaneIDs(); // The strawID and planeID are encoded in the same number. This function separates them so that they can be accessed separately.
	}
}

// This function calculates the SRB id from the chamber, view and half view IDs.
inline int StrawData::getSrbID(uint8_t chamber, uint8_t view,
		uint8_t halfView) {
	return halfView + 2 * view + 8 * chamber;
}

/* 
 * This function calculates the distance of the hit from the origin (set as the centre of the
 * chamber) and returns the value in mm.
 */
double StrawData::getStrawDistance() {
	// Note: This code was written when the straw mapping was not finalised, and so this function will need modifying once the final straw mapping is done
	if (m_error == 255) { // Prevent the distance trying to be set when the hit did not load correctly.
		return 0;
	} else {
		double strawDisplacement = m_strawID * strawparameters::STRAW_SPACING;
		if (strawDisplacement
				> (strawparameters::STRAW_LENGTH / 2.0)
						+ strawparameters::CENTRAL_GAP_DISPLACEMENT[m_strawDataHdr.view][m_strawDataHdr.chamber]) {
			strawDisplacement += strawparameters::CENTRAL_GAP_WIDTH;
		}
		strawDisplacement += strawparameters::PLANE_INDENTS[m_planeID
				+ 2 * (int) m_strawDataHdr.halfView];
		return strawDisplacement - (strawparameters::STRAW_LENGTH / 2.0);
	}
}

void StrawData::printHeader() {
	std::cout << " PacketLength = " << (int) m_strawDataHdr.packetLength
			<< std::endl;
	std::cout << " L0TriggerDecision = " << (int) m_strawDataHdr.triggerTypeWord
			<< std::endl;
	std::cout << " Chamber, View, Halfview = " << (int) m_strawDataHdr.chamber
			<< ", " << (int) m_strawDataHdr.view << ", "
			<< (int) m_strawDataHdr.halfView << std::endl;
	std::cout << " CoarseTime = " << (int) m_coarseTime << std::endl;
}

void StrawData::printNEdgesSlots() {
	std::cout << " NEdgesSlots = ";
	for (int i = 0; i < 16; i++) {
		std::cout << (int) m_nEdgesArray[i];
		if (i != 15)
			std::cout << ",";
	}
	std::cout << std::endl;
}

void StrawData::printHit() {
	if (m_error == 255) {
		std::cout << "Error: No hit exists" << std::endl;
	} else {
		std::cout << "  Err = " << (int) m_error << std::endl;
		std::cout << "  StrawID = " << (int) m_strawID << std::endl;
		std::cout << "  PlaneID = " << (int) m_planeID << std::endl;
		std::cout << "  Edge = " << (int) m_edgeType << std::endl;
		std::cout << "  FineTime = " << m_fineTime << std::endl;
	}
}

// Private Functions
/* 
 * The readX functions read in the corresponding section of binary in the binary file
 *  passed to the L1 framework and decode it back into decimal. The binary is stored 
 * in a bitset and then converted as each byte of the binary file has been reversed 
 * since being passed to the framework, and so storing it allows the ordering to be 
 * manipulated to deal with this.  
 */
void StrawData::readL0TriggerDecision() {
	std::bitset<8> tempByte;
	uint8_t L0TriggerDecision = 0;
	memcpy(&tempByte, (dataPointer + 2), 1);
	for (int i = 7; i >= 0; i--) { //Reversed to deal with the swap in endianness
		if (tempByte[7 - i] == 1)
			L0TriggerDecision += (int) pow(2, i);
	}
	m_strawDataHdr.triggerTypeWord = L0TriggerDecision;
}

void StrawData::readChamber() {
	std::bitset<8> tempBinary;
	memcpy(&tempBinary, dataPointer + 3, 1);
	m_strawDataHdr.chamber = (2 * tempBinary[0] + 1 * tempBinary[1]);
}

void StrawData::readView() {
	std::bitset<8> tempBinary;
	memcpy(&tempBinary, dataPointer + 3, 1);
	m_strawDataHdr.view = (2 * tempBinary[2] + 1 * tempBinary[3]);
}

void StrawData::readHalfView() {
	std::bitset<8> tempBinary;
	memcpy(&tempBinary, dataPointer + 3, 1);
	m_strawDataHdr.halfView = tempBinary[4];
}

void StrawData::readPacketLength() {
	std::bitset<8> tempBinary1, tempBinary2;
	memcpy(&tempBinary1, dataPointer, 1);
	memcpy(&tempBinary2, (dataPointer + 1), 1);
	uint8_t packetLength = 0;
	for (int i = 15; i >= 0; i--) { //Reversed to deal with the swap in endianness
		if (i > 7) {
			if (tempBinary2[15 - i] == 1)
				packetLength += (int) pow(2, i);
		} else {
			if (tempBinary1[7 - i] == 1)
				packetLength += (int) pow(2, i);
		}
	}
	m_strawDataHdr.packetLength = packetLength;
}

void StrawData::readCoarseTime() {
	std::bitset<8> tempByte;
	std::bitset<32> tempBinary;
	for (int i = 0; i < 4; i++) {
		memcpy(&tempByte, dataPointer + 4 + i, 1);
		for (int j = 0; j < 8; j++) {
			tempBinary[j + (i * 8)] = tempByte[7 - j];
		}
	}
	uint32_t CoarseTime = 0;
	for (int i = 0; i < 32; i++) { //Reversed to deal with the swap in endianness
		if (tempBinary[i] == 1)
			CoarseTime += (int) pow(2, i);
	}
	m_coarseTime = CoarseTime;
}

void StrawData::readNEdgesSlots() {
	if (m_strawDataHdr.packetLength == 0)
		readPacketLength(); //Ensures the m_strawDataHdr.packetLength has been accessed
	for (int i = 0; i < 16; i++)
		m_nEdgesArray[i] = 0;
	std::bitset<8> tempByte;
	for (int i = 0; i < 16; i++) {
		memcpy(&tempByte, dataPointer + m_strawDataHdr.packetLength - 16 + i,
				1);
		for (int j = 7; j >= 0; j--) { //Reversed to deal with the swap in endianness
			if (tempByte[7 - j] == 1)
				m_nEdgesArray[i] += (int) pow(2, j);
		}
	}
}

void StrawData::readDataWord(int hitNum) {
	std::bitset<16> tempBinary;
	memcpy(&tempBinary, dataPointer + 8 + 2 * hitNum, 2);
	m_error = (2 * tempBinary[6] + 1 * tempBinary[7]);
	m_edgeType = tempBinary[13];
	int tempFineTime = 0;
	for (int i = 12; i > 7; i--) {
		if (tempBinary[i] == 1)
			tempFineTime += (int) pow(2, 12 - i);
	}
	m_fineTime = decodeFineTime(tempFineTime);
	for (int i = 5; i > -3; i--) {
		if (((i < 0) && (tempBinary[16 + i] == 1))
				|| ((i >= 0) && (tempBinary[i] == 1))) {
			m_strawID += (int) pow(2, 5 - i);
		}
	}
}

void StrawData::initialiseHitVariables() {
	m_error = 0;
	m_strawID = 0;
	m_edgeType = 0;
	m_fineTime = 0;
}

/* 
 * The finetime is stored by dividing the time window into 32 and storing which fraction 
 * of the time window the time fall into. This function converts this fraction back into a 
 * time in seconds.
 */
double StrawData::decodeFineTime(int tempFineTime) {
	return 25 * (tempFineTime / 32.0);
}

void StrawData::separateStrawAndPlaneIDs() {
	if (m_strawID < 128) {
		m_planeID = 0;
	} else {
		m_strawID = m_strawID - 128;
		m_planeID = 1;
	}
}

} /* namespace na62 */