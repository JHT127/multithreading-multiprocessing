#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/time.h>


#define MAX_WORD_LENGTH 60
#define INITIAL_CAPACITY 18000000
#define TOP_K 10
#define GROWTH_FACTOR 2
#define NUM_PROCESSES 8

// Word frequency structure
typedef struct {
    char word[MAX_WORD_LENGTH];
    int frequency;
} WordFreq;

// Dynamic array for word frequencies
typedef struct {
    WordFreq *data;
    int size;
    int capacity;
} WordFreqArray;

// Shared memory structure
typedef struct {
    int size;
    WordFreq data[INITIAL_CAPACITY];
} SharedFreqData;

// Function prototypes
void init_word_freq_array(WordFreqArray *arr);
void add_word_to_freq_array(WordFreqArray *arr, const char *word);
void merge_sort_word_freq(WordFreq *arr, int left, int right);
void merge_word_freq(WordFreq *arr, int left, int mid, int right);
char** read_words_from_file(const char *filename, int *total_words);

// Initialize word frequency array
void init_word_freq_array(WordFreqArray *arr) {
    arr->data = malloc(INITIAL_CAPACITY * sizeof(WordFreq));
    if (!arr->data) {
        perror("Memory allocation failed");
        exit(1);
    }
    arr->size = 0;
    arr->capacity = INITIAL_CAPACITY;
}

// Add word to frequency array with dynamic resizing
void add_word_to_freq_array(WordFreqArray *arr, const char *word) {
    // Check if word already exists
    for (int i = 0; i < arr->size; i++) {
        if (strcmp(arr->data[i].word, word) == 0) {
            arr->data[i].frequency++;
            return;
        }
    }

    // Resize array if needed
    if (arr->size >= arr->capacity) {
        arr->capacity *= GROWTH_FACTOR;
        arr->data = realloc(arr->data, arr->capacity * sizeof(WordFreq));
        if (!arr->data) {
            perror("Memory reallocation failed");
            exit(1);
        }
    }

    // Add new word
    strncpy(arr->data[arr->size].word, word, MAX_WORD_LENGTH - 1);
    arr->data[arr->size].word[MAX_WORD_LENGTH - 1] = '\0';
    arr->data[arr->size].frequency = 1;
    arr->size++;
}

// Merge subarrays during sorting
void merge_word_freq(WordFreq *arr, int left, int mid, int right) {
    int left_size = mid - left + 1;
    int right_size = right - mid;

    // Temporary arrays
    WordFreq *left_arr = malloc(left_size * sizeof(WordFreq));
    WordFreq *right_arr = malloc(right_size * sizeof(WordFreq));

    if (!left_arr || !right_arr) {
        perror("Merge allocation failed");
        free(left_arr);
        free(right_arr);
        return;
    }

    // Copy data to temporary arrays
    memcpy(left_arr, &arr[left], left_size * sizeof(WordFreq));
    memcpy(right_arr, &arr[mid + 1], right_size * sizeof(WordFreq));

    // Merge back
    int i = 0, j = 0, k = left;
    while (i < left_size && j < right_size) {
        if (left_arr[i].frequency >= right_arr[j].frequency) {
            arr[k] = left_arr[i];
            i++;
        } else {
            arr[k] = right_arr[j];
            j++;
        }
        k++;
    }

    // Copy remaining elements
    while (i < left_size) {
        arr[k] = left_arr[i];
        i++;
        k++;
    }

    while (j < right_size) {
        arr[k] = right_arr[j];
        j++;
        k++;
    }

    // Free temporary arrays
    free(left_arr);
    free(right_arr);
}

// Recursive merge sort for word frequencies
void merge_sort_word_freq(WordFreq *arr, int left, int right) {
    if (left >= right) return;

    int mid = left + (right - left) / 2;
    merge_sort_word_freq(arr, left, mid);
    merge_sort_word_freq(arr, mid + 1, right);
    merge_word_freq(arr, left, mid, right);
}

