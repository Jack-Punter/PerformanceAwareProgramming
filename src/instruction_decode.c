typedef u8 *ByteIterator;

typedef b32 (*InstructionDecodeFunc)(Arena *, FILE*, ByteIterator*);

typedef struct {
    u8 mask;
    u8 bits;
    InstructionDecodeFunc func;
} Instruction;

// NOTE(jack): I don't think im totally happy with how these functions
// use the pCursor. Currently they need to leave the cursor on the 
// last byte of the instruction. it might be better to advance to the
// start of the next instruction in thesse, and remove the extra increment
// in the outer loop?
b32 DecodeMov       (Arena *arena, FILE *out, ByteIterator* pCursor);
b32 DecodeImm2Reg   (Arena *arena, FILE *out, ByteIterator* pCursor);
b32 DecodeImm2RegMem(Arena *arena, FILE *out, ByteIterator* pCursor);
b32 DecodeAcc2Memory(Arena *arena, FILE *out, ByteIterator* pCursor);
b32 DecodeMemory2Acc(Arena *arena, FILE *out, ByteIterator* pCursor);

#define MOV_MASK 0b11111100
#define MOV_BITS 0b10001000

#define IMM2REG_MASK 0b11110000
#define IMM2REG_BITS 0b10110000

#define IMM2REGMEM_MASK 0b11111110
#define IMM2REGMEM_BITS 0b11000110

#define ACC2MEM_MAKS 0b11111110
#define ACC2MEM_BITS 0b10100010

#define MEM2ACC_MAKS 0b11111110
#define MEM2ACC_BITS 0b10100000


Instruction instructions[] = {
    { .mask = MOV_MASK,        .bits = MOV_BITS,        .func = &DecodeMov },
    { .mask = IMM2REG_MASK,    .bits = IMM2REG_BITS,    .func = &DecodeImm2Reg },
    { .mask = IMM2REGMEM_MASK, .bits = IMM2REGMEM_BITS, .func = &DecodeImm2RegMem },
    { .mask = ACC2MEM_MAKS,    .bits = ACC2MEM_BITS,    .func = &DecodeAcc2Memory },
    { .mask = MEM2ACC_MAKS,    .bits = MEM2ACC_BITS,    .func = &DecodeMemory2Acc },
};

void PrintInstruction_2(FILE *out, String inst, String dest, String source) {
    fprintf(out, "%.*s\t%.*s,\t%.*s\n", StringExpand(inst),
            StringExpand(dest), StringExpand(source));
}


String FormatDisplacmentArgFromMod(Arena *arena, u8 mod, u8 r_m, const String *register_name_table,
                                   String effective_address_eq, ByteIterator *pCursor)
{
    ByteIterator cursor = *pCursor;
    String result = { 0 };
    
    //- Do the mod shiz to extract DISP-LO and DISP-HI if needed
    switch(mod)
    {
        case 0x0:
        {
            if (r_m == 0x6)
            {
                // NOTE(jack): I assume you always use DISP-LOW and DISP-HIGH in direct address?
                // As the memory space is always 64k
                cursor++;
                u16 addr = *(u16*)cursor;
                pCursor++;
                result = PushStringf(arena, "[%hd]", addr);
            }
            else
            {
                result = PushStringf(arena, "[%.*s]", StringExpand(effective_address_eq));
            }
        } break;
        
        case 0x1: {
            cursor++;
            i8 data = *cursor;
            if (data == 0) {
                result = PushStringf(arena, "[%.*s]", StringExpand(effective_address_eq));
            }
            else if (data > 0) {
                result = PushStringf(arena, "[%.*s + %hhd]", StringExpand(effective_address_eq), data);
            }
            else {
                result = PushStringf(arena, "[%.*s - %hhd]", StringExpand(effective_address_eq), -data);
            }
        } break;
        
        case 0x2: {
            // increment the cursor then read 2 bytes.
            cursor++;
            i16 data = *(u16*)cursor;
            // leave the cursor pointing at the last read byte
            cursor++;
            
            if (data == 0) {
                result = PushStringf(arena, "[%.*s]", StringExpand(effective_address_eq));
            }
            else if (data > 0) {
                result = PushStringf(arena, "[%.*s + %hd]", StringExpand(effective_address_eq), data);
            }
            else {
                result = PushStringf(arena, "[%.*s - %hd]", StringExpand(effective_address_eq), -data);
            }
        } break;
        
        case 0x3: { // Register to Register Mov
            // If d is set, the reg field is the destination
            result = register_name_table[r_m];
        } break;
    }
    *pCursor = cursor;
    return result;
}


