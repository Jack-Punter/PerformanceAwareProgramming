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
    assert(a);
    assert(a->offset + (size * count) < a->capacity);
    void *result = a->start + a->offset;
    a->offset += size * count;
    return result;
}

void ArenaClear(Arena *a) {
    a->offset = 0;
}

#define PushStructArray(a, T, n) ArenaPush_(a, sizeof (T), n);
#define PushStruct(a, T) PushStructArray(a, T, 1);

//- Strings 
typedef struct _String {
    u8 *str;
    i64 size;
} String;

#define StringLit(s) (String){ .str=(s), .size=strlen(s) }
#define StringExpand(s) (int)s.size, s.str

String StringConcat(Arena * a, String s1, String s2) {
    String result = { 0 };
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
    return result;
}



u8 InstructionPrefixes[] = {
    [0b10001000]
};


int main(int argc, char *argv[]) {
    // 1Gb arena for all our memory needs :D
    Arena arena = MakeArena(1 << 10);
    String LISTING_PREFIX = StringLit("computer_enhance/perfaware/part1/");
    String listing = StringLit("listing_0037_single_register_mov");
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
    
    String output_file = StringConcat(&arena, listing, StringLit(".asm"));
    // We will write our output direclty to this file.
    FILE *output = fopen(output_file.str, "w");
    
    u8 *cursor = asm_buffer;
    while (cursor <= asm_buffer + read_size) {
        bool is_mov = ((*cursor & 0xFC) ^ 0x88) == 0;
        
    }
    
    fclose(output);
    return 0;
}