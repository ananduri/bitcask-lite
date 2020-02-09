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
  unsigned int h = 0;
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
  Node* bucket_node = hashmap[h];
    
  while ((bucket_node->next_node != NULL) &&
      (bucket_node->key != key)) {
    bucket_node = bucket_node->next_node;
  }
  
  return bucket_node;
}

void insert_hashmap(Node** hashmap, int key, off_t offset) {
  Node* bucket_node = get_bucket(hashmap, key);
  
  if (bucket_node->offset == -1) {
    /* Do we need these two lines of code here? */
    bucket_node->key = key;
    bucket_node->offset = offset;
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

int append_to_segment(KeyValue keyvalue) {
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
  ssize_t bytes_read = getline(&(input_buffer->buffer), 
                               &(input_buffer->buffer_length), stdin);
  
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
  int offset;
  switch (command->type) {
    case SET: 
      if (strlen(command->keyvalue.value) > VALUE_NUM_BYTES) {
        return EXECUTE_ERROR;
      }
      int offset = append_to_segment(command->keyvalue);
      if (offset == -1) {
        return EXECUTE_ERROR;
      }
      printf("offset: %d\n", offset);
      insert_hashmap(hashmap, command->keyvalue.key, offset);
      return EXECUTE_SUCCESS;
    case GET:
      // no. here you need to read from the file.
      // in memory-hashmap only stores offsets into that file.
      offset = get_from_hashmap(hashmap, command->keyvalue.key);
      if (offset == -1) {
        return EXECUTE_ERROR;
      }
      printf("%d\n", offset);
      // use offset to retrieve value from file
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
  if (segment_p != NULL) {
    // assume starting at data for size of key, read that -> sk
    //   need to know the size of the data for the size of the key
    //   we know it is sizeof(int) which is...
    // then read size of value -> sv
    // then, next sk bytes, read and -> key
    // then, get offset after key, and -> value of hash table
    int retval;
    size_t size_k;  // some confusion to be resolved here around the void*
    retval = fread(/* type:(void*) */ &size_k, sizeof(int), 1, segment_p);
    //retval = fread(/* type:(void*) */ &size_k, 1, 1, segment_p);
    if (retval == 0) {
      printf("error while reading segment file1\n.");
      return 0;
    }
    
    // i dont actually need size_v here...
    // i will in the GET but not here
    size_t size_v;
    // size_v should not be 1
    printf("offset before read of size_v: %ld\n", ftell(segment_p));
    retval = fread(&size_v, sizeof(VALUE_NUM_BYTES), 1, segment_p);
    if (retval == 0) {
      printf("error while reading segment file2\n.");
      return 0;
    }
    int key;
    printf("offset before break: %ld\n", ftell(segment_p));
    printf("size_k: %zu\n", size_k);
    printf("size_v: %zu\n", size_v);
    retval = fread(&key, size_k, 1, segment_p);
    printf("retval: %d\n", retval);
    if (retval == 0) { // make a macro for this
      printf("error while reading segment file3\n.");
      return 0;
    }
    off_t offset = ftell(segment_p);
    insert_hashmap(hashmap, key, offset);
    printf("read offset: %lld\n", offset);
  }
  
  Command command = {0};
  InputBuffer* input_buffer = new_input_buffer();
  while (true) {
    print_prompt();
    read_input(input_buffer); //puts input in the buffer of input_buffer
        
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
