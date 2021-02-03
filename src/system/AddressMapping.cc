#include "system/AddressMapping.h"

//IMPORTANT: the adresses have to be the differnce between the logical address and the beginning if the mapping


//returns the corresponding physical address of a logical address which is reprensented by an 32 Bit Unsigned
unsigned AddressMapping::log2phys(unsigned *virtualMemStart,caddr_t logaddr) {
	unsigned offset = addr2offset(logaddr);
	unsigned page = addr2page(logaddr); 
	unsigned pageFrame = page2frame(virtualMemStart, page);
	unsigned physicalAddress = pageFrame + offset; 
	return physicalAddress;
}

//returns page identifaction part of an address, e.g.: 0000000000 1010101010 1010101010
unsigned AddressMapping::addr2page(caddr_t logaddr) {
    //turn into number so we can do our math with it
    size_t addressAsNr = reinterpret_cast<size_t> (logaddr);
    //shift until page part of the number is right
    addressAsNr = addressAsNr >> 12;
    //make all but the last 10 Bits zero
    addressAsNr = addressAsNr & 0xFFFFF; //
    //shift the number back to the left
    addressAsNr = addressAsNr  << 12;
    //cast that should be not a problem because the first 32 Bits should be zero
    unsigned page = (unsigned) addressAsNr;

	return page; 
}

//returns the offset part of an address
unsigned AddressMapping::addr2offset(caddr_t logaddr) {
    //turn into number so we can do out math on it
    size_t addressAsNr = reinterpret_cast<size_t> (logaddr);
    //make all but the last 12 Bits zero
    addressAsNr = addressAsNr & 0xFFF;
    //cast that should be not a problem
    unsigned offset = (unsigned) addressAsNr;
	return offset;
}

//returns the physical frame of the given page 
unsigned AddressMapping::page2frame(unsigned *virtualMemStart ,unsigned page) {
    unsigned distanceToPTinlogMem = *(virtualMemStart + page2pageDirectoryIndex(page));
    unsigned pageFrame = *(virtualMemStart + distanceToPTinlogMem + page2pageTableIndex(page));
    return pageFrame;
}

//returns the index for the pagetable, e.g.: 12*0 1100000011 0000110000 -> 22*0 1100000011
unsigned AddressMapping::page2pageTableIndex(unsigned page) {
    unsigned pageTableIndex = page & 0x3FF;//to make sure the other 22 Bits are 0
	return pageTableIndex;
}

//returns the index for the pagedirectory e.g.: 
unsigned AddressMapping::page2pageDirectoryIndex(unsigned page) {
    unsigned pageDirectoryIndex = page >> 10;
}

/**
 * create and return an Offset build from parameters
 *
 * @param presentBit 0 = not present, 1 = present
 * @param read_writeBit 0 = read, 1 = write
 * @param pinnedBit 0 = not pinned, 1 = pinned
 * @param accessBit 0 = not accessed, 1 = accessed
 * @param dirtyBit 0 = means swapfile = ram, 1 = means ram has changed data
 * @param pageSizeBit 0 = refering to a PT, 1 = refering to pageframe
 * @return complett Offset (you can add it to the address)
 */
unsigned AddressMapping::createOffset(bool presentBit, bool read_writeBit, bool pinnedBit, bool accessBit, bool dirtyBit, bool pageSizeBit) {
    unsigned offset = 0;

    if (presentBit) {
        offset = offset | 0b1;
    }

    if (read_writeBit) {
        offset = offset | 0b10;
    }

    if (pinnedBit) {
        offset = offset | 0b1000;
    }

    if (accessBit) {
        offset = offset | 0b100000;
    }

    if (dirtyBit) {
        offset = offset | 0b1000000;
    }

    if (pageSizeBit) {
        offset = offset | 0b10000000;
    }

    return offset;
}

/**
 * @return 0 = not present, 1 = present
 */
unsigned AddressMapping::getPresentBit(unsigned phyAddr) {
    return phyAddr & 0b1;
}

unsigned AddressMapping::setPresentBit(unsigned phyAddr) {
    return phyAddr;
}
