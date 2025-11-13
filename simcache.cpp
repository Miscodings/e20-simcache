/*
CS-UY 2214
simcache.cpp
*/

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <limits>
#include <iomanip>
#include <cstdlib>
#include <cstdint>
#include <regex>

using namespace std;

size_t const static NUM_REGS = 8;
size_t const static MEM_SIZE = 1<<13;
size_t const static REG_SIZE = 1<<16;

/*
    Prints out the correctly-formatted configuration of a cache.

    @param cache_name The name of the cache. "L1" or "L2"

    @param size The total size of the cache, measured in memory cells.
        Excludes metadata

    @param assoc The associativity of the cache. One of [1,2,4,8,16]

    @param blocksize The blocksize of the cache. One of [1,2,4,8,16,32,64])

    @param num_rows The number of rows in the given cache.
*/
void print_cache_config(const string &cache_name, int size, int assoc, int blocksize, int num_rows) {
    cout << "Cache " << cache_name << " has size " << size <<
        ", associativity " << assoc << ", blocksize " << blocksize <<
        ", rows " << num_rows << endl;
}

/*
    Prints out a correctly-formatted log entry.

    @param cache_name The name of the cache where the event
        occurred. "L1" or "L2"

    @param status The kind of cache event. "SW", "HIT", or
        "MISS"

    @param pc The program counter of the memory
        access instruction

    @param addr The memory address being accessed.

    @param row The cache row or set number where the data
        is stored.
*/
void print_log_entry(const string &cache_name, const string &status, int pc, int addr, int row) {
    cout << left << setw(8) << cache_name + " " + status <<  right <<
        " pc:" << setw(5) << pc <<
        "\taddr:" << setw(5) << addr <<
        "\trow:" << setw(4) << row << endl;
}

void load_machine_code(ifstream &f, uint16_t mem[]) {
    regex machine_code_re("^ram\\[(\\d+)\\] = 16'b(\\d+);.*$");
    size_t expectedaddr = 0;
    string line;
    while (getline(f, line)) {
        smatch sm;
        if (!regex_match(line, sm, machine_code_re)) {
            cerr << "Can't parse line: " << line << endl;
            exit(1);
        }
        size_t addr = stoi(sm[1], nullptr, 10);
        unsigned instr = stoi(sm[2], nullptr, 2);
        if (addr != expectedaddr) {
            cerr << "Memory addresses encountered out of sequence: " << addr << endl;
            exit(1);
        }
        if (addr >= MEM_SIZE) {
            cerr << "Program too big for memory" << endl;
            exit(1);
        }
        expectedaddr ++;
        mem[addr] = instr;
    }
}

