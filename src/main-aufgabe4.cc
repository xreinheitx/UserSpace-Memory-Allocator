#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <boost/program_options.hpp>

#include "system/memory/MappedChunk.h"

// size of a chunk
static const unsigned DEFAULT_CHUNKSIZE = 4096;

// a simple version of bubble sort
void bubbleSort(unsigned** array, unsigned nrElements)
{
    for (unsigned i = 0; i < nrElements; i++) {
        for (unsigned j = i; j < nrElements; j++) {
            if (*(array[i]) > *(array[j])) {
                int temp = *(array[i]);
                *(array[i]) = *(array[j]);
                *(array[j]) = temp;
            }
        }
    }
}

/**
 * A simple benchmark for mapped chunk.
 *
 * It creates an array of numbers and sorts them.
 * The numbers are distributed among the whole memory.
 * Each of them resides on its own block and the block size,
 * which means the distance between elements, can be specified.
 *
 * This benchmark accepts a set of options.
 * Use --help to see a list of them.
 */
int main(int argc, char** argv)
{
    /** 
     * parse command line arguments
     */
    namespace po = boost::program_options;
    // define available arguments
    po::options_description desc("Options");
    desc.add_options()
        ("help,h", "Print help message")
        ("total-chunks,t", po::value<uint64_t>()->required(), "Number of chunks (total)")
        ("max-chunks,m", po::value<uint64_t>()->required(), "Number of chunks available at the same time)")
        ("blocksize,b", po::value<uint64_t>()->required(), "Size of a block (determines access pattern)")
        (",r", po::value<uint64_t>()->default_value(10), "Repeat benchmark <arg> times")
        ("wball,w", "Write everything back (otherwise only dirty chunks are written back")
        ("gui,g", "Enable GUI"); 

    // read arguments
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    // did someone say 'help'?
    if (vm.count("help")) {
        std::cerr << desc << std::endl;
        return 0;
    }
    // read arguments into variables
    po::notify(vm);
    uint64_t totalChunks = vm["total-chunks"].as<uint64_t>();
    uint64_t maxChunksAvailable = vm["max-chunks"].as<uint64_t>();
    uint64_t blockSize = vm["blocksize"].as<uint64_t>();
    uint64_t runs = vm["-r"].as<uint64_t>();
    bool showGUI = false;
    if (vm.count("gui")) {
        showGUI = true;
    }
    bool writeBackAll = false;
    if (vm.count("wball")) {
        writeBackAll = true;
    }
    /**
     * parsing command line arguments done.
     * create memory with the above 
     * read configuration.
     */
    MappedChunk mem(DEFAULT_CHUNKSIZE, totalChunks, DEFAULT_CHUNKSIZE, maxChunksAvailable, writeBackAll);

    if (showGUI) {
        // TODO
        // create visualization
    }
    std::cout << "# totalChunks,maxChunksAvailable,blockSize,writeBackAll,times" << std::endl;
    std::cout << totalChunks << "," << maxChunksAvailable << "," << blockSize << "," << writeBackAll;

    // each element has a size of blockSize
    // how many can we store in the memory?
    unsigned nrElements = (totalChunks*DEFAULT_CHUNKSIZE)/blockSize;

    // this array stores pointers to the blocks
    unsigned* blocks[nrElements];

    // store the start address of each block in the array
    unsigned* memStart = static_cast<unsigned*>(mem.getStart());
    for (unsigned i = 0; i < nrElements; i++) {
        blocks[i] = memStart + (i * blockSize/sizeof(unsigned));
    }

    // for each run
    for (uint64_t run = 0; run < runs; run++) 
    {
        // initialize the field 
        for (unsigned i = 0; i < nrElements; i++) {
            *blocks[i] = i; // already sorted
            // *blocks[i] = std::rand() % 100; // pseudo-random values
            // *blocks[i] = nrElements-i; // reverse sorted
        }

        // start the timer
        auto start = std::chrono::high_resolution_clock::now();

        // run sort
        bubbleSort(blocks, nrElements);

        // stop timer and print
        auto end = std::chrono::high_resolution_clock::now();
        // options: milli, micro or nano
        long time = std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
        std::cout << "," << time;

        if (showGUI) break;
    }
    std::cout << std::endl;

    if (showGUI) {
        // TODO
        // clean up
    }

    return 0;
}