b32 DecodeMov(Arena *arena, FILE *out, ByteIterator* pCursor)
{
    ByteIterator cursor = *pCursor;
    
    b32 d = *cursor & 0x2;
    b32 is_wide = *cursor & 0x1;
    const String *register_name_table = is_wide ? wide_register_table : narrow_register_table;
    
    cursor++;
    u8 r_m = *cursor & 0x7;
    u8 reg = (*cursor >> 0x3) & 0x7;
    u8 mod = (*cursor >> 0x6) & 0x3;
    
    String source = { 0 };
    String dest = { 0 };
    
    // The d flag indicates whether the reg field is the destination or source
    String *rm_dest;
    if (d) {
        dest = register_name_table[reg];
        rm_dest = &source;
    } else {
        source = register_name_table[reg];
        rm_dest = &dest;
    }
    
    String effective_address_eq = effective_address_eq_table[r_m];
    b32 decoded = False;
    *rm_dest = FormatDisplacmentArgFromMod(arena, mod, r_m, register_name_table,
                                           effective_address_eq, &cursor);
    
    PrintInstruction_2(out, StringLit("mov"), dest, source);
    
    *pCursor = cursor;
    return True;
}

b32 DecodeImm2Reg(Arena *arena, FILE *out, ByteIterator* pCursor)
{
    ByteIterator cursor = *pCursor;
    
    b32 is_wide   = *cursor & 0b00001000;
    u8  reg = *cursor & 0b00000111;
    
    const String *register_name_table = is_wide ? wide_register_table : narrow_register_table;
    String dest = register_name_table[reg];
    
    cursor++;
    String source;
    if (!is_wide) {
        i8 data = *cursor;
        source = PushStringf(arena, "%hhd", data);
    } else {
        i16 data = *(u16*)cursor;
        source = PushStringf(arena, "%hd", data);
        cursor++;
    }
    
    PrintInstruction_2(out, StringLit("mov"), dest, source);
    
    *pCursor = cursor;
    return True;
}

b32 DecodeImm2RegMem(Arena *arena, FILE *out, ByteIterator* pCursor) {
    
    ByteIterator cursor = *pCursor;
    b32 is_wide = *cursor & 0x1;
    const String *register_name_table = is_wide ? wide_register_table : narrow_register_table;
    
    // NOTE(jack): Dest will be determined by mod (probably big switch case like in DecodeMov
    String dest = { 0 };
    String source = { 0 };
    
    cursor++;
    u8 r_m = *cursor & 0x7;
    u8 mod = (*cursor >> 0x6) & 0x3;
    String effective_address_eq = effective_address_eq_table[r_m];
    
    dest = FormatDisplacmentArgFromMod(arena, mod, r_m, register_name_table,
                                       effective_address_eq, &cursor);
    
    // Get the data
    cursor++;
    if (is_wide) {
        u16 imm = *(u16*)cursor;
        cursor++;
        source = PushStringf(arena, "word %hd", imm);
    } else {
        u8 imm = *(u8*)cursor;
        source = PushStringf(arena, "byte %hhd", imm);
    }
    
    // fprintf(out, "DecodeImm2RegMem not yet implemented\n");
    PrintInstruction_2(out, StringLit("mov"), dest, source);
    *pCursor = cursor;
    return True;
}

b32 DecodeMemory2Acc(Arena *arena, FILE *out, ByteIterator* pCursor)
{
    ByteIterator cursor = *pCursor;
    b32 is_wide = *cursor & 0x1;
    const String *register_name_table = is_wide ? wide_register_table : narrow_register_table;
    
    cursor++;
    u16 addr = *(u16*)cursor;
    cursor++;
    // The accumulator "ax" and accumulator_low "al" is at location 0 of the table
    PrintInstruction_2(out, StringLit("mov"), register_name_table[0],
                       PushStringf(arena, "[%hu]", addr));
    
    // fprintf(out, "DecodeMemory2Acc not yet implemented\n");
    *pCursor = cursor;
    return True;
}

b32 DecodeAcc2Memory(Arena *arena, FILE *out, ByteIterator* pCursor)
{
    ByteIterator cursor = *pCursor;
    b32 is_wide = *cursor & 0x1;
    const String *register_name_table = is_wide ? wide_register_table : narrow_register_table;
    
    cursor++;
    u16 addr = *(u16*)cursor;
    cursor++;
    // The accumulator "ax" and accumulator_low "al" is at location 0 of the table
    PrintInstruction_2(out, StringLit("mov"), PushStringf(arena, "[%hu]", addr), 
                       register_name_table[0]);
    
    *pCursor = cursor;
    return True;
}

