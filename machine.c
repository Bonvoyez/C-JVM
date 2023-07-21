#include <stdio.h>  // for getc, printf
#include <stdlib.h> // malloc, free
#include "ijvm.h"
#include "util.h" // read this file for debug prints, endianness helper functions


// see ijvm.h for descriptions of the below functions

FILE *in;   // use fgetc(in) to get a character from in.
            // This will return EOF if no char is available.
FILE *out;  // use for example fprintf(out, "%c", value); to print value to out

typedef struct
{
  uint32_t header;
  uint32_t constantPoolOrigin;
  uint32_t constantPoolSize;
  uint8_t *constantPool;
  uint32_t textOrigin;
  uint32_t textSize;
  uint8_t *text;
} FileContents;

FileContents fileContents = 
{
  .constantPool = NULL,
  .text = NULL
};

typedef struct 
{
  int *stack;
  int top;
  int programCounter;
  int size;
  int *size_track;
  int size_track_top;
  int *return_point;
  int return_point_top;
  int **variables_saved;
  int var_saved_top;
  int **arrays;
  int arraySize;
  int *gc;
  int gcSize;
  int const INDICATOR;
} Stack;

Stack stack = 
{
  .stack = NULL,
  .top = -1,
  .programCounter = 0,
  .size = 0,
  .size_track = NULL,
  .size_track_top = -1,
  .return_point = NULL,
  .return_point_top = -1,
  .arrays = NULL,
  .arraySize = 0,
  .gc = NULL,
  .gcSize = 0,
  .INDICATOR = 224 * 100000
};

Stack local_variables = 
{
  .stack = NULL,
  .size = 0,
  .variables_saved = NULL,
  .var_saved_top = -1
};

void set_input(FILE *fp)
{
  in = fp;
}

void set_output(FILE *fp)
{
  out = fp;
}

int init_ijvm(char *binary_path)
{
  in = stdin;
  out = stdout;

  FILE *file;

  file = fopen(binary_path, "rb");
  if (file == NULL)
  {
    return -1;
  }

  //header
  fread(&fileContents.header, sizeof(uint32_t), 1, file);
  fileContents.header = swap_uint32(fileContents.header);

  //constant pool
  fread(&fileContents.constantPoolOrigin, sizeof(uint32_t), 1, file);
  fileContents.constantPoolOrigin = swap_uint32(fileContents
  .constantPoolOrigin);
  fread(&fileContents.constantPoolSize, sizeof(uint32_t), 1, file);
  fileContents.constantPoolSize = swap_uint32(fileContents.constantPoolSize);
  fileContents.constantPool = (uint8_t*)malloc(fileContents.constantPoolSize 
  * sizeof(int8_t));
  fread(fileContents.constantPool, 1, fileContents.constantPoolSize, file);

  //text
  fread(&fileContents.textOrigin, sizeof(uint32_t), 1, file);
  fileContents.textOrigin = swap_uint32(fileContents.textOrigin);
  fread(&fileContents.textSize, sizeof(uint32_t), 1, file);
  fileContents.textSize = swap_uint32(fileContents.textSize);
  fileContents.text = (uint8_t*)malloc(fileContents.textSize * sizeof(int8_t));
  fread(fileContents.text, 1, fileContents.textSize, file);

  fclose(file);

  return 0;
}

void destroy_ijvm(void)
{
  free(fileContents.constantPool);
  free(fileContents.text);

  free(stack.stack);
  stack.programCounter = 0;
  stack.top = -1;
  stack.size = 0;
  free(stack.return_point);
  stack.return_point_top = -1;
  free(stack.size_track);
  stack.size_track_top = -1;
  for(int i = 0; i < stack.arraySize; i++)
  {
    if(stack.arrays[i] != NULL)
    {
      free(stack.arrays[i]);
    }
  }
  free(stack.arrays);
  stack.arraySize = 0;
  free(stack.gc);
  stack.gcSize = 0;

  free(local_variables.stack);
  local_variables.size = 0;
  free(local_variables.variables_saved);
  local_variables.var_saved_top = -1;
}

byte_t *get_text(void)
{
  return fileContents.text;
}

unsigned int get_text_size(void)
{   
  return fileContents.textSize;
}

word_t get_constant(int i)
{
  return read_uint32_t(&fileContents.constantPool[i * sizeof(uint32_t)]);
}

unsigned int get_program_counter(void)
{
  return stack.programCounter;
}

word_t tos(void)
{
  return stack.stack[stack.top];
}

