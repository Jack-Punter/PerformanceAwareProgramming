#include <stdio.h>
#include <assert.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef i32 b32;
#define False 0
#define True 1

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

#define ArrayCount(arr) sizeof(arr) / sizeof(arr[0])
#define Assert(c, m) assert(c)
#define OUTPUT_TO_STDIO 1

//- @Arenas 
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
#define MakeStaticArena(array) { .start = array, .capacity = ArrayCount(array) }

void *ArenaPush_(Arena *a, i64 size, i64 count) {
    Assert(a, "No arena");
    Assert(a->offset + (size * count) < a->capacity, "Allocation Out of bounds");
    void *result = a->start + a->offset;
    a->offset += size * count;
    memset(result, 0, size * count);
    return result;
}

void ArenaClear(Arena *a) {
    a->offset = 0;
}

#define PushStructArray(a, T, n) ArenaPush_(a, sizeof(T), n)
#define PushStruct(a, T) PushStructArray(a, T, 1)

//- @Strings 
typedef struct MutableString {
    u8 *str;
    i64 size;
} MutableString;

typedef struct String {
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
#define StringLitConst(s) { .str=(s), .size=sizeof(s) }
#define StringExpand(s) (int)(s).size, (s).str

String StringConcat(Arena *a, String s1, String s2) {
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

#define PushStringCopy(areana, str) _Generic((str), \
String: PushStringCopy_, \
MutableString: PushStringCopyFromMutable \
)(areana, str)

String PushStringCopy_(Arena *arena, String s) {
    MutableString result = {
        .str = PushStructArray(arena, u8, s.size),
        .size = s.size,
    };
    
    memcpy(result.str, s.str, s.size);
    return StringFromMutable(result);
}

String PushStringCopyFromMutable(Arena *arena, MutableString s) {
    return PushStringCopy_(arena, StringFromMutable(s));
}

String PushStringf(Arena *arena, const char *format, ...) {
    u8 buffer[512] = { 0 };
    MutableString string = { 
        .str = buffer,
        .size = 0,
    };
    
    va_list args;
    va_start(args, format);
    string.size = vsnprintf(buffer, 512, format, args);
    va_end(args);
    
    String result = PushStringCopy(arena, string);
    return result;
}


//~ @Application

//- @lookup_tables 
// 'l' suffix indicates low byte, 'h' suffix indicates hight byte
// NOTE(jack): High byte bitpattern sets the 3rd bit of the 
// Low byte bitpattern.

// This is for 8 byte registers
String narrow_register_table[] = {
    [0b000] = StringLitConst("al"),
    [0b001] = StringLitConst("cl"),
    [0b010] = StringLitConst("dl"),
    [0b011] = StringLitConst("bl"),
    [0b100] = StringLitConst("ah"),
    [0b101] = StringLitConst("ch"),
    [0b110] = StringLitConst("dh"),
    [0b111] = StringLitConst("bh"),
};

String wide_register_table[] = {
    [0b000] = StringLitConst("ax"),
    [0b001] = StringLitConst("cx"),
    [0b010] = StringLitConst("dx"),
    [0b011] = StringLitConst("bx"),
    [0b100] = StringLitConst("sp"),
    [0b101] = StringLitConst("bp"),
    [0b110] = StringLitConst("si"),
    [0b111] = StringLitConst("di"),
};

String effective_address_eq_table[] = {
    [0b000] = StringLitConst("bx + si"),
    [0b001] = StringLitConst("bx + di"),
    [0b010] = StringLitConst("bp + si"),
    [0b011] = StringLitConst("bp + di"),
    [0b100] = StringLitConst("si"),
    [0b101] = StringLitConst("di"),
    [0b110] = StringLitConst("bp"),
    [0b111] = StringLitConst("bx"),
};

#include "instruction_decode.c"

// NOTE(jack): Currently this is unused
u8 Memory[65536];

int main(int argc, char *argv[])
{
    // TODO(jack): This reminds me, I should really make myself a "standard"
    // library, including a virtual memory arena, for all the platforms.
    // 1Gb arena for all our memory needs :D
    Arena arena = MakeArena(1 << 10);
    
    String LISTING_PREFIX = StringLit("computer_enhance/perfaware/part1/");
    String listing = StringLit("listing_0040_challenge_movs");
    
    String listing_path = StringConcat(&arena, LISTING_PREFIX, listing);
    
    fprintf(stderr, "Decoding listing %.*s\n", StringExpand(listing_path));
    
    FILE *asm_file = fopen(listing_path.str, "rb");
    if (!asm_file) {
        fprintf(stderr, "failed to open listing\n");
        return 1;
    }
    
    // Get the file
    fseek(asm_file, 0, SEEK_END);
    i64 asm_file_size = ftell(asm_file);
    fseek(asm_file, 0, SEEK_SET);
    
    u8 *asm_buffer = PushStructArray(&arena, u8, asm_file_size);
    i64 read_size = fread(asm_buffer, 1, asm_file_size, asm_file);
    if (read_size != asm_file_size) {
        fprintf(stderr, "Did not read the full asm_file size aborting\n");
        return 1;
    }
    
#ifdef OUTPUT_TO_STDIO 
    FILE *output = stdout;
#else
    // We will write our output direclty to this file.
    String output_file = StringConcat(&arena, listing, StringLit(".asm"));
    FILE *output = fopen(output_file.str, "w");
#endif
    
    // NOTE(jack): outputting this so that we could theoretically re-assemble our
    // dissasembly and validate the output is correct.
    fprintf(output, "bits 16\n\n");
    
    // For now we know the binary is well formed so we should always never overrun
    // whilst decoding an instruction
    u8 scratch_buffer[1024] = { 0 };
    Arena scratch = MakeStaticArena(scratch_buffer);
    for(ByteIterator cursor = asm_buffer;
        cursor < asm_buffer + read_size;
        cursor += 1)
    {
        // Loop through the registered instructions and decode them
        for (Instruction *inst = instructions;
             inst != instructions + ArrayCount(instructions);
             inst++)
        {
            if ((*cursor & inst->mask) == inst->bits) {
                // TODO(jack): Maybe check result here?
                b32 result = inst->func(&scratch, output, &cursor);
                Assert(result, "The function failed to decode");
                break;
            }
        }
        
        ArenaClear(&scratch);
    }
    
    fclose(output);
    return 0;
}