void e20sim(uint16_t memory[], uint16_t regs[],
    int L1size, int L1assoc, int L1blocksize,
    int L2size = 0, int L2assoc = 0, int L2blocksize = 0) {

    int L1rows = (L1size / L1blocksize) / L1assoc;
    int L2rows = (L2size > 0) ? (L2size / L2blocksize) / L2assoc : 0;

    int L1tags[L1rows][L1assoc] = {};
    int L1valid[L1rows][L1assoc] = {};
    int L1lru[L1rows][L1assoc] = {};

    int L2tags[64][16] = {};
    int L2valid[64][16] = {};
    int L2lru[64][16] = {};

    print_cache_config("L1", L1size, L1assoc, L1blocksize, L1rows);
    if (L2size > 0) print_cache_config("L2", L2size, L2assoc, L2blocksize, L2rows);

    int time = 0;
    uint16_t pc = 0;
    bool running = true;

    while (running) {
        time++;
        uint16_t instr = memory[pc % MEM_SIZE];
        uint16_t opcode = (instr >> 13) & 0b111;
        uint16_t regSrcA, regSrcB, regDst, imm;

        if (opcode == 0b100 || opcode == 0b101) {
            // lw or sw
            if (opcode == 0b100) {
                regDst = (instr >> 7) & 0b111;
                regSrcA = (instr >> 10) & 0b111;
            } else {
                regSrcB = (instr >> 7) & 0b111;
                regSrcA = (instr >> 10) & 0b111;
            }

            imm = instr & 0b1111111;
            if (imm & 0b1000000) imm |= 0b1111111110000000;
            int addr = (regs[regSrcA] + imm) % MEM_SIZE;

            // ==== Access L1 Cache ====
            int L1blockAddr = addr / L1blocksize;
            int L1row = L1blockAddr % L1rows;
            int L1tag = L1blockAddr / L1rows;
            bool hitL1 = false;
            int L1slot = -1;

            for (int i = 0; i < L1assoc; i++) {
                if (L1valid[L1row][i] && L1tags[L1row][i] == L1tag) {
                    hitL1 = true;
                    L1slot = i;
                    break;
                }
            }

            if (opcode == 0b100) { // LW
                if (hitL1) {
                    print_log_entry("L1", "HIT", pc, addr, L1row);
                    L1lru[L1row][L1slot] = time;
                } else {
                    print_log_entry("L1", "MISS", pc, addr, L1row);

                    // ==== Access L2 Cache ====
                    bool hitL2 = false;
                    int L2row = 0, L2tag = 0, L2slot = 0;

                    if (L2size > 0) {
                        int L2blockAddr = addr / L2blocksize;
                        L2row = L2blockAddr % L2rows;
                        L2tag = L2blockAddr / L2rows;
                        for (int i = 0; i < L2assoc; i++) {
                            if (L2valid[L2row][i] && L2tags[L2row][i] == L2tag) {
                                hitL2 = true;
                                L2slot = i;
                                break;
                            }
                        }

                        if (hitL2) {
                            print_log_entry("L2", "HIT", pc, addr, L2row);
                            L2lru[L2row][L2slot] = time;
                        } else {
                            print_log_entry("L2", "MISS", pc, addr, L2row);
                            int oldest = 0;
                            for (int i = 1; i < L2assoc; i++)
                                if (!L2valid[L2row][i] || L2lru[L2row][i] < L2lru[L2row][oldest])
                                    oldest = i;
                            L2tags[L2row][oldest] = L2tag;
                            L2valid[L2row][oldest] = 1;
                            L2lru[L2row][oldest] = time;
                        }
                    }

                    // Allocate in L1
                    int oldest = 0;
                    for (int i = 1; i < L1assoc; i++)
                        if (!L1valid[L1row][i] || L1lru[L1row][i] < L1lru[L1row][oldest])
                            oldest = i;
                    L1tags[L1row][oldest] = L1tag;
                    L1valid[L1row][oldest] = 1;
                    L1lru[L1row][oldest] = time;
                }

                regs[regDst] = memory[addr];
            }
            else { // SW
                // write-through to L1 and L2
                print_log_entry("L1", "SW", pc, addr, L1row);
                int oldest = 0;
                for (int i = 1; i < L1assoc; i++)
                    if (!L1valid[L1row][i] || L1lru[L1row][i] < L1lru[L1row][oldest])
                        oldest = i;
                L1tags[L1row][oldest] = L1tag;
                L1valid[L1row][oldest] = 1;
                L1lru[L1row][oldest] = time;

                if (L2size > 0) {
                    int L2blockAddr = addr / L2blocksize;
                    int L2row = L2blockAddr % L2rows;
                    int L2tag = L2blockAddr / L2rows;
                    print_log_entry("L2", "SW", pc, addr, L2row);

                    oldest = 0;
                    for (int i = 1; i < L2assoc; i++)
                        if (!L2valid[L2row][i] || L2lru[L2row][i] < L2lru[L2row][oldest])
                            oldest = i;
                    L2tags[L2row][oldest] = L2tag;
                    L2valid[L2row][oldest] = 1;
                    L2lru[L2row][oldest] = time;
                }
                memory[addr] = regs[regSrcB];
            }
            pc++;
        }
        
        else if (opcode == 0b000) { // add or sub or other reg-reg ops
            regSrcA = (instr >> 10) & 0b111;
            regSrcB = (instr >> 7) & 0b111;
            regDst = (instr >> 4) & 0b111;
            uint16_t funct = instr & 0b1111;  // bottom 4 bits for function code
            if (funct == 0b0000) {  // add
                regs[regDst] = regs[regSrcA] + regs[regSrcB];
            } else if (funct == 0b0001) {  // sub
                regs[regDst] = regs[regSrcA] - regs[regSrcB];
            } else if (funct == 0b0011) {  // and
                regs[regDst] = regs[regSrcA] & regs[regSrcB];
            } else if (funct == 0b0010) {  // or
                regs[regDst] = regs[regSrcA] | regs[regSrcB];
            } else if (funct == 0b0100) {  // slt (set if less than)
                regs[regDst] = (regs[regSrcA] < regs[regSrcB]) ? 1 : 0;
            } else if (funct == 0b1000) {  // jr
                pc = regs[regSrcA];
                pc--;
            } else {
                exit(1);
            }
            pc++;
        }
        else if (opcode == 0b001) { // addi
            regDst = (instr >> 7) & 0b111;
            regSrcA = (instr >> 10) & 0b111;
            imm = instr & 0b1111111;  // 7-bit immediate
            if (imm & 0b1000000) imm |= 0b1111111110000000;
            regs[regDst] = regs[regSrcA] + imm;
            pc++;
        }
        else if (opcode == 0b110) { // jeq
            regSrcA = (instr >> 10) & 0b111;
            regSrcB = (instr >> 7) & 0b111;
            imm = instr & 0b1111111;  // 7-bit immediate
            if (imm & 0b1000000) imm |= 0b1111111110000000; // sign-extend if negative (bit 6 is set)
            if (regs[regSrcA] == regs[regSrcB]) {
                pc = pc + 1 + imm;
            }
            else {
                pc = pc + 1;
            }
        }  
        else if (opcode == 0b111) {
            regSrcA = (instr >> 10) & 0b111;
            regDst = (instr >> 7) & 0b111;
            imm = instr & 0b1111111;  // 7-bit immediate
            if (imm & 0b1000000) imm |= 0b1111111110000000; // sign-extend if negative (bit 6 is set)
            if (regs[regSrcA] < imm) {
                regs[regDst] = 1;
            }
            else {
                regs[regDst] = 0;
            }
            pc++;
        }
        else if (opcode == 0b010) { // j
            imm = instr & 0b1111111111111;  // 13-bit immediate
            if (imm == pc) {
                running = false;
            }
            else {
                imm = instr & 0b1111111111111;  // 13-bit absolute address
                pc = imm;    
            }
        }      
        else if (opcode == 0b011) { // jal
            imm = instr & 0b1111111111111;  // 13-bit absolute address
            regs[7] = pc + 1;  // store return address in $7
            pc = imm;
        }
        else { // unknown
            exit(1);
        }
        regs[0] = 0;
    }
}

