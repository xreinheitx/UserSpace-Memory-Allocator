#include "system/MappedChunk.h"
#include <signal.h>
#include <unistd.h>

#define PAGESIZE sysconf(_SC_PAGESIZE)

MappedChunk::MappedChunk(size_t UserSize, size_t ChunksNumber, size_t MaxActChunks)
{   
    /////////////////////////////////////////////////
    /*Converting the size to multiple of PAGESIZE*/
    size_t CorrectSize; 
    if(!(UserSize % PAGESIZE))
    {
        CorrectSize = UserSize; 
        // cout << CorrectSize << endl;
    }
    else 
    {
        CorrectSize = (UserSize / PAGESIZE) * PAGESIZE + PAGESIZE;
        // cout << CorrectSize << endl;
    }
    /////////////////////////////////////////////////
    /*Validating the chunksNumber and the maxActChunks*/
    if((CorrectSize / ChunksNumber) % PAGESIZE != 0)
    {
        cerr << "|###> Error: wrong number of chunks" << endl; 
        exit(1);
    }
    if(MaxActChunks >= ChunksNumber)
    {
        cerr << "|###> Error: Wrong max number of active chunks" << endl;
        exit(1); 
    }
    /////////////////////////////////////////////////
    this->chunksNumber = ChunksNumber;
    this->maxActChunks = MaxActChunks;
    this->chunkSize = CorrectSize / ChunksNumber; 
    /////////////////////////////////////////////////
    /*Mapping the whole size*/
    this->memblock = mmap(NULL, CorrectSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(this->memblock == MAP_FAILED)
    {
        cerr << "|###> Error: Mmap Failed" <<endl;
        exit(1); 
    }
    else 
        cout << "|###> An anonymous mapping with length = " << CorrectSize << " has been created" <<endl; 
    /////////////////////////////////////////////////
    /*Seperating the (same size) chunks by marking each one as PROT_NONE*/
    unsigned long startAddress   =  reinterpret_cast<unsigned long>(this->memblock); 
    unsigned long lastAddress    =  startAddress + this->chunkSize * this->chunksNumber; 
    for(unsigned long chunkStartAddress = startAddress; chunkStartAddress < lastAddress; chunkStartAddress += this->chunkSize)
    {
        cout << "|###> " << chunkStartAddress << " has been marked with PROT_NONE" << endl;
        mprotect((void*) chunkStartAddress, this->chunkSize, PROT_NONE);
    }
}

void MappedChunk::FixPermissions(void *address)
{
    if(address<this->memblock || reinterpret_cast<size_t>(address) >= reinterpret_cast<size_t>(this->memblock) + this->chunksNumber * this->chunkSize)
    {
        cout << "|>>> Error: SIGSEGV outside of the mmaped range! " << endl;
    }
    //mprotect(address, this->chunkSize, PROT_READ | PROT_WRITE);
    if(this->ChunkQueue.isFull(this->maxActChunks)) 
    {
        void* add = ChunkQueue.deQueue();

        mprotect(add, this->chunkSize, PROT_NONE);
        mprotect(this->FindStartAddress(address), this->chunkSize, PROT_READ | PROT_WRITE);

        this->ChunkQueue.enQueue(this->FindStartAddress(address));
    }
    else 
    {   
        this->ChunkQueue.enQueue(this->FindStartAddress(address));
        mprotect(this->FindStartAddress(address), this->chunkSize, PROT_READ | PROT_WRITE);
    }
}

void* MappedChunk::FindStartAddress(void* ptr)
{
   unsigned long address        =  reinterpret_cast<unsigned long>(ptr); 
   unsigned long startAddress   =  reinterpret_cast<unsigned long>(this->memblock); 
   unsigned long lastAddress    =  startAddress + this->chunkSize * this->chunksNumber; 
   for(unsigned long chunkStartAddress = startAddress; chunkStartAddress < lastAddress; chunkStartAddress += this->chunkSize)
   {
        if(address >= chunkStartAddress && address < chunkStartAddress + chunkSize) 
            return (void*) chunkStartAddress; 
   }
   /*
    It will never reach here
   */
   return NULL; 
}

void* MappedChunk::getStart()
{

    return this-> memblock;
}

size_t MappedChunk::getSize()
{

    return chunkSize * chunksNumber;
}

void MappedChunk::printChunkStarts()
{
    for(unsigned int i = 0; i < this -> chunksNumber; i++)
    {
        cout << reinterpret_cast<unsigned long>(memblock) + i * this->chunkSize <<endl;
    }
}

void MappedChunk::DisplayActiveChunks()
{

    this->ChunkQueue.displayQueue();
}

void* MappedChunk::expand(size_t size)
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

MappedChunk::~MappedChunk()
{

    munmap(this->memblock, this->chunksNumber * this -> chunkSize);
}