bool finished(void)
{
  if(fileContents.textSize == stack.programCounter)
  {
    return true;
  }
  return false;
}

word_t get_local_variable(int i)
{
  return local_variables.stack[i];
}

void inc_stack_size(void)
{
  stack.size += 1;
  stack.top += 1;
  stack.stack = realloc(stack.stack, (stack.size) * sizeof(int));
}

void dec_stack_size(void)
{
  stack.top -= 1;
  stack.size -= 1;
  stack.stack = realloc(stack.stack, (stack.size) * sizeof(int));
}

short get_short_arg(void)
{
  return (short) ((fileContents.text[stack.programCounter + 1] 
  * 2 * 2 * 2 * 2 * 2 * 2 * 2 * 2) + fileContents.text[stack
  .programCounter + 2]);
}

void save_frame(int var_size)
{
  local_variables.var_saved_top += 1;
  local_variables.variables_saved = realloc(local_variables.variables_saved
  , (local_variables.var_saved_top + 1) * sizeof(int*));
  local_variables.variables_saved[local_variables.var_saved_top] 
  = (int*) malloc((var_size + 1) * sizeof(int));
  local_variables.variables_saved[local_variables.var_saved_top][0] = var_size;

  for(int i = 0; i < var_size; i++)
  {
    local_variables.variables_saved[local_variables.var_saved_top][i + 1] 
    = local_variables.stack[i];
  }
}

void restore_frame(void)
{
  local_variables.size = local_variables.variables_saved[local_variables
  .var_saved_top][0];
  local_variables.stack = realloc(local_variables.stack, local_variables.size 
  * sizeof(int));
  

  for(int i = 0; i < local_variables.size; i++)
  {
    local_variables.stack[i] = local_variables.variables_saved[local_variables
    .var_saved_top][i + 1];
  }
  free(local_variables.variables_saved[local_variables.var_saved_top]);
  local_variables.variables_saved = realloc(local_variables.variables_saved
  , local_variables.var_saved_top * sizeof(int*));

  local_variables.var_saved_top -= 1;
  
}

int get_location(int i)
{
  int index = fileContents.text[stack.programCounter + 1];
  const int MULTIPLIER = 256;

  if(i >= 1)
  {
    index += i * MULTIPLIER;
  }

  return index;
}

bool check_stack(int i)
{
  for(int j = 0; j < stack.size; j++)
  {
    if(stack.gc[i] == stack.stack[j])
    {
      return true;
    }
  }
  return false;
}

bool check_variables(int i)
{
  for(int j = 0; j < local_variables.size; j++)
  {
    if(stack.gc[i] == local_variables.stack[j])
    {
      return true;
    }
  }
  return false;
}

bool check_saved_var(int i)
{
  for(int j = 0; j < local_variables.var_saved_top + 1; j++)
  {
    for(int k = 0; k < local_variables.variables_saved[j][0] + 1; k++)
    {
      if(stack.gc[i] == local_variables.variables_saved[j][k])
      {
        return true;
      }
    }
  }
  return false;
}

bool check_arrays(int i, int cycle_check[])
{
  for(int j = 0; j < stack.arraySize; j++)
  {
    if(stack.arrays[j] != NULL)
    {
      for(int k = 0; k < stack.arrays[j][0] + 1; k++)
      {
        if(stack.gc[i] == stack.arrays[j][k])
        {
          cycle_check[j] = 1;
          return true;
        }
      }
    }
  }
  return false;
}

void bipush_func(void)
{
  inc_stack_size();
  stack.stack[stack.top] = (int8_t) fileContents.text[stack
  .programCounter + 1];
  stack.programCounter += 2;
}

void dup_func(void)
{
  if(stack.size >= 1)
    {
      inc_stack_size();
      stack.stack[stack.top] = stack.stack[stack.top - 1];
    }
    stack.programCounter += 1;
}

void iadd_func(void)
{
  int calculated = 0;

  if(stack.size >= 2)
    {
      calculated = stack.stack[stack.top] + stack.stack[stack.top - 1];
      dec_stack_size();
      stack.stack[stack.top] = calculated;
    }
    stack.programCounter += 1;
}

void iand_func(void)
{
  int calculated = 0;

  if(stack.size >= 2)
    {
      calculated = stack.stack[stack.top] & stack.stack[stack.top - 1];
      dec_stack_size();
      stack.stack[stack.top] = calculated;
    }
    stack.programCounter += 1;
}