/**
    Main function
    Takes command-line args as documented below
*/
int main(int argc, char *argv[]) {
    /*
        Parse the command-line arguments
    */
    char *filename = nullptr;
    bool do_help = false;
    bool arg_error = false;
    string cache_config;
    for (int i=1; i<argc; i++) {
        string arg(argv[i]);
        if (arg.rfind("-",0)==0) {
            if (arg== "-h" || arg == "--help")
                do_help = true;
            else if (arg=="--cache") {
                i++;
                if (i>=argc)
                    arg_error = true;
                else
                    cache_config = argv[i];
            }
            else
                arg_error = true;
        } else {
            if (filename == nullptr)
                filename = argv[i];
            else
                arg_error = true;
        }
    }

    /* Display error message if appropriate */
    if (arg_error || do_help || filename == nullptr) {
        cerr << "usage " << argv[0] << " [-h] [--cache CACHE] filename" << endl << endl;
        cerr << "Simulate E20 cache" << endl << endl;
        cerr << "positional arguments:" << endl;
        cerr << "  filename    The file containing machine code, typically with .bin suffix" << endl<<endl;
        cerr << "optional arguments:"<<endl;
        cerr << "  -h, --help  show this help message and exit"<<endl;
        cerr << "  --cache CACHE  Cache configuration: size,associativity,blocksize (for one"<<endl;
        cerr << "                 cache) or"<<endl;
        cerr << "                 size,associativity,blocksize,size,associativity,blocksize"<<endl;
        cerr << "                 (for two caches)"<<endl;
        return 1;
    }

    ifstream f(filename);
    if (!f.is_open()) {
        cerr << "Can't open file "<<filename<<endl;
        return 1;
    }

    /* parse cache config */
    if (cache_config.size() > 0) {
        vector<int> parts;
        size_t pos;
        size_t lastpos = 0;
        while ((pos = cache_config.find(",", lastpos)) != string::npos) {
            parts.push_back(stoi(cache_config.substr(lastpos,pos)));
            lastpos = pos + 1;
        }
        parts.push_back(stoi(cache_config.substr(lastpos)));

        uint16_t memory[MEM_SIZE] = {0};
        uint16_t regs[NUM_REGS] = {0};
        load_machine_code(f, memory);

        if (parts.size() == 3) {
            int L1size = parts[0];
            int L1assoc = parts[1];
            int L1blocksize = parts[2];
            // TODO: execute E20 program and simulate one cache here
            e20sim(memory, regs, L1size, L1assoc, L1blocksize);
        } else if (parts.size() == 6) {
            int L1size = parts[0];
            int L1assoc = parts[1];
            int L1blocksize = parts[2];
            int L2size = parts[3];
            int L2assoc = parts[4];
            int L2blocksize = parts[5];
            // TODO: execute E20 program and simulate two caches here
            e20sim(memory, regs, L1size, L1assoc, L1blocksize,
                L2size, L2assoc, L2blocksize);
        } else {
            cerr << "Invalid cache config"  << endl;
            return 1;
        }
    }

    return 0;
}
