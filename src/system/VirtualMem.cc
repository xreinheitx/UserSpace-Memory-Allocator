#include "system/VirtualMem.h"


#define PAGESIZE sysconf(_SC_PAGESIZE)
#define FOUR_GB 4294967296
#define PAGETABLE_SIZE 1024
#define NUMBER_OF_PAGES FOUR_GB/PAGESIZE
#define NUMBER_OF_PT NUMBER_OF_PAGES/PAGETABLE_SIZE

void VirtualMem::initializeVirtualMem(unsigned numberOfPF)
{
	this -> numberOfPF = numberOfPF;
	unsigned phyMenLength = PAGESIZE * numberOfPF;
	//open the shared memory file (physical memory)
	this -> fd = shm_open("phy-Mem", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		cerr << "|###> Error: open the shm failed" <<endl;
        exit(1); 
	}
	if (ftruncate(fd, phyMenLength) == -1) {
		cerr << "|###> Error: truncate failed" <<endl;
        exit(1); 
	}

    /*map the whole logical memory size*/
	this -> virtualMemStartAddress = (unsigned*) mmap(NULL, FOUR_GB, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(this -> virtualMemStartAddress == (unsigned*) MAP_FAILED)
    {
        cerr << "|###> Error: virtual Mmap Failed" <<endl;
        exit(1);
    }

	initializePDandFirstPT();
}


/**
 * This method is just called, when the whole virtual memory gets initialized.
 * It unmaps the first 2 pages of logical memory and maps 2 page frames of phys. memory,
 * to use. Furthermore it pins them.
*/
void VirtualMem::initializePDandFirstPT()
{
	//unmap the first page, to map the same number of page frame for the PD
	//munmap(this->virtualMemStartAddress, PAGESIZE*(NUMBER_OF_PT+1));-> think this is wrong, because we are not using all PT in the beginning
	munmap(this->virtualMemStartAddress, PAGESIZE);

	//map page frame for PD
	unsigned* addrPD = (unsigned*) mmap(this->virtualMemStartAddress, PAGESIZE, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_FIXED, this -> fd, 0);
    if(addrPD == (unsigned*) MAP_FAILED)
    {
        cerr << "|###> Error: Mmap PD Failed" <<endl;
        exit(1);
    }
	nextFreeFrameIndex++;
	pagesinRAM++;
	this->writeQueue.enQueue(virtualMemStartAddress);
	


	//initialize first PT with the two phys adresses of the PD and the PT itself
	munmap(this->virtualMemStartAddress + PAGETABLE_SIZE, PAGESIZE);

	//map page frame for PD
	unsigned* addrFirstPT = (unsigned*) mmap(this->virtualMemStartAddress + PAGETABLE_SIZE, PAGESIZE, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_FIXED, this -> fd, PAGESIZE);
    if(addrFirstPT == (unsigned*) MAP_FAILED)
    {
        cerr << "|###> Error: Mmap PD Failed" <<endl;
        exit(1);
    }
	nextFreeFrameIndex++;
	pagesinRAM++;
	this->writeQueue.enQueue(virtualMemStartAddress + PAGETABLE_SIZE);
	*(virtualMemStartAddress + PAGETABLE_SIZE) = ((0 << 12) | mappingUnit.createOffset(1,1,1,1));
	*(virtualMemStartAddress + PAGETABLE_SIZE + 1) = ((1 << 12) | mappingUnit.createOffset(1,1,1,1));


	//add the physical address of the PT in the PD -> here the logical and physical value is equal to one another
	*(virtualMemStartAddress) = (1 << 12) | mappingUnit.createOffset(1,1,1,1);


	//create all PT but the presentBits are not set -> they are not mapped in
	unsigned offset = mappingUnit.createOffset(0,1,0,0);
	for(unsigned i = 1; i < PAGETABLE_SIZE; i++)
	{
		//(i+1) because at 0 is PD and at 1 is first PT, so start the others PT starts at 2
		*(virtualMemStartAddress + i) = (((i+1) << 12) | offset);
	}
}

void VirtualMem::fixPermissions(void* address)
{
	void* pageStartAddr = findStartAddress(address);
	unsigned* pagePTEntryAddr = mappingUnit.logAddr2PTEntryAddr(virtualMemStartAddress, (unsigned*) pageStartAddr);

	unsigned phyAddr = ((char*) pageStartAddr) - ((char*) virtualMemStartAddress);
	unsigned first10Bits = mappingUnit.phyAddr2PDIndex(phyAddr);
	//if PT not present
	if (pagePTEntryAddr == 0) {
		unsigned* addrPDEntry = virtualMemStartAddress + first10Bits;
		addPTEntry2PD(addrPDEntry);
		pagePTEntryAddr = mappingUnit.logAddr2PTEntryAddr(virtualMemStartAddress, (unsigned*) pageStartAddr);
	}

    unsigned pageFrameAddr = mappingUnit.logAddr2PF(virtualMemStartAddress, (unsigned*) pageStartAddr);

	permission_change permissionChange;

	//determine which permission change is the right one
	if(mappingUnit.getPresentBit(pageFrameAddr) == NOT_PRESENT)
	{
		//this is the case when we change the permission from non to read
		if(pagesinRAM < numberOfPF)
		{
			permissionChange = NONTOREAD_NOTFULL;
		}else
		{
			permissionChange = NONTOREAD_FULL;
		}
	}else{
		//this is the case when we change the permission from read to write
			permissionChange = READTOWRITE;
	}


	switch(permissionChange)
	{
		case NONTOREAD_NOTFULL:
			//if there is already data on the disk, we have to this data in
			if (mappingUnit.getAccessed(pageFrameAddr) == ACCESSED) {
				this->pageIn(pageStartAddr);
			}
			mapIn(pageStartAddr);
			readPageActivate(pageStartAddr);
			break;

		case NONTOREAD_FULL:
			//first we have to kick out one chunk, preferibly in the readQueue
			void* kickedChunkAddr;
			if (!this->readQueue.isEmpty()) {
				kickedChunkAddr = this->readQueue.deQueue();
			}else{
				//kickout a chunk in the writeQueue
				kickedChunkAddr = this->writeQueue.deQueue();
    			unsigned kickedPageFrameAddr = mappingUnit.logAddr2PF(virtualMemStartAddress, (unsigned*) kickedChunkAddr);
				unsigned pinnedBit = mappingUnit.getPinnedBit(kickedPageFrameAddr);

				while (pinnedBit == PINNED) {
					this->writeQueue.enQueue(kickedChunkAddr);
					kickedChunkAddr = this->writeQueue.deQueue();
					kickedPageFrameAddr = mappingUnit.logAddr2PF(virtualMemStartAddress, (unsigned*) kickedChunkAddr);
					pinnedBit = mappingUnit.getPinnedBit(kickedPageFrameAddr);
				}
				this->pageOut(kickedChunkAddr);
			}
			//now just deactivate all the stuff
			mapOut(kickedChunkAddr);
			

			mapIn(pageStartAddr);
			//last but not least: activate the chunk just like in case 1
			if (mappingUnit.getAccessed(pageFrameAddr) == ACCESSED) {
				mprotect(pageStartAddr, PAGESIZE, PROT_WRITE);
				this->pageIn(pageStartAddr);
			}
			readPageActivate(pageStartAddr);
			break;

		case READTOWRITE:
			//first we have to delete the chunk in the read queue
			this->readQueue.deQueue(pageStartAddr);
			//last we set all the flags right
			writePageActivate(pageStartAddr);
			break;
	}

	pageFrameAddr = mappingUnit.logAddr2PF(virtualMemStartAddress, (unsigned*) pageStartAddr);
}

void VirtualMem::readPageActivate(void* pageStartAddr)
{
	unsigned* pageTableEntry = mappingUnit.logAddr2PTEntryAddr(virtualMemStartAddress, (unsigned*) pageStartAddr);
	
	mappingUnit.setReadAndWriteBit(pageTableEntry, READ);
	mprotect(pageStartAddr, PAGESIZE, PROT_READ);
	this->readQueue.enQueue(pageStartAddr);
}

void VirtualMem::writePageActivate(void* pageStartAddr)
{
	unsigned* pageTableEntry = mappingUnit.logAddr2PTEntryAddr(virtualMemStartAddress, (unsigned*) pageStartAddr);
	
	mappingUnit.setReadAndWriteBit(pageTableEntry, WRITE);
	mprotect(pageStartAddr, PAGESIZE, PROT_WRITE);
	this->writeQueue.enQueue(pageStartAddr);
}

void VirtualMem::pageOut(void* kickedChunkAddr)
{
	off_t offset = reinterpret_cast<off_t>(kickedChunkAddr)-reinterpret_cast<off_t>(this->virtualMemStartAddress); 
	this->swapFile.swapFileWrite(kickedChunkAddr, offset , PAGESIZE);
	unsigned* pageTableEntry = mappingUnit.logAddr2PTEntryAddr(virtualMemStartAddress, (unsigned*) kickedChunkAddr);
    unsigned pageFrameAddr = *(pageTableEntry);
	this->pageoutPointer = mappingUnit.cutOfOffset(pageFrameAddr);
}


void VirtualMem::pageIn(void* chunckStartAddr)
{
	off_t offset = reinterpret_cast<off_t>(chunckStartAddr)-reinterpret_cast<off_t>(this->virtualMemStartAddress); 
	this->swapFile.swapFileRead(chunckStartAddr, offset , PAGESIZE);
}

void VirtualMem::mapOut(void* pageStartAddress) {
	pagesinRAM--;
	//map out shared memory file
	munmap(pageStartAddress, PAGESIZE);
	//map in MAP_Anonymous (simulation for no physical nemory behind it)
	mmap(pageStartAddress, PAGESIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	unsigned* pageTableEntry = mappingUnit.logAddr2PTEntryAddr(virtualMemStartAddress, (unsigned*) pageStartAddress);
	mappingUnit.setPresentBit(pageTableEntry, NOT_PRESENT);
	
	//TODO check if last Page (this only temporary solution)
	unsigned dis = ((char*) pageTableEntry) - ((char*) virtualMemStartAddress);
	if (dis < (PAGESIZE*2)+4) {
		dis = dis - PAGESIZE - 4;
		unsigned* pdEntry = (unsigned*) (((char*) virtualMemStartAddress) + dis);
		mappingUnit.setPresentBit(pdEntry, NOT_PRESENT);
	}
}

void VirtualMem::mapIn(void* pageStartAddress) {
	//unmap the virtual space
	munmap(pageStartAddress, PAGESIZE);

	//map in the physical space
	void* addr;
	if (pageoutPointer == 0) {
		addr = mmap(pageStartAddress, PAGESIZE, PROT_NONE, MAP_PRIVATE | MAP_FIXED, this -> fd, nextFreeFrameIndex*PAGESIZE);
	} else {
		addr = mmap(pageStartAddress, PAGESIZE, PROT_NONE, MAP_PRIVATE | MAP_FIXED, this -> fd, pageoutPointer*PAGESIZE);
	}
    if(addr == MAP_FAILED) {
    	cerr << "|###> Error: phy Mmap Failed from " << pageStartAddress << endl;
    	exit(1);
    }


	pagesinRAM++;
	addPageEntry2PT((unsigned*) pageStartAddress);
	if(nextFreeFrameIndex+1 < numberOfPF) {
		nextFreeFrameIndex++;
	}
}

void VirtualMem::addPageEntry2PT(unsigned* startAddrPage) {
	unsigned phyAddr = ((char*) startAddrPage) - ((char*) virtualMemStartAddress);
	unsigned first10Bits = mappingUnit.phyAddr2PDIndex(phyAddr);
	unsigned pageTableAddr = *(virtualMemStartAddress + first10Bits);
	
	//if the PT not existing then create it by setting the presentBit
	if (mappingUnit.getPresentBit(pageTableAddr) == NOT_PRESENT) {
		addPTEntry2PD(virtualMemStartAddress + first10Bits);
	}

	//TODO check if last Page (this only temporary solution)
	unsigned* pageTableEntry = mappingUnit.logAddr2PTEntryAddr(virtualMemStartAddress, (unsigned*) startAddrPage);
	unsigned dis = ((char*) pageTableEntry) - ((char*) virtualMemStartAddress);
	if (dis < (PAGESIZE*2)+4) {
		//add the page in PT
		if (pageoutPointer == 0) {
			*(pageTableEntry) = (nextFreeFrameIndex << 12) | mappingUnit.createOffset(1,0,1,1);
		} else {
			*(pageTableEntry) = (pageoutPointer << 12) | mappingUnit.createOffset(1,0,1,1);
			pageoutPointer = 0;
		}
	} else {
		//add the page in PT
		if (pageoutPointer == 0) {
			*(pageTableEntry) = (nextFreeFrameIndex << 12) | mappingUnit.createOffset(1,0,1,0);
		} else {
			*(pageTableEntry) = (pageoutPointer << 12) | mappingUnit.createOffset(1,0,1,0);
			pageoutPointer = 0;
		}
	}
}

void VirtualMem::addPTEntry2PD(unsigned* pdEntryOfPT) {
	char* pageStartAddressOfPT = ((char*) virtualMemStartAddress) + mappingUnit.phyAddr2page( *(pdEntryOfPT) );

	//if the RAM is full we need to throw something out
	if (pagesinRAM >= numberOfPF) {
		void* kickedChunkAddr;
		if (!this->readQueue.isEmpty()) {
			kickedChunkAddr = this->readQueue.deQueue();
		}else{
			//kickout a chunk in the writeQueue
			kickedChunkAddr = this->writeQueue.deQueue();
			unsigned kickedPageFrameAddr = mappingUnit.logAddr2PF(virtualMemStartAddress, (unsigned*) kickedChunkAddr);
			unsigned pinnedBit = mappingUnit.getPinnedBit(kickedPageFrameAddr);

			while (pinnedBit == PINNED) {
				this->writeQueue.enQueue(kickedChunkAddr);
				kickedChunkAddr = this->writeQueue.deQueue();
				kickedPageFrameAddr = mappingUnit.logAddr2PF(virtualMemStartAddress, (unsigned*) kickedChunkAddr);
				pinnedBit = mappingUnit.getPinnedBit(kickedPageFrameAddr);
			}
			this->pageOut(kickedChunkAddr);
		}
		mapOut(kickedChunkAddr);
	}

	mapIn(pageStartAddressOfPT);
	writePageActivate(pageStartAddressOfPT);

	//change PT to Present in PD
	mappingUnit.setPresentBit(pdEntryOfPT, PRESENT);
	mappingUnit.setAccessed(pdEntryOfPT, ACCESSED);
	mappingUnit.setPinnedBit(pdEntryOfPT, PINNED);
}

void* VirtualMem::findStartAddress(void* address)
{
	unsigned pageStart = mappingUnit.phyAddr2page(((char*) address) - ((char*) virtualMemStartAddress));
	return (void*) (((char*) virtualMemStartAddress) + pageStart);
}


void* VirtualMem::getStart()
{
    return (void*) (this->virtualMemStartAddress + ((NUMBER_OF_PT+1) * PAGETABLE_SIZE));
}

size_t VirtualMem::getSize()
{

    return PAGESIZE * NUMBER_OF_PAGES;
}

void VirtualMem::fillList(list<int>* virtualMem, list<unsigned>* physicalMem) {
	//virtualspace 
    unsigned* ptr1 = (unsigned*) virtualMemStartAddress;

    for (unsigned i = 0; i < PAGETABLE_SIZE; i++) {
        // 0 or 1 or 2 = none or read or write

		//searching in PD for present PT
		if (mappingUnit.getPresentBit(*(ptr1)) == PRESENT) {
			//found one at ...

			//searching in PT for present Pages
			unsigned* ptrToPT = (virtualMemStartAddress + (mappingUnit.cutOfOffset(*(ptr1))*PAGETABLE_SIZE));
			for (int j = 0; j < PAGETABLE_SIZE; j++) {
				unsigned page = *(ptrToPT);
				if (mappingUnit.getPresentBit(page) == PRESENT) {
					//found one Page
					if (mappingUnit.getReadAndWriteBit(page) == READ) {
						virtualMem -> push_back(1);
					} else {
						virtualMem -> push_back(2);
					}
					
					physicalMem -> push_back( (mappingUnit.cutOfOffset(page)+1));
						
				} else {
					virtualMem -> push_back(0);
					physicalMem -> push_back(0);
				}

				ptrToPT++;
			}

		}

		//one step more in PD
        ptr1++;
    }
}

void* VirtualMem::expand(size_t size)
{
    /*
    i dont think that it makes sense to implement expand because you could do 3 things:
        -increase the number of chunks, but as long as you dont increase the number of max. act. chunks
        it wouldnt have any effect other than increasing the "amount of space in the hard drive"
            (this is what the inactive chungs simulate)
        -increase the amount of max. active chunks...yeah...
        -increase the space of each chunk, but because the chunks are consecutive to each other you first have to destroy
        (at least) all but the first one, increase the memory of the first one for some amount and than rebuild the other with
        the new memory size...this also dont seem like it makes sense for this simulation
    */
    return NULL;
}

VirtualMem::~VirtualMem()
{
    munmap(this->virtualMemStartAddress, NUMBER_OF_PAGES * PAGESIZE);
	shm_unlink("phy-Mem");
}

void VirtualMem::resetQueues() {
	//need to be changed

	/*int k = writeQueue.size();
	int l = readQueue.size();
	for (int i = 0; i < k; i++) {
		void* kickedWriteChunkAddr = this->writeQueue.deQueue();
		kickedPageDeactivate(kickedWriteChunkAddr);
	}

	for (int i = 0; i < l; i++) {
		void* kickedReadChunkAddr = this->readQueue.deQueue();
		kickedPageDeactivate(kickedReadChunkAddr);
	}

	this -> pinnedPages = 0;*/
}
