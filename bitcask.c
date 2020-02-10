#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 
 * Data layout of the segment file
 */
//need to reserve an extra char for null terminator?
#define KEY_NUM_BYTES 32
#define VALUE_NUM_BYTES 25

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 * In-memory hash table
 * Write some tests explicitly for this.
 * Don't need to use a fancy framework, just make them automatic to run.
 */
typedef struct Node_t Node;
struct Node_t {
  int key;
  off_t offset;
  //char value[VALUE_NUM_BYTES];  // this is probably not what we want right?
  Node* next_node;
};

#define MULT 31  //effective multiplier (according to Bentley)
#define NHASH 101  //prime near hash table capacity
unsigned int hash(int key) {
  /* homemade shitty hash function */
  unsigned int h = 1;
  for (; key; key >>= 1) {
    h = MULT * h + (key & 1);
  }
  return h % NHASH;
}

/*
 * Returns pointer to array of pointers to linked lists.
 *
 * Since values desribe offsets in a file,
 * -1 is an invalid value
 * and can therefore serve as a sentinal value
 * for an unoccupied node
 */
Node** create_hashmap() {
  Node** array = (Node**)malloc(NHASH * sizeof(Node*));
  for (int i = 0; i < NHASH; i++) {
    array[i] = (Node*)malloc(sizeof(Node));
    array[i]->next_node = NULL;
    
    //strcpy(array[i]->offset, "-1");
    array[i]->offset = -1;
  }
  return array;
}

/*
 * This function returns either the node
 * corresponding to the key,
 * or the last node in the list in the bucket
 * corresponding to the key. 
 *
 * In the latter case,
 * the calling function has to allocate
 * a new node, and store the key/value inside.
 */
Node* get_bucket(Node** hashmap, int key) {
  unsigned int h = hash(key);
  //printf("hash: %d\n", h);
  Node* bucket_node = hashmap[h];
  
  // is this right? what if the bucket's key doesn't match but next
  // node is null?
  // it's ok, think we check in insert_hashmap
  while ((bucket_node->next_node != NULL) &&
      (bucket_node->key != key)) {
    bucket_node = bucket_node->next_node;
  }
  
  return bucket_node;
}

void insert_hashmap(Node** hashmap, int key, off_t offset) {
  //printf("insert_hashmap called with: key=%d, offset=%d\n", key, offset);
  Node* bucket_node = get_bucket(hashmap, key);
  
  if (bucket_node->offset == -1) {
    /* Do we need these two lines of code here? */
    bucket_node->key = key;
    bucket_node->offset = offset;
    //printf("bucket[key=%d, offset=%d]\n", key, offset);
    return;
  }
  
  if (bucket_node->key != key) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    bucket_node->next_node = new_node;
    new_node->key = key;
    bucket_node->offset = offset;
    new_node->next_node = (Node*)malloc(sizeof(Node));
    return;
  }
  
  bucket_node->key = key;
  bucket_node->offset = offset;
  bucket_node->next_node = (Node*)malloc(sizeof(Node));
  //printf("bucket[key=%d, offset=%d]\n", key, offset);
}
 
off_t get_from_hashmap(Node** hashmap, int key) {
  Node* bucket_node = get_bucket(hashmap, key);
  if (bucket_node == NULL || (bucket_node->offset == -1)) {
    return -1;
  }
  return bucket_node->offset;
}

void cleanup_hashmap() {
  //TODO: free all pointers
  printf("You forgot to cleanup\n");
}

