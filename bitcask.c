#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//first, need to build repl functionality

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

/* 
 * Data layout of the segment file
 */
const uint32_t VALUE_SIZE = 255;

const uint32_t VALUE_NUM_BYTES_SIZE = sizeof(VALUE_SIZE);

typedef struct Node_t Node;
struct Node_t {
  //key (to match with when doing lookups)
  int key;
  //value stored as raw string
  char value[VALUE_SIZE];
  //pointer to next node
  Node* next_node;
};


// hash function
// a popular multiplier
#define MULT 31
#define NHASH 101
unsigned int hash(char *p) {
  unsigned int h = 0;
  for (; *p; p++) {
    h = MULT * h + *p;
  }
  return h % NHASH;
}

// returns pointer to array of pointers to linked lists
// nhash denotes the length of the array of the hashmap
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
  bucket_node->value = value; //*** this is problematic; copy the array or do something else
  bucket_node->next_node = (Node*)malloc(sizeof(Node*));
  //is the key of the uninitialized but allocated next node
  //set to NULL?
}

char* get_hashmap(Node** hashmap, int key) {
  //hash the key
  //get the linked list at that value in the hashmap array  
  Node* bucket_node = get_bucket(hashmap, key);  
  return bucket_node->value;
}

void cleanup_hashmap() {
  //free all pointers
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
  printf("bitcask-lite>");
}

void process_command(InputBuffer* input_buffer) {
  if (strcmp(input_buffer->buffer, ".exit") == 0) {
    exit(EXIT_SUCCESS);
  } else {
    printf("Unrecognized command '%s'\n", input_buffer->buffer);
  }
}

int main(int argc, char* argv[]) {
  InputBuffer* input_buffer = new_input_buffer();
  while (true) {
    print_prompt();
    read_input(input_buffer); //puts input in the buffer of input_buffer
    
    process_command(input_buffer);
  }
}