// Read words from input file with dynamic memory allocation
char** read_words_from_file(const char *filename, int *total_words) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return NULL;
    }

    // Initial allocation
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
    // Start timing
    struct timeval start, end;
    gettimeofday(&start, NULL);

    const char *filename = "text8.txt";
    int total_words = 0;

    // Read words from file
    char **words = read_words_from_file(filename, &total_words);
    if (!words) {
        fprintf(stderr, "Failed to read words from file\n");
        return 1;
    }

    // Create shared memory for word frequencies
    SharedFreqData *shared_data = mmap(NULL, sizeof(SharedFreqData),
                                       PROT_READ | PROT_WRITE,
                                       MAP_SHARED | MAP_ANONYMOUS,
                                       -1, 0);

    if (shared_data == MAP_FAILED) {
        perror("mmap failed");
        for (int i = 0; i < total_words; i++) {
            free(words[i]);
        }
        free(words);
        return 1;
    }

    // Fork child processes with optimized chunk distribution
    pid_t pids[NUM_PROCESSES];
    int chunk_size = total_words / NUM_PROCESSES;
    int remainder = total_words % NUM_PROCESSES;

    for (int i = 0; i < NUM_PROCESSES; i++) {
        pids[i] = fork();

        if (pids[i] == -1) {
            perror("fork failed");
            munmap(shared_data, sizeof(SharedFreqData));
            for (int j = 0; j < total_words; j++) {
                free(words[j]);
            }
            free(words);
            exit(1);
        } else if (pids[i] == 0) {
            // Child process
            int start = i * chunk_size + (i < remainder ? i : remainder);
            int end = start + chunk_size + (i < remainder ? 1 : 0);

            // Local word frequency array
            WordFreqArray local_freq;
            init_word_freq_array(&local_freq);

            // Count frequencies for this subset of words
            for (int j = start; j < end; j++) {
                add_word_to_freq_array(&local_freq, words[j]);
            }

            // Transfer to shared memory with atomic-like synchronization
            for (int j = 0; j < local_freq.size; j++) {
                int found = 0;
                for (int k = 0; k < shared_data->size; k++) {
                    if (strcmp(shared_data->data[k].word, local_freq.data[j].word) == 0) {
                        shared_data->data[k].frequency += local_freq.data[j].frequency;
                        found = 1;
                        break;
                    }
                }

                // Add new word if not found
                if (!found && shared_data->size < INITIAL_CAPACITY) {
                    strcpy(shared_data->data[shared_data->size].word, local_freq.data[j].word);
                    shared_data->data[shared_data->size].frequency = local_freq.data[j].frequency;
                    shared_data->size++;
                }
            }

            // Free local resources
            free(local_freq.data);
            exit(0);
        }
    }

    // Parent process waits for children
    for (int i = 0; i < NUM_PROCESSES; i++) {
        int status;
        waitpid(pids[i], &status, 0);

        // Check if child process terminated normally
        if (!WIFEXITED(status)) {
            fprintf(stderr, "Child process %d did not terminate normally\n", pids[i]);
        }
    }

    // Sort words by frequency
    merge_sort_word_freq(shared_data->data, 0, shared_data->size - 1);

    // End timing calculation
    gettimeofday(&end, NULL);
    double execution_time = (end.tv_sec - start.tv_sec) +
                            (end.tv_usec - start.tv_usec) / 1000000.0;

    // Print top 10 most frequent words
    printf("Top 10 Most Frequent Words:\n");
    int print_limit = (TOP_K < shared_data->size) ? TOP_K : shared_data->size;
    for (int i = 0; i < print_limit; i++) {
        printf("%s: %d\n", shared_data->data[i].word, shared_data->data[i].frequency);
    }

    // Print statistics
    printf("\nTotal Words: %d\n", total_words);
    printf("Number of Processes Used: %d\n", NUM_PROCESSES);
    printf("Execution Time: %.4f seconds\n", execution_time);

    // Free resources
    for (int i = 0; i < total_words; i++) {
        free(words[i]);
    }
    free(words);
    munmap(shared_data, sizeof(SharedFreqData));

    return 0;
}