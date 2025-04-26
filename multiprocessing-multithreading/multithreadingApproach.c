#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#define MAX_WORD_LENGTH 60
#define INITIAL_CAPACITY 18000000
#define TOP_K 10
#define GROWTH_FACTOR 2
#define NUM_THREADS 8

// Mutex for thread-safe operations
pthread_mutex_t frequency_mutex = PTHREAD_MUTEX_INITIALIZER;

// Structure to store word and its frequency
typedef struct {
    char *word;
    int frequency;
} WordFreq;

// Structure to manage dynamic array
typedef struct {
    WordFreq *data;
    int size;
    int capacity;
} WordFreqArray;

// Thread argument structure
typedef struct {
    char **words;
    int start;
    int end;
    WordFreqArray *shared_word_freq;
} ThreadArgs;

// Create dynamic word frequency array with initial memory allocation
WordFreqArray* create_word_freq_array() {
    WordFreqArray *arr = malloc(sizeof(WordFreqArray));
    if (!arr) {
        perror("Memory allocation failed");
        exit(1);
    }
    arr->data = malloc(INITIAL_CAPACITY * sizeof(WordFreq));
    if (!arr->data) {
        perror("Memory allocation failed");
        free(arr);
        exit(1);
    }
    arr->size = 0;
    arr->capacity = INITIAL_CAPACITY;
    return arr;
}

// Resize array when capacity is reached
void resize_word_freq_array(WordFreqArray *arr) {
    int new_capacity = arr->capacity * GROWTH_FACTOR;
    WordFreq *new_data = realloc(arr->data, new_capacity * sizeof(WordFreq));

    if (!new_data) {
        perror("Memory reallocation failed");
        free(arr->data);
        free(arr);
        exit(1);
    }

    arr->data = new_data;
    arr->capacity = new_capacity;
}

// Free dynamically allocated memory
void free_word_freq_array(WordFreqArray *arr) {
    for (int i = 0; i < arr->size; i++) {
        free(arr->data[i].word);
    }
    free(arr->data);
    free(arr);
}

// Merge two sorted subarrays by frequency
void merge(WordFreq arr[], int left, int mid, int right) {
    int left_size = mid - left + 1;
    int right_size = right - mid;

    // Create temporary arrays for merging
    WordFreq *left_arr = malloc(left_size * sizeof(WordFreq));
    WordFreq *right_arr = malloc(right_size * sizeof(WordFreq));

    if (!left_arr || !right_arr) {
        perror("Merge array allocation failed");
        free(left_arr);
        free(right_arr);
        return;
    }

    // Copy data to temporary arrays
    for (int i = 0; i < left_size; i++) {
        left_arr[i].word = strdup(arr[left + i].word);
        left_arr[i].frequency = arr[left + i].frequency;
    }
    for (int j = 0; j < right_size; j++) {
        right_arr[j].word = strdup(arr[mid + 1 + j].word);
        right_arr[j].frequency = arr[mid + 1 + j].frequency;
    }

    // Merge the temporary arrays
    int i = 0, j = 0, k = left;
    while (i < left_size && j < right_size) {
        if (left_arr[i].frequency >= right_arr[j].frequency) {
            arr[k].word = left_arr[i].word;
            arr[k].frequency = left_arr[i].frequency;
            i++;
        } else {
            arr[k].word = right_arr[j].word;
            arr[k].frequency = right_arr[j].frequency;
            j++;
        }
        k++;
    }

    // Copy remaining elements
    while (i < left_size) {
        arr[k].word = left_arr[i].word;
        arr[k].frequency = left_arr[i].frequency;
        i++;
        k++;
    }

    while (j < right_size) {
        arr[k].word = right_arr[j].word;
        arr[k].frequency = right_arr[j].frequency;
        j++;
        k++;
    }

    // Free temporary arrays
    free(left_arr);
    free(right_arr);
}

// Recursive merge sort for word frequencies
void merge_sort(WordFreq arr[], int left, int right) {
    if (left >= right) return;

    int mid = left + (right - left) / 2;
    merge_sort(arr, left, mid);
    merge_sort(arr, mid + 1, right);
    merge(arr, left, mid, right);
}