void ior_func(void)
{
  int calculated = 0;

  if(stack.size >= 2)
    {
      calculated = stack.stack[stack.top] | stack.stack[stack.top - 1];
      dec_stack_size();
      stack.stack[stack.top] = calculated;
    }
    stack.programCounter += 1;
}

void isub_func(void)
{
  int calculated = 0;

  if(stack.size >= 2)
    {
      calculated = stack.stack[stack.top - 1] - stack.stack[stack.top];
      dec_stack_size();
      stack.stack[stack.top] = calculated;
    }
    stack.programCounter += 1;
}

void pop_func(void)
{
  if(stack.size >= 1)
    {
      dec_stack_size();
    }
    stack.programCounter += 1;
}

void swap_func(void)
{
  int top = 0;

  if(stack.size >= 2)
    {
      top = stack.stack[stack.top];
      stack.stack[stack.top] = stack.stack[stack.top - 1];
      stack.stack[stack.top - 1] = top;
    }
    stack.programCounter += 1;
}

void in_func(void)
{
  inc_stack_size();
  int input = fgetc(in);
  if(input == -1)
  {
    stack.stack[stack.top] = 0;
  }
  else
  {
    stack.stack[stack.top] = input;
  }
  
  stack.programCounter += 1;
}

void out_func(void)
{
  if(stack.size >= 1)
    {
      fprintf(out, "%c", stack.stack[stack.top]);
      dec_stack_size();
    }
    stack.programCounter += 1;
}

void goto_func(void)
{
  stack.programCounter += get_short_arg();
}

void ifeq_func(void)
{
  if(stack.size >= 1)
  {
    if(stack.stack[stack.top] == 0)
    {
      stack.programCounter += get_short_arg();
    }
    else
    {
      stack.programCounter += 3;
    }
    dec_stack_size();
  }
}

void iflt_func(void)
{
  if(stack.size >= 1)
  {
    if(stack.stack[stack.top] < 0)
    {
      stack.programCounter += get_short_arg();
    }
    else
    {
      stack.programCounter += 3;
    }
    dec_stack_size();
  }
}

void if_icmpeq_func(void)
{
  if(stack.size >= 1)
  {
    if(stack.stack[stack.top] == stack.stack[stack.top - 1])
    {
      stack.programCounter += get_short_arg();
    }
    else
    {
      stack.programCounter += 3;
    }
    dec_stack_size();
    dec_stack_size();
  }
}

void ldc_w_func(void)
{
  inc_stack_size();
  stack.stack[stack.top] = get_constant(get_short_arg());
  stack.programCounter += 3;
}

void iload_func(int i)
{
  int index = get_location(i);

  if(local_variables.size > index)
  {
    inc_stack_size();
    stack.stack[stack.top] = get_local_variable(index);
  }

  stack.programCounter += 2;
}

void istore_func(int i)
{
  int index = get_location(i);

  if(stack.size >= 1)
  {
    if(local_variables.size < index + 1)
    {
      local_variables.size = index + 1;
      local_variables.stack = realloc(local_variables.stack
      , (local_variables.size) * sizeof(int));
    }

    local_variables.stack[index] 
    = stack.stack[stack.top];
    dec_stack_size();
  }

  stack.programCounter += 2;
}

void iinc_func(int i)
{
  int index = get_location(i);

  if(local_variables.size > index)
  {
    local_variables.stack[index]
    += (int8_t) fileContents.text[stack.programCounter + 2];
  }
  stack.programCounter += 3;
}

void wide_func(void)
{
  stack.programCounter += 1;
  int size = fileContents.text[stack.programCounter + 1];

  switch(fileContents.text[stack.programCounter])
  {
    case 0x15: //ILOAD
      stack.programCounter += 1;
      iload_func(size);
      break;
    case 0x36: //ISTORE
      stack.programCounter += 1;
      istore_func(size);
      break;
    case 0x84: //IINC
      stack.programCounter += 1;
      iinc_func(size);
      break;
    default:
      break;
  }
}

