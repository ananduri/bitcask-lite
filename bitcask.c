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
const uint32_t VALUE_NUM_BYTES = 25;
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
 * returns pointer to array of pointers to linked lists.
 *
 * since values desribe offsets in a file,
 * -1 is an invalid value
 * and can therefore serve as a sentinal value
 * for an unoccupied node
 */
Node** create_hashmap() {
  Node** array = (Node**)malloc(NHASH * sizeof(Node*));
  for (int i = 0; i < NHASH; i++) {
    array[i] = (Node*)malloc(sizeof(Node));
    array[i]->next_node = NULL;
    
    strcpy(array[i]->value, "-1");
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
    printf("key: %d\n", bucket_node->key);
    bucket_node = bucket_node->next_node;
  }
  
  return bucket_node;
}

void insert_hashmap(Node** hashmap, int key, char* value) {
  Node* bucket_node = get_bucket(hashmap, key);
  
  if (strcmp(bucket_node->value, "-1") == 0) {
    bucket_node->key = key;
    strcpy(bucket_node->value, value);
    return;
  }
  
  if (bucket_node->key != key) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    bucket_node->next_node = new_node;
    new_node->key = key;
    strcpy(bucket_node->value, value);
    new_node->next_node = (Node*)malloc(sizeof(Node));
    return;
  }
  
  bucket_node->key = key;
  strcpy(bucket_node->value, value);
  bucket_node->next_node = (Node*)malloc(sizeof(Node));
}
 
char* get_hashmap(Node** hashmap, int key) {
  Node* bucket_node = get_bucket(hashmap, key);
  if (bucket_node == NULL || strcmp(bucket_node->value, "-1") == 0) {
    return NULL;
  }
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
    
    char* keyword = strtok(input_buffer->buffer, " ");
    char* key_string = strtok(NULL, " ");
    int key = atoi(key_string);
    char* value = strtok(NULL, " ");
        
    if (key_string == NULL || value == NULL) {
      //return error
      return PROCESS_ERROR;
    }
    
    //~ are these valid quantities to be comparing?
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
  char* value;
  switch (command->type) {
    case SET:
      if (strlen(command->keyvalue.value) > VALUE_NUM_BYTES) {
        return EXECUTE_ERROR;
      }
      insert_hashmap(hashmap, command->keyvalue.key, command->keyvalue.value);
      return EXECUTE_SUCCESS;
    case GET:
      value = get_hashmap(hashmap, command->keyvalue.key);
      if (value == NULL) {
        return EXECUTE_ERROR;
      }
      printf("%s\n", value);
      return EXECUTE_SUCCESS;
  }
}

int main(int argc, char* argv[]) {
  Node** hashmap = create_hashmap();
  
  Command command;
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