// Thread function to process word frequencies
void* process_word_chunk(void *arg) {
    ThreadArgs *thread_args = (ThreadArgs*)arg;

    // Local frequency array for thread
    WordFreqArray local_word_freq;
    local_word_freq.data = malloc(INITIAL_CAPACITY * sizeof(WordFreq));
    local_word_freq.size = 0;
    local_word_freq.capacity = INITIAL_CAPACITY;

    // Process words in assigned chunk
    for (int i = thread_args->start; i < thread_args->end; i++) {
        int found = 0;

        // Check local frequencies first
        for (int j = 0; j < local_word_freq.size; j++) {
            if (strcmp(local_word_freq.data[j].word, thread_args->words[i]) == 0) {
                local_word_freq.data[j].frequency++;
                found = 1;
                break;
            }
        }

        // Add new word to local array if not found
        if (!found) {
            if (local_word_freq.size >= local_word_freq.capacity) {
                local_word_freq.capacity *= GROWTH_FACTOR;
                local_word_freq.data = realloc(local_word_freq.data,
                                               local_word_freq.capacity * sizeof(WordFreq));
            }

            local_word_freq.data[local_word_freq.size].word = strdup(thread_args->words[i]);
            local_word_freq.data[local_word_freq.size].frequency = 1;
            local_word_freq.size++;
        }
    }

    // Merge local results with shared array
    pthread_mutex_lock(&frequency_mutex);
    for (int i = 0; i < local_word_freq.size; i++) {
        int found = 0;
        for (int j = 0; j < thread_args->shared_word_freq->size; j++) {
            if (strcmp(thread_args->shared_word_freq->data[j].word,
                       local_word_freq.data[i].word) == 0) {
                thread_args->shared_word_freq->data[j].frequency +=
                        local_word_freq.data[i].frequency;
                found = 1;
                break;
            }
        }

        // Add new word to shared array if not found
        if (!found) {
            if (thread_args->shared_word_freq->size >=
                thread_args->shared_word_freq->capacity) {
                resize_word_freq_array(thread_args->shared_word_freq);
            }

            thread_args->shared_word_freq->data[thread_args->shared_word_freq->size].word =
                    strdup(local_word_freq.data[i].word);
            thread_args->shared_word_freq->data[thread_args->shared_word_freq->size].frequency =
                    local_word_freq.data[i].frequency;
            thread_args->shared_word_freq->size++;
        }
    }
    pthread_mutex_unlock(&frequency_mutex);

    // Free local resources
    for (int i = 0; i < local_word_freq.size; i++) {
        free(local_word_freq.data[i].word);
    }
    free(local_word_freq.data);

    return NULL;
}

// Read words from input file
char** read_words_from_file(char *filename, int *total_words) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return NULL;
    }

    // Initialize dynamic array for words
    int capacity = INITIAL_CAPACITY;
    char **words = malloc(capacity * sizeof(char*));
    *total_words = 0;

    char buffer[MAX_WORD_LENGTH];

    // Read words with dynamic reallocation
    while (fscanf(file, "%59s", buffer) == 1) {
        if (*total_words >= capacity) {
            capacity *= GROWTH_FACTOR;
            char **temp = realloc(words, capacity * sizeof(char*));
            if (!temp) {
                perror("Memory reallocation failed");
                // Free previously allocated memory
                for (int i = 0; i < *total_words; i++) {
                    free(words[i]);
                }
                free(words);
                fclose(file);
                return NULL;
            }
            words = temp;
        }

        words[*total_words] = strdup(buffer);
        (*total_words)++;
    }

    fclose(file);
    return words;
}

int main() {
    char filename[] = "text8.txt";  // name of input file
    int total_words = 0;

    // Time tracking structures
    struct timeval start, end;
    double execution_time;

    // Start timing execution
    gettimeofday(&start, NULL);

    // Read words from file
    char **words = read_words_from_file(filename, &total_words);
    if (!words) {
        fprintf(stderr, "Failed to read words from file\n");
        return 1;
    }

    // Create shared word frequency array
    WordFreqArray *word_freq = create_word_freq_array();

    // Create thread handles
    pthread_t threads[NUM_THREADS];
    ThreadArgs thread_args[NUM_THREADS];

    // Distribute work among threads
    int chunk_size = total_words / NUM_THREADS;
    int remainder = total_words % NUM_THREADS;

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        // Calculate start and end for each thread
        int start = i * chunk_size + (i < remainder ? i : remainder);
        int end = start + chunk_size + (i < remainder ? 1 : 0);

        // Prepare thread arguments
        thread_args[i].words = words;
        thread_args[i].start = start;
        thread_args[i].end = end;
        thread_args[i].shared_word_freq = word_freq;

        // Create thread
        if (pthread_create(&threads[i], NULL, process_word_chunk, &thread_args[i]) != 0) {
            perror("Thread creation failed");
            // Cleanup resources
            for (int j = 0; j < total_words; j++) {
                free(words[j]);
            }
            free(words);
            free_word_freq_array(word_freq);
            return 1;
        }
    }

    // Wait for all threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Sort words by frequency
    merge_sort(word_freq->data, 0, word_freq->size - 1);

    // End timing execution
    gettimeofday(&end, NULL);
    execution_time = (end.tv_sec - start.tv_sec) +
                     (end.tv_usec - start.tv_usec) / 1000000.0;

    // Print top frequent words
    printf("Top 10 Most Frequent Words:\n");
    int print_limit = (TOP_K < word_freq->size) ? TOP_K : word_freq->size;
    for (int i = 0; i < print_limit; i++) {
        printf("%s: %d\n", word_freq->data[i].word, word_freq->data[i].frequency);
    }

    // Print statistics
    printf("\nTotal Words: %d\n", total_words);
    printf("Number of Threads Used: %d\n", NUM_THREADS);
    printf("Execution Time: %.4f seconds\n", execution_time);

    // Free resources
    for (int i = 0; i < total_words; i++) {
        free(words[i]);
    }
    free(words);
    free_word_freq_array(word_freq);

    return 0;
}