void invokevirtual_func(void)
{ 
  save_frame(local_variables.size);

  stack.return_point_top += 1;
  stack.return_point = realloc(stack.return_point
  , (stack.return_point_top + 1) * sizeof(int));
  stack.return_point[stack.return_point_top] = stack.programCounter + 3;
  
  stack.programCounter = get_constant((int) get_short_arg()) - 1;
  int arg_size = get_short_arg();
  stack.programCounter += 2;
  
  local_variables.size = arg_size;
  if(local_variables.size < get_short_arg())
  {
    local_variables.size = get_short_arg();
  }
  local_variables.stack = realloc(local_variables.stack
  , (local_variables.size) * sizeof(int));
  
  for(int i = arg_size; i > 0; i--)
  {
    local_variables.stack[arg_size - i] = stack.stack[stack.top - i + 1];
  }
  stack.size -= arg_size;
  stack.top = stack.size - 1;
  stack.stack = realloc(stack.stack, stack.size * sizeof(int));

  stack.size_track_top += 1;
  stack.size_track = realloc(stack.size_track, (stack.size_track_top + 1)
  * sizeof(int));
  stack.size_track[stack.size_track_top] = stack.size;

  stack.programCounter = stack.programCounter + 3;
}

void ireturn_func(void)
{
  if(stack.size_track_top >= 0)
  {
    stack.programCounter = stack.return_point[stack.return_point_top];
    stack.return_point = realloc(stack.return_point
    , stack.return_point_top * sizeof(int));
    stack.return_point_top -= 1;

    int return_value = stack.stack[stack.top];
    stack.size = stack.size_track[stack.size_track_top] + 1;
    stack.top = stack.size - 1;
    stack.stack = realloc(stack.stack, stack.size * sizeof(int));
    stack.stack[stack.top] = return_value;
    stack.size_track = realloc(stack.size_track, (stack.size_track_top)
    * sizeof(int));
    stack.size_track_top -= 1;

    restore_frame();
  }
}

void tailCall_func(void)
{
  stack.programCounter = get_constant((int) get_short_arg()) - 1;
  int arg_size = get_short_arg();
  stack.programCounter += 2;

  local_variables.size = arg_size;
  if(local_variables.size < get_short_arg())
  {
    local_variables.size = get_short_arg();
  }
  local_variables.stack = realloc(local_variables.stack
  , (local_variables.size) * sizeof(int));
  
  for(int i = arg_size; i > 0; i--)
  {
    local_variables.stack[arg_size - i] = stack.stack[stack.top - i + 1];
  }
  stack.size -= arg_size;
  stack.top = stack.size - 1;
  stack.stack = realloc(stack.stack, stack.size * sizeof(int));

  stack.programCounter = stack.programCounter + 3;
}

void newArray_func(void)
{
  if(stack.size >= 1)
  {
    int count = stack.stack[stack.top];
    int arrayRef = stack.arraySize;
    
    stack.arraySize += 1;
    stack.arrays = realloc(stack.arrays, stack.arraySize * sizeof(int*));
    
    stack.arrays[arrayRef] = (int*) malloc((count + 1) * sizeof(int));
    stack.arrays[arrayRef][0] = count;
    arrayRef = stack.INDICATOR + arrayRef;
    stack.stack[stack.top] = arrayRef;

    stack.gcSize += 1;
    stack.gc = realloc(stack.gc, stack.gcSize * sizeof(int));
    stack.gc[stack.gcSize - 1] = arrayRef;
    
    stack.programCounter += 1;
  }
}

void iaload_func(void)
{
  if(stack.size >= 2)
  {
    int arrayRef = stack.stack[stack.top] - stack.INDICATOR;
    int index = stack.stack[stack.top - 1];
    dec_stack_size();
    dec_stack_size();
    
    if(stack.arrays[arrayRef][0] < index + 1)
    {
      fprintf(stderr, "ERROR\n");
      stack.programCounter = fileContents.textSize;
      return;
    }
    
    inc_stack_size();
    stack.stack[stack.top] = stack.arrays[arrayRef][index + 1];
    
    stack.programCounter += 1;
  }
}

void iastore_func(void)
{
  if(stack.size >= 3)
  {
    int arrayRef = stack.stack[stack.top] - stack.INDICATOR;
    int index = stack.stack[stack.top - 1];
    int value = stack.stack[stack.top - 2];
    dec_stack_size();
    dec_stack_size();
    dec_stack_size();
    
    if(stack.arrays[arrayRef][0] < index + 1)
    {
      fprintf(stderr, "ERROR\n");
      stack.programCounter = fileContents.textSize;
      return;
    }
    
    stack.arrays[arrayRef][index + 1] = value;
    
    stack.programCounter += 1;
  }
}

