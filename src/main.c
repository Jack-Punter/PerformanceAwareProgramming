#include <stdio.h>
#include <assert.h>
#include <malloc.h>
#include <stdint.h>
#include <string.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef i32 b32;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

#define Assert(c, m) assert(c)
#define OUTPUT_TO_STDIO 1

//- Arenas 
typedef struct _Arena {
    u8 *start;
    i64 offset;
    i64 capacity;
} Arena;

Arena MakeArena(i64 size) {
    Arena arena = {
        .start = malloc(size),
        .offset = 0,
        .capacity = size,
    };
    assert(arena.start);
    return arena;
}

void *ArenaPush_(Arena *a, i64 size, i64 count) {
    Assert(a, "No arena");
    Assert(a->offset + (size * count) < a->capacity, "Allocation Out of bounds");
    void *result = a->start + a->offset;
    a->offset += size * count;
    return result;
}

void ArenaClear(Arena *a) {
    a->offset = 0;
}

#define PushStructArray(a, T, n) ArenaPush_(a, sizeof(T), n);
#define PushStruct(a, T) PushStructArray(a, T, 1);

//- Strings 
typedef struct _MutableString {
    u8 *str;
    i64 size;
} MutableString;

typedef struct _String {
    const u8 *str;
    i64 size;
} String;

String StringFromMutable(MutableString s) {
    String result = {
        .str = s.str,
        .size = s.size,
    };
    
    return result;
}

#define StringLit(s) (String){ .str=(s), .size=strlen(s) }
#define StringConstLit(s) (String){ .str=(s), .size=sizeof(s) }
#define StringExpand(s) (int)s.size, s.str

String StringConcat(Arena * a, String s1, String s2) {
    MutableString result = { 0 };
    i64 new_size = s1.size + s2.size;
    // Allocate enough space for string + null terminato
    result.str = PushStructArray(a, u8, new_size + 1);
    result.size = new_size;
    
    u8 *dest = result.str;
    for (i64 i = 0; i < s1.size; ++i) {
        *dest++ = s1.str[i];
    }
    
    for (i64 i = 0; i < s2.size; ++i) {
        *dest++ = s2.str[i];
    }
    
    *dest = 0;
    return StringFromMutable(result);
}

//- Application 
int main(int argc, char *argv[])
{
    // This is for 8 byte registers
    // L suffix indicates low byte, H suffix indicates hight byte
    // NOTE(jack): High byte bitpattern sets the 3rd bit of the 
    // Low byte bitpattern.
    String W0RegisterNames[] = {
        [0x0] = StringLit("al"),
        [0x1] = StringLit("cl"),
        [0x2] = StringLit("dl"),
        [0x3] = StringLit("bl"),
        [0x4] = StringLit("ah"),
        [0x5] = StringLit("ch"),
        [0x6] = StringLit("dh"),
        [0x7] = StringLit("bh"),
    };
    
    String W1RegisterNames[] = {
        [0x0] = StringLit("ax"),
        [0x1] = StringLit("cx"),
        [0x2] = StringLit("dx"),
        [0x3] = StringLit("bx"),
        [0x4] = StringLit("sp"),
        [0x5] = StringLit("bp"),
        [0x6] = StringLit("si"),
        [0x7] = StringLit("di"),
    };
    
    // 1Gb arena for all our memory needs :D
    Arena arena = MakeArena(1 << 10);
    String LISTING_PREFIX = StringLit("computer_enhance/perfaware/part1/");
    String listing = StringLit("listing_0038_many_register_mov");
    String listing_path = StringConcat(&arena, LISTING_PREFIX, listing);
    
    printf("Decoding listing %.*s\n", StringExpand(listing_path));
    
    FILE *asm_file = fopen(listing_path.str, "rb");
    if (!asm_file) {
        printf("failed to open listing\n");
        return 1;
    }
    
    // Get the asm_filesize
    fseek(asm_file, 0, SEEK_END);
    i64 asm_file_size = ftell(asm_file);
    fseek(asm_file, 0, SEEK_SET);
    
    u8 *asm_buffer = PushStructArray(&arena, u8, asm_file_size);
    i64 read_size = fread(asm_buffer, 1, asm_file_size, asm_file);
    if (read_size != asm_file_size) {
        printf("Did not read the full asm_file size aborting\n");
        return 1;
    }
    
#ifdef OUTPUT_TO_STDIO 
    FILE *output = stdout;
#else
    // We will write our output direclty to this file.
    String output_file = StringConcat(&arena, listing, StringLit(".asm"));
    FILE *output = fopen(output_file.str, "w");
#endif
    
    fprintf(output, "bits 16\n\n");
    u8 *cursor = asm_buffer;
    while (cursor != asm_buffer + read_size) {
        // Check if the top 6 bits of the byte are 0b100010xx = 0x88. this indicates a 
        b32 is_mov = ((*cursor & 0xFC) ^ 0x88) == 0;
        b32 D = *cursor & 0x2;
        b32 W = *cursor & 0x1;
        if (is_mov) {
            cursor++;
            u8 r_m = *cursor & 0x7;
            u8 reg = (*cursor >> 0x3) & 0x7;
            u8 mod = (*cursor >> 0x6) & 0x3;
            Assert(mod == 3, "Should always be 0b11 for this listing");
            String *RegisterTable = W ? W1RegisterNames : W0RegisterNames;
            String Source = D ? RegisterTable[r_m] : RegisterTable[reg];
            String Dest = D ? RegisterTable[reg] : RegisterTable[r_m];
            fprintf(output, "mov\t%.*s,\t%.*s\n", StringExpand(Dest), StringExpand(Source));
        }
        
        cursor++;
    }
    
    fclose(output);
    return 0;
}