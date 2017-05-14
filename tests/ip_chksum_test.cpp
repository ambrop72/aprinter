
#include <stddef.h>
#include <limits.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include <random>
#include <algorithm>
#include <functional>
#include <vector>

#include <aipstack/misc/Chksum.h>

using namespace AIpStack;

using random_bytes_engine = std::independent_bits_engine<
    std::mt19937, CHAR_BIT, unsigned char>;

static size_t const BufSize = 101;
static int const Iterations = 10000000;

int main ()
{
    std::random_device rd;
    random_bytes_engine rbe(rd());
    
    // This tests the m_sum overflow which is hard to trigger with typical inputs.
    // The test case is 1023 bytes 0xFF which are defined by a chain of buffers:
    // 1. 255 2-byte buffers
    //    At this point the 32-bit sum would be 0x00feff01.
    // 2. 1 1-byte buffer
    //    At this point the 32-bit sum would be 0xff0001fe. We have 0xFF in
    //    the upper 8 bits due to byte swapping due to uneven buffer size,
    //    this is what allows overflow to occur earlier than ordinary.
    // 3. 256 2-byte buffers
    //    Just before the last one the sum is 0xffff00ff and with the last
    //    one it overflows to 0x000000fe. The overflow must be detected and
    //    folded into the sum, to become 0x000000ff.
    {
        char data[1023];
        ::memset(data, 0xFF, sizeof(data));
        
        int const NumNodes = 512;
        IpBufNode node[NumNodes];
        for (int i = 0; i < NumNodes; i++) {
            size_t buf_size = (i == 255) ? 1: 2;
            IpBufNode *next = (i == NumNodes - 1) ? nullptr : &node[i + 1];
            node[i] = {data, buf_size, next};
        }
        
        uint16_t chksum = IpChksum(IpBufRef{&node[0], 0, 1023});
        
        uint16_t good_chksum = IpChksum(data, 1023);
        AMBRO_ASSERT_FORCE(chksum == good_chksum)
        AMBRO_ASSERT_FORCE(chksum == 0xFF)
    }
    
    char buf[BufSize];
    
    for (int iter = 0; iter < Iterations; iter++) {
        std::generate(buf, buf+BufSize, std::ref(rbe));
        uint16_t good_chksum = IpChksum(buf, BufSize);
        
        size_t break_poss[] = {BufSize/3, BufSize/3+1, BufSize/2, BufSize/2+1};
        int const num_break_pos = 4;
        
        IpBufNode node[4];
        
        for (int break1 = 0; break1 < num_break_pos; break1++) {
            size_t break1_pos = break_poss[break1];
            
            node[0] = {buf, break1_pos, &node[1]};
            node[1] = {buf+break1_pos, BufSize-break1_pos, nullptr};
            uint16_t chksum = IpChksum(IpBufRef{&node[0], 0, BufSize});
            AMBRO_ASSERT_FORCE(chksum == good_chksum)
            
            for (int break2 = break1+1; break2 < num_break_pos; break2++) {
                size_t break2_pos = break_poss[break2];
                AMBRO_ASSERT_FORCE(break2_pos > break1_pos)
                
                node[0] = {buf, break1_pos, &node[1]};
                node[1] = {buf+break1_pos, break2_pos-break1_pos, &node[2]};
                node[2] = {buf+break2_pos, BufSize-break2_pos, nullptr};
                uint16_t chksum = IpChksum(IpBufRef{&node[0], 0, BufSize});
                AMBRO_ASSERT_FORCE(chksum == good_chksum)
                
                for (int break3 = break2+1; break3 < num_break_pos; break3++) {
                    size_t break3_pos = break_poss[break3];
                    AMBRO_ASSERT_FORCE(break3_pos > break2_pos)
                    
                    node[0] = {buf, break1_pos, &node[1]};
                    node[1] = {buf+break1_pos, break2_pos-break1_pos, &node[2]};
                    node[2] = {buf+break2_pos, break3_pos-break2_pos, &node[3]};
                    node[3] = {buf+break3_pos, BufSize-break3_pos, nullptr};
                    uint16_t chksum = IpChksum(IpBufRef{&node[0], 0, BufSize});
                    AMBRO_ASSERT_FORCE(chksum == good_chksum)
                }
            }
        }
    }
    
    return 0;
}