void gc_func(void)
{
  bool found = false;
  int cycle_check[stack.gcSize][stack.gcSize];

  for(int i = 0; i < stack.gcSize; i++)
  {
    if(stack.gc[i] == 0)
    {
      continue;
    }
    found = check_stack(i);

    if(found == false)
    {
      found = check_variables(i);
    }
    if(found == false)
    {
      found = check_saved_var(i);
    }
    if(found == false)
    {
      found = check_arrays(i, cycle_check[i]);
    }
    if(found == false)
    {
      free(stack.arrays[stack.gc[i] - stack.INDICATOR]);
      stack.arrays[stack.gc[i] - stack.INDICATOR] = NULL;
      stack.gc[i] = 0;
    }
    found = false;
  }
  for(int i = 0; i < stack.gcSize; i++)
  {
    for(int j = 0; j < stack.gcSize; j++)
    {
      if(cycle_check[i][j] == 1 && cycle_check[j][i] == 1 && stack.gc[j] != 0
      && stack.gc[i] != 0 && i != j)
      {
        free(stack.arrays[stack.gc[i] - stack.INDICATOR]);
        stack.arrays[stack.gc[i] - stack.INDICATOR] = NULL;
        stack.gc[i] = 0;
        free(stack.arrays[stack.gc[j] - stack.INDICATOR]);
        stack.arrays[stack.gc[j] - stack.INDICATOR] = NULL;
        stack.gc[j] = 0;
        cycle_check[i][j] = 0;
        cycle_check[j][i] = 0;
      }
    }
  }
  stack.programCounter += 1;
}

void step(void)
{
  if(stack.programCounter == 0)
  {
    stack.stack = (int*) malloc(0 * sizeof(int));
    local_variables.stack = (int*) malloc(0 * sizeof(int));
    stack.return_point = (int*) malloc(0 * sizeof(int));
    stack.size_track = (int*) malloc(0 * sizeof(int));
    local_variables.variables_saved = (int**) malloc(0  * sizeof(int*));
    stack.arrays = (int**) malloc(0  * sizeof(int*));
    stack.gc = (int*) malloc(0 * sizeof(int));
  }
  
  switch(fileContents.text[stack.programCounter])
  {
    case 0x10: //BIPUSH
      bipush_func();
      break;
    case 0x59: //DUP
      dup_func();
      break;
    case 0x60: //IADD
      iadd_func();
      break;
    case 0x7E: //IAND
      iand_func();
      break;
    case 0xB0: //IOR
      ior_func();
      break;
    case 0x64: //ISUB
      isub_func();
      break;
    case 0x00: //NOP
      stack.programCounter += 1;
      break;
    case 0x57: //POP
      pop_func();
      break;
    case 0x5F: //SWAP
      swap_func();
      break;
    case 0xFE: //ERR
      fprintf(stderr, "ERROR\n");
      stack.programCounter += fileContents.textSize;
      break;
    case 0xFF: //HALT
      stack.programCounter = fileContents.textSize;
      break;
    case 0xFC: //IN
      in_func();
      break;
    case 0xFD: //OUT
      out_func();
      break;
    case 0xA7: //GOTO
      goto_func();
      break;
    case 0x99: //IFEQ
      ifeq_func();
      break;
    case 0x9B: //IFLT
      iflt_func();
      break;
    case 0x9F: //IF_ICMPEQ
      if_icmpeq_func();
      break;
    case 0x13: //LDC_W
      ldc_w_func();
      break;
    case 0x15: //ILOAD
      iload_func(0);
      break;
    case 0x36: //ISTORE
      istore_func(0);
      break;
    case 0x84: //IINC
      iinc_func(0);
      break;
    case 0xC4: //WIDE
      wide_func();
      break;
    case 0xB6: //INVOKEVIRTUAL
      invokevirtual_func();
      break;
    case 0xAC: //IRETURN
      ireturn_func();
      break;
    case 0xCB: //TAILCALL
      tailCall_func();
      break;
    case 0xD1: // NEWARRAY
      newArray_func();
      break;
    case 0xD2: // IALOAD
      iaload_func();
      break;
    case 0xD3: // IASTORE
      iastore_func();
      break;
    case 0xD4: //GC
      gc_func();
      break;
    default:
      break;
  }
}

void run(void)
{
  while (!finished())
  {
    step();
  }
}

byte_t get_instruction(void)
{
  return get_text()[get_program_counter()];
}

// Below: methods needed by bonus assignments, see ijvm.h

int get_call_stack_size(void)
{
  return stack.size_track_top;
}


// Checks if reference is a freed heap array. Note that this assumes that
//
bool is_heap_freed(word_t reference)
{
  if(stack.arrays[reference - stack.INDICATOR] == NULL)
  {
    return true;
  }
  return false;
}
