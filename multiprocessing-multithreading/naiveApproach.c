#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#define MAX_WORD_LENGTH 60
#define INITIAL_CAPACITY 18000000
#define TOP_K 10
#define GROWTH_FACTOR 2

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

// Count frequencies of words in the input array
int count_word_frequencies(char **words, int total_words, WordFreqArray *word_freq) {
    for (int i = 0; i < total_words; i++) {
        int found = 0;

        // Check if word already exists
        for (int j = 0; j < word_freq->size; j++) {
            if (strcmp(word_freq->data[j].word, words[i]) == 0) {
                word_freq->data[j].frequency++;
                found = 1;
                break;
            }
        }

        // Add new word if not found
        if (!found) {
            if (word_freq->size >= word_freq->capacity) {
                resize_word_freq_array(word_freq);
            }

            word_freq->data[word_freq->size].word = strdup(words[i]);
            word_freq->data[word_freq->size].frequency = 1;
            word_freq->size++;
        }
    }

    return 1;
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
    char filename[] = "text8.txt";  //name of cleaned dataset in my laptop
    int total_words = 0;
    clock_t start, end;
    double execution_time;

    // Start timing execution
    start = clock();

    // Read words from file
    char **words = read_words_from_file(filename, &total_words);
    if (!words) {
        fprintf(stderr, "Failed to read words from file\n");
        return 1;
    }

    // Create word frequency array
    WordFreqArray *word_freq = create_word_freq_array();

    // Count word frequencies
    if (!count_word_frequencies(words, total_words, word_freq)) {
        fprintf(stderr, "Failed to count word frequencies\n");
        for (int i = 0; i < total_words; i++) {
            free(words[i]);
        }
        free(words);
        free_word_freq_array(word_freq);
        return 1;
    }

    // Sort words by frequency
    merge_sort(word_freq->data, 0, word_freq->size - 1);

    // End timing execution
    end = clock();
    execution_time = ((double) (end - start)) / CLOCKS_PER_SEC;

    // Print top frequent words
    printf("Top 10 Most Frequent Words:\n");
    int print_limit = (TOP_K < word_freq->size) ? TOP_K : word_freq->size;
    for (int i = 0; i < print_limit; i++) {
        printf("%s: %d\n", word_freq->data[i].word, word_freq->data[i].frequency);
    }

    // Print statistics
    printf("\nTotal Words: %d\n", total_words);
    printf("Execution Time: %.4f seconds\n", execution_time);

    // Free resources
    for (int i = 0; i < total_words; i++) {
        free(words[i]);
    }
    free(words);
    free_word_freq_array(word_freq);

    return 0;
}