/* End of in-memory hash table.
 *  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct InputBuffer_t {
  char* buffer;
  size_t buffer_length;
  ssize_t input_length;
};
typedef struct InputBuffer_t InputBuffer;

InputBuffer* new_input_buffer() {
  //allocate and initialize
  InputBuffer* input_buffer = malloc(sizeof(InputBuffer));
  input_buffer->buffer = NULL;
  input_buffer->buffer_length = 0;
  input_buffer->input_length = 0;
  return input_buffer;
}

struct KeyValue_t {
  int key;
  char value[VALUE_NUM_BYTES];
};
typedef struct KeyValue_t KeyValue;

enum ProcessResult_t {
  PROCESS_SUCCESS,
  PROCESS_ERROR,
};
typedef enum ProcessResult_t ProcessResult;

enum ExecuteResult_t {
  EXECUTE_SUCCESS,
  EXECUTE_ERROR,
};
typedef enum ExecuteResult_t ExecuteResult;

enum CommandType_t {
  SET,
  GET
};
typedef enum CommandType_t CommandType;

struct Command_t {
  CommandType type;
  KeyValue keyvalue;
};
typedef struct Command_t Command;

off_t append_to_segment(KeyValue keyvalue) {
  /* for now, keep appending to the same file 
   * get the location where we started appending in the file 
   * need to append the key and the value, 
   * to check if this is the right keyvalue and therefore the right file later on, 
   * when we have multiple files */
  int offset;
  FILE* pfile;
  pfile = fopen("segment0", "ab");
  if (pfile == NULL) {
    perror ("Error opening file");
    offset = -1; 
    return offset;
  }
  offset = ftell(pfile); //does this do anything?

  //first write the length of the keyvalue in bytes,
  //then write the keyvalue itself.
  //isn't sizeof(KeyValue) a constant? 
  //
  //write the following:
  //1. size of the key
  int retval = putw(sizeof(int), pfile);
  //int retval = fwrite(sizeof(int), sizeof(sizeof(int)), 1, pfile);
  if (retval != 0) {
    perror ("Error opening file");
    offset = -1;
    return offset;
  }
  //2. size of the value
  //retval = fwrite(VALUE_NUM_BYTES, sizeof(uint32_t), 1, pfile);
  retval = putw(VALUE_NUM_BYTES, pfile);
  if (retval != 0) {
    perror ("Error opening file");
    offset = -1;
    return offset;
  }
  //3. key
  retval = fwrite(&keyvalue.key, sizeof(int), 1, pfile);
  if (retval != 1) {
    perror ("Error opening file");
    offset = -1; 
    return offset;
  }
  //4. value
  retval = fwrite(&keyvalue.value, VALUE_NUM_BYTES, 1, pfile);
  if (retval != 1) {
    perror ("Error opening file");
    offset = -1; 
    return offset;
  }

  fclose(pfile);
  return offset;
}
  

void read_input(InputBuffer* input_buffer) {
  //printf("x\n");
  ssize_t bytes_read = getline(&(input_buffer->buffer), 
                               &(input_buffer->buffer_length), stdin);
  printf("xx\n");
  
  if (bytes_read <= 0) {
    printf("Error reading input\n");
    exit(EXIT_FAILURE);
  }
  
  //get rid of trailing newline
  input_buffer->buffer[bytes_read - 1] = 0;
  input_buffer->input_length = bytes_read - 1;
}

void print_prompt() {
  printf("bitcask-lite> ");
}

ProcessResult process_command(InputBuffer* input_buffer, Command* command) {
  if (strcmp(input_buffer->buffer, ".exit") == 0) {
    cleanup_hashmap();
    exit(EXIT_SUCCESS);
  } else if (strncmp(input_buffer->buffer, "set ", 4) == 0) {
    command->type = SET;
    
    //this keyword var is not used
    char* keyword = strtok(input_buffer->buffer, " ");
    char* key_string = strtok(NULL, " ");
    int key = atoi(key_string);

    char* value = strtok(NULL, " ");
    // print out contents of this value char array
    /* while (pointer is not terminating null byte) { */
    /*   print what value points to; */
    /*   advance value by 4 bytes (char value); */
    /* } */
    
    if (key_string == NULL || value == NULL) {
      //return error
      return PROCESS_ERROR;
    }
    
    //meta: are these valid quantities to be comparing?
    if (strlen(key_string) > KEY_NUM_BYTES) {
      //return key too long error
      return PROCESS_ERROR;
    }
    
    if (strlen(value) > VALUE_NUM_BYTES) {
      //return value too long error
      return PROCESS_ERROR;
    }
    
    command->keyvalue.key = key;
    strcpy(command->keyvalue.value, value);
    return PROCESS_SUCCESS;
    
  } else if (strncmp(input_buffer->buffer, "get ", 4) == 0) {
    command->type = GET;
    
    //keyword is not used
    char* keyword = strtok(input_buffer->buffer, " ");
    char* key_string = strtok(NULL, " ");
    int key = atoi(key_string);

    if (key_string == NULL) {
      return PROCESS_ERROR;
    }
    
    command->keyvalue.key = key;
    
    return PROCESS_SUCCESS;
  } else {
    printf("Unrecognized command '%s'\n", input_buffer->buffer);
    return PROCESS_ERROR;
  }
}

