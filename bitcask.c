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
const uint32_t KEY_NUM_BYTES = 32;
const uint32_t VALUE_NUM_BYTES = 256;
//const uint32_t VALUE_NUM_BYTES_SIZE = sizeof(VALUE_NUM_BYTES);

/*
 * In-memory hash table
 */
typedef struct Node_t Node;
struct Node_t {
  int key;
  char value[VALUE_NUM_BYTES];
  Node* next_node;
};

#define MULT 31  //effective multiplier
#define NHASH 101  //prime near hash table capacity
unsigned int hash(char *p) {
  unsigned int h = 0;
  for (; *p; p++) {
    h = MULT * h + *p;
  }
  return h % NHASH;
}

// returns pointer to array of pointers to linked lists
Node** create_hashmap() {
  void* array = malloc(NHASH * sizeof(Node*));
  return (Node**)array;
}

Node* get_bucket(Node** hashmap, int key) {
  unsigned int h = hash((char*)&key);
  Node* bucket_node = hashmap[h];
  while (bucket_node->key != key) {
    bucket_node = bucket_node->next_node;
  }
  return bucket_node;
}

void insert_hashmap(Node** hashmap, int key, char* value) {
  //hash the key
  //get the linked list at that value in the hashmap array
  //add a node with the value
  Node* bucket_node = get_bucket(hashmap, key);
  
  bucket_node->key = key;
  strcpy(bucket_node->value, value);
  bucket_node->next_node = (Node*)malloc(sizeof(Node*));
}
 
char* get_hashmap(Node** hashmap, int key) {
  //hash the key
  //get the linked list at that value in the hashmap array  
  Node* bucket_node = get_bucket(hashmap, key);  
  return bucket_node->value;
}

void cleanup_hashmap() {
  //free all pointers
  printf("You forgot to cleanup\n");
}







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
  char key[KEY_NUM_BYTES];
  char value[VALUE_NUM_BYTES];
};
typedef struct KeyValue_t KeyValue;

enum ProcessResult_t {
  SUCCESS,
  ERROR,
};
typedef enum ProcessResult_t ProcessResult;

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
    
    printf("command type set to SET\n");
    
    char* keyword = strtok(input_buffer->buffer, " ");
    char* key = strtok(NULL, " ");
    char* value = strtok(NULL, " ");
    
    if (key == NULL || value == NULL) {
      //return error
      return ERROR;
    }
    
    if (strlen(key) > KEY_NUM_BYTES) {
      //return key too long error
      return ERROR;
    }
    
    if (strlen(value) > VALUE_NUM_BYTES) {
      //return value too long error
      return ERROR;
    }
    
    strcpy(command->keyvalue.key, key);
    strcpy(command->keyvalue.value, value);
        
    printf("key: %s\n", command->keyvalue.key);
    printf("value: %s\n", command->keyvalue.value);
    return SUCCESS;
  } else if (strncmp(input_buffer->buffer, "get ", 4) == 0) {
    command->type = GET;
    //read in key
    
    return SUCCESS;
  } else {
    printf("Unrecognized command '%s'\n", input_buffer->buffer);
    return ERROR;
  }
}

int main(int argc, char* argv[]) {
  Node** hashmap = create_hashmap();
  
  Command command;
  InputBuffer* input_buffer = new_input_buffer();
  while (true) {
    print_prompt();
    read_input(input_buffer); //puts input in the buffer of input_buffer
    process_command(input_buffer, &command);
  }
}
