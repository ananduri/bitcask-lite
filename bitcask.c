#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

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
  //int file_id;
  Node* next_node;
};

#define MULT 31  //effective multiplier (according to Bentley)
#define NHASH 101  //prime near hash table capacity
unsigned int hash(int key) {
  /* homemade bad hash function */
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

off_t append_to_segment(KeyValue keyvalue) {
  /* for now, keep appending to the same file.
   *   
   * need to append both the key and the value, 
   * to check if this is the right keyvalue
   * and therefore the right file later on, 
   * when we have multiple files */
  FILE* segment_p;
  segment_p = fopen("segment0", "ab");
  if (segment_p == NULL) {
    perror ("Error opening file");
    return -1;
  }
  off_t offset = ftell(segment_p); // switch to this

  //Write the following:
  //1. size of the key
  int retval = putw(sizeof(int), segment_p);
  if (retval != 0) {
    perror ("Error opening file");
    return -1;
  }
  //2. size of the value
  retval = putw(VALUE_NUM_BYTES, segment_p);
  if (retval != 0) {
    perror ("Error opening file");
    return -1;
  }
  //3. key
  retval = fwrite(&keyvalue.key, sizeof(int), 1, segment_p);
  if (retval != 1) {
    perror ("Error opening file");
    return -1;
  }
  //4. value
  retval = fwrite(&keyvalue.value, VALUE_NUM_BYTES, 1, segment_p);
  if (retval != 1) {
    perror ("Error opening file");
    return -1;
  }

  fclose(segment_p);
  return offset;
}
  

void read_input(InputBuffer* input_buffer) {
  ssize_t bytes_read = getline(&(input_buffer->buffer), 
                               &(input_buffer->buffer_length), stdin);
  
  if (bytes_read <= 0) {
    printf("Error reading input\n");
    exit(EXIT_FAILURE);
  }
  
  // We are subtracting 1 in order to get rid of trailing newline.
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
    
    //this keyword var is not used; can I just call the function without the lvalue?
    char* keyword = strtok(input_buffer->buffer, " ");
    char* key_string = strtok(NULL, " ");
    int key = atoi(key_string);
    char* value = strtok(NULL, " ");
    if (key_string == NULL || value == NULL) {
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
    
    //keyword is not used; can I just call the function without the lvalue?
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

off_t get_file_size(FILE* segment_p) {
  int fd = fileno(segment_p);
  if (fd == -1) {
    printf("error when getting file descriptor\n");
    return -1;
  }
  struct stat sb;
  if (fstat(fd, &sb) == -1) {
    perror("fstat");  // another style of erroring out?
    return -1;
  }
  return sb.st_size;
}

int get_value_from_segment(FILE* segment_p, off_t offset, char* value) {
  // Reading value directly from file here.
  // May need to read the size of the value, and then the value.
  // Actually, need to read the key as well, to compare with the input
  // and for that, need the size of the key as well.
  int retval;
  retval = fseek(segment_p, offset, SEEK_SET);
  if (retval != 0) {
    printf("error while seeking segment file\n.");
  }
  size_t size_k = 0;
  retval = fread(&size_k, sizeof(int), 1, segment_p);
  if (retval == 0) {
    printf("error while reading segment file1\n");
    return -1;
  }
  size_t size_v = 0;
  retval = fread(&size_v, sizeof(VALUE_NUM_BYTES), 1, segment_p);
  if (retval == 0) {
    printf("error while reading segment file2\n");
    return -1;
  }
  int read_key;
  retval = fread(&read_key, size_k, 1, segment_p);
  if (retval == 0) {
    printf("error while reading segment file3\n");
    return -1;
  }
  // compare read key with supplied key
  retval = fread(value, size_v, 1, segment_p);
  if (retval == 0) {
    printf("error while reading segment file4\n");
    return -1;
  }
  return 0;
}

ExecuteResult execute_command(Command* command, Node** hashmap) {
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
      printf("offset: %ld\n", offset);
      insert_hashmap(hashmap, command->keyvalue.key, offset);
      return EXECUTE_SUCCESS;
    case GET:
      offset = get_from_hashmap(hashmap, command->keyvalue.key);
      if (offset == -1) {
        return EXECUTE_ERROR;
      }
      printf("remembered offset is: %ld\n", offset);
      FILE* segment_p;
      segment_p = fopen("segment0", "r");
      if (segment_p == NULL) {
        printf("segment file not found upon GET.\n");
        return EXECUTE_ERROR;
      }
      char value[VALUE_NUM_BYTES] = {0};
      int retval = get_value_from_segment(segment_p, offset, value);
      if (retval == -1) {
        /* error printed in callee */
        return EXECUTE_ERROR;
      }
      printf("retrieved: %s\n", value);
      return EXECUTE_SUCCESS;
    default:
      return EXECUTE_ERROR;
  }
}

int load_segment_into_memory(FILE* segment_p, Node** hashmap) {
  int retval;
  retval = fseek(segment_p, 0, SEEK_END);
  if (retval != 0) {
    printf("error while reading segment file\n.");
    return -1;
  }
  off_t offset_end = ftell(segment_p);
  retval = fseek(segment_p, 0, SEEK_SET);
  if (retval != 0) {
    printf("error while reading segment file\n.");
    return -1;
  }

  if (segment_p != NULL) {
    // could just keep updating an int with the offset
    while (ftell(segment_p) < offset_end) {
      off_t offset = ftell(segment_p);
      size_t size_k;
      retval = fread(&size_k, sizeof(int), 1, segment_p);
      if (retval == 0) {
        printf("error while reading segment file\n");
        return -1;
      }
      fseek(segment_p, sizeof(VALUE_NUM_BYTES), SEEK_CUR);

      int key;
      retval = fread(&key, size_k, 1, segment_p);
      if (retval == 0) {
        printf("error while reading segment file\n");
        return -1;
      }
      insert_hashmap(hashmap, key, offset);
      
      retval = fseek(segment_p, VALUE_NUM_BYTES, SEEK_CUR);
      if (retval != 0) { // make a macro for this
        printf("error while seeking in file\n");
        return -1;
      }
    }
  }
  return 0;
}

int main(int argc, char* argv[]) {
  Node** hashmap = create_hashmap();
  int retval;

  // read max segment ID from ID file
  FILE* idfile_p;
  int segment_id;
  idfile_p = fopen("last_used_id", "a+");
  // what if file doesn't exist--have to use a+ ?
  if (idfile_p == NULL) {
    printf("error while opening segment ID file\n");
    return -1;
  }
  bool created_segment_id_file = false;
  if (ftell(idfile_p) == 0) {
    segment_id = 0;
    created_segment_id_file = true;
  } else {
    // seek to beginning (glibc does do this but not other impls)
    fseek(idfile_p, 0, SEEK_SET);
    retval = fread(&segment_id, sizeof(int), 1, idfile_p);
    if (retval != 0) {
      printf("error while reading ID file\n");
      return -1;
    }
  }
  
  char[10] segment_file_name;
  retval = sprintf(segment_file_name, "segment%d", segment_id);
  if (retval < 0) {
    printf("error while calling sprintf\n");
    return -1;
  }
  printf("segment file name: %s\n", segment_file_name);  // debug statement

  // check if a segment file exists on disk,
  // and if so, load that data into the in-memory hashmap.
  FILE* segment_p;
  //segment_p = fopen("segment0", "r");
  segment_p = fopen(segment_file_name, "r");
  if (segment_p == NULL) {
    printf("error while opening segment file\n");
    return -1;
  }
  retval = load_segment_into_memory(segment_p, hashmap);
  if (retval != 0) {
    return -1;
  }
  if (created_segment_id_file) {
    // will segment_id always be 0 if the segment id file is just created?
    putw(segment_id, idfile_p);
  }
  
  Command command = {0};
  InputBuffer* input_buffer = new_input_buffer();
  while (true) {
    print_prompt();
    read_input(input_buffer);
        
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