ExecuteResult execute_command(Command* command, Node** hashmap) {
  // char* value; no
  off_t offset;
  switch (command->type) {
    case SET: 
      if (strlen(command->keyvalue.value) > VALUE_NUM_BYTES) {
        return EXECUTE_ERROR;
      }
      offset = append_to_segment(command->keyvalue);
      if (offset == -1) {
        return EXECUTE_ERROR;
      }
      printf("offset: %d\n", offset);
      insert_hashmap(hashmap, command->keyvalue.key, offset);
      return EXECUTE_SUCCESS;
    case GET:
      printf("case statement entered.\n");
      // no. here you need to read from the file.
      // in memory-hashmap only stores offsets into that file.
      offset = get_from_hashmap(hashmap, command->keyvalue.key);
      if (offset == -1) {
        return EXECUTE_ERROR;
      }
      printf("remembered offset is: %d\n", offset);
      // use offset to retrieve value from file
      FILE* segment_p;
      segment_p = fopen("segment0", "r");
      if (segment_p == NULL) {
        printf("segment file not found upon GET.\n");
        return EXECUTE_ERROR;
      }
      int retval;
      retval = fseek(segment_p, offset, SEEK_SET);
      if (retval != 0) {
        printf("error while reading segment file\n.");
      }
      char* value[VALUE_NUM_BYTES] = {0};
      retval = fread(value, VALUE_NUM_BYTES, 1, segment_p);
      if (retval == 0) {
        printf("error while reading segment file\n");
        return 0;
      }
      // do i need to reset the offset after the read?
      
      printf("retrieved value: %s\n", value);
      return EXECUTE_SUCCESS;
    default:
      return EXECUTE_ERROR;
  }
}

/* int load_segment_into_memory(/\*take file descriptor?*\/) { */
/*   // open file and read */
/* } */

int main(int argc, char* argv[]) {
  Node** hashmap = create_hashmap();

  // check if a segment file exists on disk,
  // and if so, load that data into the in-memory hashmap.
  FILE* segment_p;
  segment_p = fopen("segment0", "r");
  if (segment_p == NULL) {
    printf("segment file not found.\n");
    // ATTN: need to skip the loading of the file.
  }
  
  int retval;
  retval = fseek(segment_p, 0, SEEK_END);
  if (retval != 0) {
    printf("error while reading segment file\n.");
  }
  off_t offset_end = ftell(segment_p);
  retval = fseek(segment_p, 0, SEEK_SET);
  if (retval != 0) {
    printf("error while reading segment file\n.");
  }

  if (segment_p != NULL) {
    // could just keep updating an int with the offset
    while (ftell(segment_p) != offset_end) {
      // assume starting at data for size of key, read that -> sk
      //   need to know the size of the data for the size of the key
      //   we know it is sizeof(int) which is...
      // then jump by size of value
      // then, next sk bytes, read and -> key of hash table
      // then, get offset after key, and -> value of hash table
      size_t size_k;
      retval = fread(&size_k, sizeof(int), 1, segment_p);
      if (retval == 0) {
        printf("error while reading segment file1\n");
        return 0;
      }
      fseek(segment_p, sizeof(VALUE_NUM_BYTES), SEEK_CUR);

      int key;
      retval = fread(&key, size_k, 1, segment_p);
      if (retval == 0) { // make a macro for this
        printf("error while reading segment file3\n");
        return 0;
      }
      off_t offset = ftell(segment_p);
      printf("read offset: %lld\n", offset);
      insert_hashmap(hashmap, key, offset);
      
      // is it okay to fseek past the end of the file?
      retval = fseek(segment_p, VALUE_NUM_BYTES, SEEK_CUR);
      if (retval != 0) { // make a macro for this
        printf("error while seeking in file\n");
        return 0;
      }
    }
  }
  
  Command command = {0};
  InputBuffer* input_buffer = new_input_buffer();
  while (true) {
    print_prompt();
    read_input(input_buffer); //puts input in the buffer of input_buffer
    printf("input successfully read.\n");
        
    switch (process_command(input_buffer, &command)) {
      case PROCESS_SUCCESS:
        break;
      case PROCESS_ERROR:
        printf("Error processing command\n");
        return -1;
    }

    switch (execute_command(&command, hashmap)) {
      case EXECUTE_SUCCESS:
        printf("Executed\n");
        break;
      case EXECUTE_ERROR:
        printf("Error\n");
        break;
    }
  }
}
